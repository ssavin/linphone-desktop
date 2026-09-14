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

#include "KiwiCallLoginCore.hpp"
#include "core/App.hpp"
#include "tool/Utils.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

DEFINE_ABSTRACT_OBJECT(KiwiCallLoginCore)

namespace {
constexpr char kKiwiCallLoginUrl[] = "https://kiwicall.ru/api/mobile/login";
} // namespace

QSharedPointer<KiwiCallLoginCore> KiwiCallLoginCore::create() {
	auto model = QSharedPointer<KiwiCallLoginCore>(new KiwiCallLoginCore(), &QObject::deleteLater);
	model->moveToThread(App::getInstance()->thread());
	return model;
}

KiwiCallLoginCore::KiwiCallLoginCore(QObject *parent) : QObject(parent) {
	mNetworkManager = new QNetworkAccessManager(this);
}

KiwiCallLoginCore::~KiwiCallLoginCore() {
}

void KiwiCallLoginCore::login(const QString &email, const QString &password) {
	emit loginStarted();

	QUrl url(QString::fromLatin1(kKiwiCallLoginUrl));
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

	QJsonObject body;
	body["email"] = email;
	body["password"] = password;

	auto *reply = mNetworkManager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		// Qt treats any non-2xx HTTP status (401 wrong password, 404 no SIP
		// account, ...) as a QNetworkReply error, but our API still sends a
		// JSON body with a human-readable "error" field in those cases - so
		// try to parse the body regardless of reply->error() first, and only
		// fall back to a generic message when there is genuinely no body to
		// read (DNS failure, connection refused, timeout, ...).
		QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
		if (obj.contains(QStringLiteral("error"))) {
			emit loginFailed(obj.value(QStringLiteral("error")).toString());
			return;
		}
		if (reply->error() != QNetworkReply::NoError) {
			qWarning() << "[KiwiCallLoginCore] network error:" << reply->error() << reply->errorString();
			emit loginFailed(tr("kiwicall_login_network_error"));
			return;
		}
		if (obj.value(QStringLiteral("requiresClinicChoice")).toBool()) {
			emit loginFailed(tr("kiwicall_login_multiple_clinics_error"));
			return;
		}
		QString provisioningUrl = obj.value(QStringLiteral("provisioningUrl")).toString();
		if (provisioningUrl.isEmpty()) {
			emit loginFailed(tr("kiwicall_login_failed_error"));
			return;
		}
		emit loginSucceeded();
		Utils::useFetchConfig(provisioningUrl);
	});
}
