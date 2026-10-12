/*
 * KiwiCall: mute every other application's playback while a call is in progress.
 */

#include "AudioDucker.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>

#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {
// Calls fn(volume, sessionId, isExpired) for every playback session of the other processes.
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
					fn(volume.Get(), id, state == AudioSessionStateExpired);
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
	connect(qApp, &QCoreApplication::aboutToQuit, this, &AudioDucker::release);
}

void AudioDucker::engage() {
#ifdef Q_OS_WIN
	if (mEngaged) return;
	mEngaged = true;
	scan();
	mTimer.start();
#endif
}

void AudioDucker::scan() {
#ifdef Q_OS_WIN
	bool changed = false;
	forEachForeignSession([&](ISimpleAudioVolume *volume, const QString &id, bool expired) {
		if (expired || mMuted.contains(id)) return;
		BOOL muted = FALSE;
		if (FAILED(volume->GetMute(&muted)) || muted) return; // Already muted by the user: leave it alone.
		if (SUCCEEDED(volume->SetMute(TRUE, nullptr))) {
			mMuted.insert(id);
			changed = true;
		}
	});
	if (changed) saveState();
#endif
}

void AudioDucker::release() {
#ifdef Q_OS_WIN
	mTimer.stop();
	mEngaged = false;
	if (mMuted.isEmpty()) return;
	forEachForeignSession([&](ISimpleAudioVolume *volume, const QString &id, bool) {
		if (mMuted.contains(id)) volume->SetMute(FALSE, nullptr);
	});
	mMuted.clear();
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
			if (!line.isEmpty()) mMuted.insert(line);
		}
		file.close();
	}
	release();
#endif
}

void AudioDucker::saveState() const {
	QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/muted-sessions.txt";
	if (mMuted.isEmpty()) {
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
