/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
 *
 * This file is part of linphone-desktop
 * (see https://www.linphone.org).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DESKTOP_TOOLS_WINDOWS_H_
#define DESKTOP_TOOLS_WINDOWS_H_

#include <QImage>
#include <QObject>
#include <QVariantMap>
#include <windows.h>

class VideoSourceDescriptorModel;
// =============================================================================

class DesktopTools : public QObject {
	Q_OBJECT

	Q_PROPERTY(
	    bool screenSaverStatus READ getScreenSaverStatus WRITE setScreenSaverStatus NOTIFY screenSaverStatusChanged)

public:
	DesktopTools(QObject *parent = Q_NULLPTR);
	~DesktopTools();

	bool getScreenSaverStatus() const;
	void setScreenSaverStatus(bool status);

	static void init() {
	}
	static void applicationStateChanged(Qt::ApplicationState){};

	static QList<QVariantMap> getWindows();
	static QImage takeScreenshot(void *window);
	static QImage getWindowIcon(void *window);

	static void *getDisplay(uintptr_t screenIndex) {
		return reinterpret_cast<void *>(screenIndex);
	}
	static uintptr_t getDisplayIndex(void *screenSharing);
	static QRect getWindowGeometry(void *screenSharing);
	HWND mWindowId = 0; // Window
	VideoSourceDescriptorModel *mVideoSourceDescriptorModel = nullptr;

	// Windows ducks (lowers/mutes) every other app's audio while our WASAPI
	// stream is tagged AudioCategory_Communications (see external/linphone-sdk/
	// mswasapi) - expected OS behavior for VoIP apps, but surprising to users
	// who don't know this Windows setting exists. Offer to switch it to "no
	// action" once, like the Android battery-optimization-exemption dialog.
	Q_INVOKABLE bool isAudioDuckingEnabled() const;
	Q_INVOKABLE bool disableAudioDucking();
	Q_INVOKABLE bool shouldOfferAudioDuckingFix() const;
	Q_INVOKABLE void setAudioDuckingFixDismissed();

signals:
	void screenSaverStatusChanged(bool status);
	void windowIdSelectionStarted();
	void windowIdSelectionEnded();

private:
	bool mScreenSaverStatus = true;
};

#endif // DESKTOP_TOOLS_WINDOWS_H_
