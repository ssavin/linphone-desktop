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

#ifndef CRM_CARD_CORE_H_
#define CRM_CARD_CORE_H_

#include "model/core/CoreModel.hpp"
#include "tool/AbstractObject.hpp"
#include "tool/thread/SafeConnection.hpp"
#include <QNetworkAccessManager>
#include <QObject>
#include <QSharedPointer>
#include <QVariantList>
#include <functional>

// "Шторка звонка": looks up the caller's KiwiCall CRM card (name, recent
// calls, upcoming appointments) by phone number on an incoming call, so
// the operator sees who's calling instead of a bare number - see the
// Mango-Talker-parity plan. One instance per CallCore (not a QML
// singleton, unlike OnlineStatusCore, since a lookup is scoped to a
// single call), but reuses the exact same authentication mechanism:
// Basic Auth built from the default account's own SIP credentials
// (Account::findAuthInfo()), since desktop has no browser session to
// authenticate with either.
class CrmCardCore : public QObject, public AbstractObject {
	Q_OBJECT
	Q_PROPERTY(bool loading READ getLoading NOTIFY loadingChanged)
	Q_PROPERTY(bool found READ getFound NOTIFY foundChanged)
	Q_PROPERTY(QString clientName READ getClientName NOTIFY foundChanged)
	Q_PROPERTY(QString clientComment READ getClientComment NOTIFY foundChanged)
	Q_PROPERTY(QVariantList tags READ getTags NOTIFY foundChanged)
	Q_PROPERTY(int visitsCount READ getVisitsCount NOTIFY foundChanged)
	Q_PROPERTY(QVariantList recentCalls READ getRecentCalls NOTIFY foundChanged)
	Q_PROPERTY(QVariantList appointments READ getAppointments NOTIFY foundChanged)

public:
	static QSharedPointer<CrmCardCore> create();
	CrmCardCore(QObject *parent = nullptr);
	~CrmCardCore();

	void setSelf(QSharedPointer<CrmCardCore> me);

	bool getLoading() const {
		return mLoading;
	}
	bool getFound() const {
		return mFound;
	}
	QString getClientName() const {
		return mClientName;
	}
	QString getClientComment() const {
		return mClientComment;
	}
	QVariantList getTags() const {
		return mTags;
	}
	int getVisitsCount() const {
		return mVisitsCount;
	}
	QVariantList getRecentCalls() const {
		return mRecentCalls;
	}
	QVariantList getAppointments() const {
		return mAppointments;
	}

	// Looked up once per incoming call - the raw SIP remote address/number
	// as seen by liblinphone, not yet normalized (the backend does that).
	Q_INVOKABLE void lookup(const QString &phoneNumber);

signals:
	void loadingChanged();
	void foundChanged();
	void lookupFailed(const QString &message);

private:
	void withCredentials(std::function<void(const QString &username, const QString &password)> callback);
	void setLoading(bool value);
	void applyResult(const QJsonObject &obj);

	bool mLoading = false;
	bool mFound = false;
	QString mClientName;
	QString mClientComment;
	QVariantList mTags;
	int mVisitsCount = 0;
	QVariantList mRecentCalls;
	QVariantList mAppointments;
	QNetworkAccessManager *mNetworkManager = nullptr;
	QSharedPointer<SafeConnection<CrmCardCore, CoreModel>> mCoreModelConnection;

	DECLARE_ABSTRACT_OBJECT
};
Q_DECLARE_METATYPE(QSharedPointer<CrmCardCore>);

#endif // CRM_CARD_CORE_H_
