/*
 * KiwiCall: mute every other application's playback while a call is in progress.
 */

#include "AudioDucker.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>

#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {
// A session instance id looks like "<device>|<app>%b{guid}|<n>%b<pid>"; the app part identifies the program.
// Windows remembers the mute state per program, so that is what has to be tracked (and restored).
QString appIdOf(const QString &instanceId) {
	static const QRegularExpression instanceSuffix(QStringLiteral("\\|\\d+%b(\\d+|#)$"));
	QString id = instanceId;
	id.remove(instanceSuffix);
	return id;
}

// Calls fn(volume, appId, isExpired) for every playback session of the other processes.
template <typename Fn>
void forEachForeignSession(Fn fn) {
	HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	ComPtr<IMMDeviceEnumerator> enumerator;
	if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) {
		ComPtr<IMMDeviceCollection> devices;
		UINT deviceCount = 0;
		if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices)) &&
		    SUCCEEDED(devices->GetCount(&deviceCount))) {
			const DWORD ownPid = GetCurrentProcessId();
			for (UINT i = 0; i < deviceCount; ++i) {
				ComPtr<IMMDevice> device;
				ComPtr<IAudioSessionManager2> manager;
				ComPtr<IAudioSessionEnumerator> sessions;
				int sessionCount = 0;
				if (FAILED(devices->Item(i, &device)) ||
				    FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
				                            reinterpret_cast<void **>(manager.GetAddressOf()))) ||
				    FAILED(manager->GetSessionEnumerator(&sessions)) || FAILED(sessions->GetCount(&sessionCount)))
					continue;
				for (int j = 0; j < sessionCount; ++j) {
					ComPtr<IAudioSessionControl> control;
					ComPtr<IAudioSessionControl2> control2;
					ComPtr<ISimpleAudioVolume> volume;
					if (FAILED(sessions->GetSession(j, &control)) || FAILED(control.As(&control2)) ||
					    FAILED(control.As(&volume)))
						continue;
					DWORD pid = 0;
					control2->GetProcessId(&pid);
					if (pid == ownPid) continue;
					AudioSessionState state = AudioSessionStateInactive;
					control->GetState(&state);
					LPWSTR rawId = nullptr;
					if (FAILED(control2->GetSessionInstanceIdentifier(&rawId)) || !rawId) continue;
					QString id = QString::fromWCharArray(rawId);
					CoTaskMemFree(rawId);
					if (id.contains(QStringLiteral("audiodg.exe"), Qt::CaseInsensitive))
						continue; // Not an application.
					fn(volume.Get(), appIdOf(id), state == AudioSessionStateExpired);
				}
			}
		}
	}
	if (SUCCEEDED(init)) CoUninitialize();
}
} // namespace
#endif

AudioDucker *AudioDucker::instance() {
	static AudioDucker *sInstance = new AudioDucker(qApp);
	return sInstance;
}

AudioDucker::AudioDucker(QObject *parent) : QObject(parent) {
	mTimer.setInterval(2000);
	connect(&mTimer, &QTimer::timeout, this, &AudioDucker::scan);
	mPendingTimer.setInterval(3000);
	connect(&mPendingTimer, &QTimer::timeout, this, &AudioDucker::scanPending);
	connect(qApp, &QCoreApplication::aboutToQuit, this, &AudioDucker::release);
}

void AudioDucker::engage() {
#ifdef Q_OS_WIN
	if (mEngaged) return;
	mEngaged = true;
	mPendingTimer.stop();
	scan();
	mTimer.start();
#endif
}

void AudioDucker::scan() {
#ifdef Q_OS_WIN
	bool changed = false;
	forEachForeignSession([&](ISimpleAudioVolume *volume, const QString &appId, bool expired) {
		if (expired) return;
		BOOL muted = FALSE;
		if (FAILED(volume->GetMute(&muted))) return;
		if (mMuted.contains(appId)) { // Our program: a new session of it may have inherited or lost the mute.
			if (!muted) volume->SetMute(TRUE, nullptr);
			return;
		}
		if (muted) return; // Already muted by the user: leave it alone.
		if (SUCCEEDED(volume->SetMute(TRUE, nullptr))) {
			mMuted.insert(appId);
			changed = true;
			qInfo() << "[AudioDucker] muted" << appId;
		}
	});
	if (changed) saveState();
#endif
}

void AudioDucker::release() {
#ifdef Q_OS_WIN
	mTimer.stop();
	mEngaged = false;
	if (mMuted.isEmpty() && mPending.isEmpty()) return;
	// Windows remembers the mute of a program and applies it to the sessions it opens later (a browser that was idle
	// when the call ended, for instance). Keep watching these programs until they are seen un-muted.
	mPending.unite(mMuted);
	mMuted.clear();
	saveState();
	scanPending();
	if (!mPending.isEmpty()) mPendingTimer.start();
#endif
}

void AudioDucker::scanPending() {
#ifdef Q_OS_WIN
	if (mEngaged) return;
	QSet<QString> done;
	forEachForeignSession([&](ISimpleAudioVolume *volume, const QString &appId, bool expired) {
		if (expired || !mPending.contains(appId)) return;
		BOOL muted = FALSE;
		if (FAILED(volume->GetMute(&muted))) return;
		if (muted) {
			volume->SetMute(FALSE, nullptr);
			qInfo() << "[AudioDucker] unmuted" << appId;
		} else done.insert(appId); // Seen sound-enabled after the call: nothing left to restore for it.
	});
	if (done.isEmpty()) return;
	mPending.subtract(done);
	if (mPending.isEmpty()) mPendingTimer.stop();
	saveState();
#endif
}

void AudioDucker::restoreLeftovers() {
#ifdef Q_OS_WIN
	QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/muted-sessions.txt";
	QFile file(path);
	if (!file.exists()) return;
	if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QTextStream in(&file);
		while (!in.atEnd()) {
			QString line = in.readLine().trimmed();
			if (!line.isEmpty()) mPending.insert(appIdOf(line)); // Older files hold instance ids.
		}
		file.close();
	}
	scanPending();
	if (!mPending.isEmpty()) mPendingTimer.start();
#endif
}

void AudioDucker::saveState() const {
	QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/muted-sessions.txt";
	if (mMuted.isEmpty() && mPending.isEmpty()) {
		QFile::remove(path);
		return;
	}
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return;
	QTextStream out(&file);
	for (const QString &id : mMuted)
		out << id << '\n';
}
