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

#include "ContactImportCore.hpp"
#include "core/App.hpp"
#include "model/tool/ToolModel.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextStream>
#include <QUrlQuery>

DEFINE_ABSTRACT_OBJECT(ContactImportCore)

namespace {
constexpr char kKiwiCallApiBase[] = "https://kiwicall.ru/api";
} // namespace

QSharedPointer<ContactImportCore> ContactImportCore::create() {
	auto model = QSharedPointer<ContactImportCore>(new ContactImportCore(), &QObject::deleteLater);
	model->moveToThread(App::getInstance()->thread());
	model->setSelf(model);
	return model;
}

ContactImportCore::ContactImportCore(QObject *parent) : QObject(parent) {
	mNetworkManager = new QNetworkAccessManager(this);
}

ContactImportCore::~ContactImportCore() {
}

void ContactImportCore::setSelf(QSharedPointer<ContactImportCore> me) {
	if (mCoreModelConnection) mCoreModelConnection->disconnect();
	mCoreModelConnection = SafeConnection<ContactImportCore, CoreModel>::create(me, CoreModel::getInstance());
}

// ---------------------------------------------------------------------------
// CSV import
// ---------------------------------------------------------------------------

// Expected format, one contact per line: name,phone[,extra phone...]
// A header line (first cell looking like "name"/"имя") is skipped if present.
void ContactImportCore::importFromCsv(const QString &filePath) {
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		emit importError(tr("contact_import_file_open_error"));
		return;
	}
	emit importStarted();

	QList<ImportedContactEntry> contacts;
	QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	stream.setCodec("UTF-8");
#endif
	bool firstLine = true;
	while (!stream.atEnd()) {
		QString line = stream.readLine();
		if (line.trimmed().isEmpty()) continue;
		QStringList cells = line.split(',');
		for (auto &cell : cells) {
			cell = cell.trimmed();
			if (cell.startsWith('"') && cell.endsWith('"') && cell.length() >= 2) cell = cell.mid(1, cell.length() - 2);
		}
		if (firstLine) {
			firstLine = false;
			QString firstCellLower = cells.value(0).toLower();
			if (firstCellLower.contains(QStringLiteral("name")) || firstCellLower.contains(QStringLiteral("имя"))) {
				continue; // header row
			}
		}
		if (cells.isEmpty() || cells.first().isEmpty()) continue;
		ImportedContactEntry entry;
		entry.name = cells.value(0);
		for (int i = 1; i < cells.size(); ++i) {
			QString phone = cells.value(i);
			if (!phone.isEmpty()) entry.phones << phone;
		}
		if (!entry.phones.isEmpty()) contacts << entry;
	}
	file.close();

	if (contacts.isEmpty()) {
		emit importError(tr("contact_import_file_empty_error"));
		return;
	}
	createFriends(contacts);
}

// ---------------------------------------------------------------------------
// KiwiCall CRM import
// ---------------------------------------------------------------------------

void ContactImportCore::importFromKiwiCall(const QString &email, const QString &password) {
	emit importStarted();

	QUrl url(QString::fromLatin1(kKiwiCallApiBase) + QStringLiteral("/auth/login"));
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

	QJsonObject body;
	body["email"] = email;
	body["password"] = password;
	body["product"] = QStringLiteral("kiwicall");

	auto *reply = mNetworkManager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			emit importError(tr("contact_import_network_error").arg(reply->errorString()));
			return;
		}
		QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
		QJsonObject obj = doc.object();
		if (obj.contains(QStringLiteral("error"))) {
			emit importError(obj.value(QStringLiteral("error")).toString());
			return;
		}
		if (obj.value(QStringLiteral("requiresClinicChoice")).toBool()) {
			emit importError(tr("contact_import_multiple_clinics_error"));
			return;
		}

		QString sessionCookie;
		for (const auto &header : reply->rawHeaderPairs()) {
			if (QString::fromLatin1(header.first).compare(QStringLiteral("Set-Cookie"), Qt::CaseInsensitive) == 0) {
				QString value = QString::fromUtf8(header.second);
				int semi = value.indexOf(';');
				QString cookiePair = semi >= 0 ? value.left(semi) : value;
				if (cookiePair.startsWith(QStringLiteral("connect.sid="))) {
					sessionCookie = cookiePair;
					break;
				}
			}
		}
		if (sessionCookie.isEmpty()) {
			emit importError(tr("contact_import_login_failed_error"));
			return;
		}

		auto accumulator = QSharedPointer<QList<ImportedContactEntry>>::create();
		fetchClientsPage(sessionCookie, 1, accumulator);
	});
}

void ContactImportCore::fetchClientsPage(const QString &sessionCookie,
                                         int page,
                                         QSharedPointer<QList<ImportedContactEntry>> accumulator) {
	QUrl url(QString::fromLatin1(kKiwiCallApiBase) + QStringLiteral("/clients"));
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("page"), QString::number(page));
	query.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("100"));
	url.setQuery(query);

	QNetworkRequest request(url);
	request.setRawHeader("Cookie", sessionCookie.toUtf8());

	auto *reply = mNetworkManager->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply, sessionCookie, page, accumulator]() {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			emit importError(tr("contact_import_network_error").arg(reply->errorString()));
			return;
		}
		QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
		QJsonObject obj = doc.object();
		QJsonArray rows = obj.value(QStringLiteral("rows")).toArray();
		int total = obj.value(QStringLiteral("total")).toInt();

		for (const auto &rowValue : rows) {
			QJsonObject row = rowValue.toObject();
			ImportedContactEntry entry;
			entry.name = row.value(QStringLiteral("name")).toString();
			QString phone = row.value(QStringLiteral("phone")).toString();
			if (!phone.isEmpty()) entry.phones << phone;
			for (const auto &extra : row.value(QStringLiteral("extraPhones")).toArray()) {
				QString extraPhone = extra.toString();
				if (!extraPhone.isEmpty()) entry.phones << extraPhone;
			}
			if (entry.name.isEmpty()) entry.name = entry.phones.value(0);
			if (!entry.phones.isEmpty()) *accumulator << entry;
		}

		if (!rows.isEmpty() && accumulator->size() < total) {
			fetchClientsPage(sessionCookie, page + 1, accumulator);
		} else if (accumulator->isEmpty()) {
			emit importError(tr("contact_import_no_clients_error"));
		} else {
			createFriends(*accumulator);
		}
	});
}

// ---------------------------------------------------------------------------
// Friend creation (must run on the linphone thread)
// ---------------------------------------------------------------------------

void ContactImportCore::createFriends(const QList<ImportedContactEntry> &contacts) {
	mCoreModelConnection->invokeToModel([this, contacts]() {
		auto core = CoreModel::getInstance()->getCore();
		auto friendList = ToolModel::getAppFriendList();
		int successCount = 0;
		int errorCount = 0;
		for (const auto &entry : contacts) {
			auto contact = core->createFriend();
			if (!entry.name.isEmpty()) contact->setName(entry.name.toStdString());
			for (const auto &phone : entry.phones)
				contact->addPhoneNumber(phone.toStdString());
			bool created = friendList->addFriend(contact) == linphone::FriendList::Status::OK;
			if (created) ++successCount;
			else ++errorCount;
		}
		friendList->updateSubscriptions();
		mCoreModelConnection->invokeToCore(
		    [this, successCount, errorCount]() { emit importFinished(successCount, errorCount); });
	});
}
