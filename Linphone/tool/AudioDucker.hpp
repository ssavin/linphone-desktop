/*
 * KiwiCall: mute every other application's playback while a call is in progress.
 *
 * Windows only (Core Audio sessions); a no-op on the other platforms.
 * Must be used from the main thread.
 */

#ifndef AUDIO_DUCKER_H_
#define AUDIO_DUCKER_H_

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

class AudioDucker : public QObject {
	Q_OBJECT
public:
	static AudioDucker *instance();

	// Mute all other playback sessions (idempotent). New sessions started during the call are caught by a timer.
	void engage();
	// Restore everything that this class muted (also keeps watching programs whose session was gone at that moment).
	void release();
	// Un-mute programs left muted by a previous run (crash, or a session that did not exist when the call ended).
	void restoreLeftovers();

private:
	AudioDucker(QObject *parent = nullptr);
	void scan();
	void scanPending();
	void saveState() const;

	QSet<QString> mMuted;   // Programs muted by the current call.
	QSet<QString> mPending; // Programs to un-mute as soon as a muted session of theirs shows up.
	QTimer mTimer;
	QTimer mPendingTimer;
	bool mEngaged = false;
};

#endif
