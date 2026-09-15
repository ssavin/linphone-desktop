/*
 * Copyright (c) 2026 KiwiCall.
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

#ifndef ONLINE_STATUS_CORE_H_
#define ONLINE_STATUS_CORE_H_

#include "model/core/CoreModel.hpp"
#include "tool/AbstractObject.hpp"
#include "tool/thread/SafeConnection.hpp"
#include <QNetworkAccessManager>
#include <QObject>
#include <QSharedPointer>
#include <functional>

// Self-service "on line / not on line" toggle for the KiwiCall operator
// queue (mirrors the website's pause/unpause switch). Desktop has no
// browser session to authenticate with, so this authenticates the same way
// the SIP registration itself does: extension + password, read back from
// the default account's own AuthInfo (Account::findAuthInfo()) rather than
// stored separately.
class OnlineStatusCore : public QObject, public AbstractObject {
	Q_OBJECT
	Q_PROPERTY(bool online READ getOnline NOTIFY onlineChanged)
	Q_PROPERTY(bool available READ getAvailable NOTIFY availableChanged)

public:
	static QSharedPointer<OnlineStatusCore> create();
	OnlineStatusCore(QObject *parent = nullptr);
	~OnlineStatusCore();

	void setSelf(QSharedPointer<OnlineStatusCore> me);

	bool getOnline() const {
		return mOnline;
	}
	bool getAvailable() const {
		return mAvailable;
	}

	Q_INVOKABLE void refresh();
	Q_INVOKABLE void toggle();

signals:
	void onlineChanged();
	void availableChanged();
	void toggleFailed(const QString &message);

private:
	void withCredentials(std::function<void(const QString &username, const QString &password)> callback);
	void setOnline(bool value);
	void setAvailable(bool value);

	bool mOnline = false;
	bool mAvailable = false;
	QNetworkAccessManager *mNetworkManager = nullptr;
	QSharedPointer<SafeConnection<OnlineStatusCore, CoreModel>> mCoreModelConnection;

	DECLARE_ABSTRACT_OBJECT
};
Q_DECLARE_METATYPE(QSharedPointer<OnlineStatusCore>);

#endif // ONLINE_STATUS_CORE_H_
