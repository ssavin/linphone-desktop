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

#include "OnlineStatusCore.hpp"
#include "core/App.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

DEFINE_ABSTRACT_OBJECT(OnlineStatusCore)

namespace {
constexpr char kSelfStatusUrl[] = "https://kiwicall.ru/api/mobile/self-status";

QByteArray basicAuthHeader(const QString &username, const QString &password) {
	return "Basic " + (username + ":" + password).toUtf8().toBase64();
}
} // namespace

QSharedPointer<OnlineStatusCore> OnlineStatusCore::create() {
	auto model = QSharedPointer<OnlineStatusCore>(new OnlineStatusCore(), &QObject::deleteLater);
	model->moveToThread(App::getInstance()->thread());
	model->setSelf(model);
	return model;
}

OnlineStatusCore::OnlineStatusCore(QObject *parent) : QObject(parent) {
	mNetworkManager = new QNetworkAccessManager(this);
}

OnlineStatusCore::~OnlineStatusCore() {
}

void OnlineStatusCore::setSelf(QSharedPointer<OnlineStatusCore> me) {
	if (mCoreModelConnection) mCoreModelConnection->disconnect();
	mCoreModelConnection = SafeConnection<OnlineStatusCore, CoreModel>::create(me, CoreModel::getInstance());
}

void OnlineStatusCore::setOnline(bool value) {
	if (mOnline != value) {
		mOnline = value;
		emit onlineChanged();
	}
}

void OnlineStatusCore::setAvailable(bool value) {
	if (mAvailable != value) {
		mAvailable = value;
		emit availableChanged();
	}
}

void OnlineStatusCore::setBusy(bool value) {
	if (mBusy != value) {
		mBusy = value;
		emit busyChanged();
	}
}

// Reads the default account's own SIP credentials (the same ones it
// registers with) via Account::findAuthInfo() on the core thread, then
// hands them back here on the App thread for the HTTP call.
void OnlineStatusCore::withCredentials(std::function<void(const QString &username, const QString &password)> callback) {
	mCoreModelConnection->invokeToModel([this, callback]() {
		QString username;
		QString password;
		auto core = CoreModel::getInstance()->getCore();
		auto account = core ? core->getDefaultAccount() : nullptr;
		if (account) {
			auto authInfo = account->findAuthInfo();
			if (authInfo) {
				username = QString::fromStdString(authInfo->getUsername());
				// Right after a successful SIP REGISTER, the SDK typically
				// replaces the plaintext password in AuthInfo with its HA1
				// digest and clears the original (confirmed live) - fall
				// back to HA1 (which the server also accepts, see
				// sip-provisioning.ts) whenever plaintext isn't available.
				password = QString::fromStdString(authInfo->getPassword());
				if (password.isEmpty()) password = QString::fromStdString(authInfo->getHa1());
			}
		}
		mCoreModelConnection->invokeToCore([callback, username, password]() { callback(username, password); });
	});
}

void OnlineStatusCore::refresh() {
	withCredentials([this](const QString &username, const QString &password) {
		if (username.isEmpty() || password.isEmpty()) {
			setAvailable(false);
			return;
		}

		QNetworkRequest request((QUrl(QString::fromLatin1(kSelfStatusUrl))));
		request.setRawHeader("Authorization", basicAuthHeader(username, password));

		auto *reply = mNetworkManager->get(request);
		connect(reply, &QNetworkReply::finished, this, [this, reply]() {
			reply->deleteLater();
			QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
			if (!obj.contains(QStringLiteral("online"))) {
				// Not a KiwiCall operator extension (or a network error) -
				// hide the toggle rather than show an error, it just
				// doesn't apply to this account.
				setAvailable(false);
				return;
			}
			setOnline(obj.value(QStringLiteral("online")).toBool());
			setAvailable(true);
		});
	});
}

void OnlineStatusCore::toggle() {
	if (mBusy) return;
	const bool newValue = !mOnline;
	setBusy(true);
	withCredentials([this, newValue](const QString &username, const QString &password) {
		if (username.isEmpty() || password.isEmpty()) {
			setAvailable(false);
			setBusy(false);
			return;
		}

		QNetworkRequest request((QUrl(QString::fromLatin1(kSelfStatusUrl))));
		request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
		request.setRawHeader("Authorization", basicAuthHeader(username, password));

		QJsonObject body;
		body["online"] = newValue;

		auto *reply = mNetworkManager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
		connect(reply, &QNetworkReply::finished, this, [this, reply]() {
			reply->deleteLater();
			QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
			setBusy(false);
			if (obj.contains(QStringLiteral("error"))) {
				emit toggleFailed(obj.value(QStringLiteral("error")).toString());
				return;
			}
			if (obj.contains(QStringLiteral("online"))) {
				setOnline(obj.value(QStringLiteral("online")).toBool());
			} else if (reply->error() != QNetworkReply::NoError) {
				emit toggleFailed(tr("online_status_network_error"));
			}
		});
	});
}
