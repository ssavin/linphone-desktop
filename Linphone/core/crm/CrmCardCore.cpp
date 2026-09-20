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

#include "CrmCardCore.hpp"
#include "core/App.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

DEFINE_ABSTRACT_OBJECT(CrmCardCore)

namespace {
constexpr char kLookupUrl[] = "https://kiwicall.ru/api/mobile/clients/lookup";

QByteArray basicAuthHeader(const QString &username, const QString &password) {
	return "Basic " + (username + ":" + password).toUtf8().toBase64();
}

QVariantList jsonArrayToVariantList(const QJsonArray &array) {
	QVariantList list;
	for (const auto &value : array)
		list << value.toObject().toVariantMap();
	return list;
}
} // namespace

QSharedPointer<CrmCardCore> CrmCardCore::create() {
	auto model = QSharedPointer<CrmCardCore>(new CrmCardCore(), &QObject::deleteLater);
	model->moveToThread(App::getInstance()->thread());
	model->setSelf(model);
	return model;
}

CrmCardCore::CrmCardCore(QObject *parent) : QObject(parent) {
	mNetworkManager = new QNetworkAccessManager(this);
}

CrmCardCore::~CrmCardCore() {
}

void CrmCardCore::setSelf(QSharedPointer<CrmCardCore> me) {
	if (mCoreModelConnection) mCoreModelConnection->disconnect();
	mCoreModelConnection = SafeConnection<CrmCardCore, CoreModel>::create(me, CoreModel::getInstance());
}

void CrmCardCore::setLoading(bool value) {
	if (mLoading != value) {
		mLoading = value;
		emit loadingChanged();
	}
}

// Same credential source as OnlineStatusCore::withCredentials() - the
// default account's own SIP AuthInfo, falling back to HA1 once the SDK
// has cleared the plaintext password after a successful REGISTER.
void CrmCardCore::withCredentials(std::function<void(const QString &username, const QString &password)> callback) {
	mCoreModelConnection->invokeToModel([this, callback]() {
		QString username;
		QString password;
		auto core = CoreModel::getInstance()->getCore();
		auto account = core ? core->getDefaultAccount() : nullptr;
		if (account) {
			auto authInfo = account->findAuthInfo();
			if (authInfo) {
				username = QString::fromStdString(authInfo->getUsername());
				password = QString::fromStdString(authInfo->getPassword());
				if (password.isEmpty()) password = QString::fromStdString(authInfo->getHa1());
			}
		}
		mCoreModelConnection->invokeToCore([callback, username, password]() { callback(username, password); });
	});
}

void CrmCardCore::applyResult(const QJsonObject &obj) {
	mFound = obj.value(QStringLiteral("found")).toBool();
	if (mFound) {
		QJsonObject client = obj.value(QStringLiteral("client")).toObject();
		mClientName = client.value(QStringLiteral("name")).toString();
		mClientComment = client.value(QStringLiteral("comment")).toString();
		mTags = jsonArrayToVariantList(client.value(QStringLiteral("tags")).toArray());
		mVisitsCount = client.value(QStringLiteral("visitsCount")).toInt();
		mRecentCalls = jsonArrayToVariantList(obj.value(QStringLiteral("recentCalls")).toArray());
		mAppointments = jsonArrayToVariantList(obj.value(QStringLiteral("appointments")).toArray());
	} else {
		mClientName.clear();
		mClientComment.clear();
		mTags.clear();
		mVisitsCount = 0;
		mRecentCalls.clear();
		mAppointments.clear();
	}
	emit foundChanged();
}

void CrmCardCore::lookup(const QString &phoneNumber) {
	setLoading(true);
	withCredentials([this, phoneNumber](const QString &username, const QString &password) {
		if (username.isEmpty() || password.isEmpty()) {
			setLoading(false);
			return;
		}

		QUrl url(QString::fromLatin1(kLookupUrl));
		QUrlQuery query;
		query.addQueryItem(QStringLiteral("phone"), phoneNumber);
		url.setQuery(query);

		QNetworkRequest request(url);
		request.setRawHeader("Authorization", basicAuthHeader(username, password));

		auto *reply = mNetworkManager->get(request);
		connect(reply, &QNetworkReply::finished, this, [this, reply]() {
			reply->deleteLater();
			setLoading(false);
			QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
			if (obj.contains(QStringLiteral("error"))) {
				emit lookupFailed(obj.value(QStringLiteral("error")).toString());
				return;
			}
			applyResult(obj);
		});
	});
}
