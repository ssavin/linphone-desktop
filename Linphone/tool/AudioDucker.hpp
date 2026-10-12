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
	// Restore everything that this class muted.
	void release();
	// Un-mute sessions left muted by a previous run that crashed during a call.
	void restoreLeftovers();

private:
	AudioDucker(QObject *parent = nullptr);
	void scan();
	void saveState() const;

	QSet<QString> mMuted;
	QTimer mTimer;
	bool mEngaged = false;
};

#endif
