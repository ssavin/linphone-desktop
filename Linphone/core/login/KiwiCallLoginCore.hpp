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

#ifndef KIWICALL_LOGIN_CORE_H_
#define KIWICALL_LOGIN_CORE_H_

#include "tool/AbstractObject.hpp"
#include <QNetworkAccessManager>
#include <QObject>
#include <QSharedPointer>
#include <QString>

// Simple KiwiCall email/password login for the desktop assistant: POSTs
// credentials to kiwicall.ru and, on success, applies the returned Linphone
// remote-provisioning URL the same way the "Configuration distante" dialog
// does (Utils::useFetchConfig) - just fed a server-issued URL instead of one
// typed by hand. Mirrors linphone-android's KiwiCallLoginViewModel.
class KiwiCallLoginCore : public QObject, public AbstractObject {
	Q_OBJECT

public:
	static QSharedPointer<KiwiCallLoginCore> create();
	KiwiCallLoginCore(QObject *parent = nullptr);
	~KiwiCallLoginCore();

	Q_INVOKABLE void login(const QString &email, const QString &password);

signals:
	void loginStarted();
	void loginFailed(const QString &message);
	void loginSucceeded();

private:
	QNetworkAccessManager *mNetworkManager = nullptr;

	DECLARE_ABSTRACT_OBJECT
};
Q_DECLARE_METATYPE(QSharedPointer<KiwiCallLoginCore>);

#endif // KIWICALL_LOGIN_CORE_H_
