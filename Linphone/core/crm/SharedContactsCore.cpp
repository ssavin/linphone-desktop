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

#include "SharedContactsCore.hpp"
#include "core/App.hpp"
#include "model/tool/ToolModel.hpp"
#include "tool/Utils.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

DEFINE_ABSTRACT_OBJECT(SharedContactsCore)

namespace {
constexpr char kContactsUrl[] = "https://kiwicall.ru/api/mobile/contacts";
constexpr char kSuggestUrl[] = "https://kiwicall.ru/api/mobile/contacts/suggest";
constexpr char kClientNameUrl[] = "https://kiwicall.ru/api/mobile/contacts/clients/%1/name";
constexpr char kSharedListName[] = "kiwicall_shared";
constexpr char kRefKeyPrefix[] = "kiwicall:";
constexpr int kRetryIntervalMs = 30 * 1000;       // until the first successful sync
constexpr int kRefreshIntervalMs = 5 * 60 * 1000; // afterwards

QByteArray basicAuthHeader(const QString &username, const QString &password) {
	return "Basic " + (username + ":" + password).toUtf8().toBase64();
}

// Phone numbers are compared by digits only: the SDK may reformat what we add.
QSet<QString> digitSet(const QStringList &phones) {
	QSet<QString> out;
	for (const auto &phone : phones) {
		QString digits;
		for (const QChar ch : phone)
			if (ch.isDigit()) digits.append(ch);
		if (!digits.isEmpty()) out.insert(digits);
	}
	return out;
}
} // namespace

QSharedPointer<SharedContactsCore> SharedContactsCore::create() {
	auto model = QSharedPointer<SharedContactsCore>(new SharedContactsCore(), &QObject::deleteLater);
	model->moveToThread(App::getInstance()->thread());
	model->setSelf(model);
	return model;
}

SharedContactsCore::SharedContactsCore(QObject *parent) : QObject(parent) {
	mNetworkManager = new QNetworkAccessManager(this);
	mTimer.setSingleShot(true);
	connect(&mTimer, &QTimer::timeout, this, &SharedContactsCore::syncNow);
}

SharedContactsCore::~SharedContactsCore() {
}

void SharedContactsCore::setSelf(QSharedPointer<SharedContactsCore> me) {
	if (mCoreModelConnection) mCoreModelConnection->disconnect();
	mCoreModelConnection = SafeConnection<SharedContactsCore, CoreModel>::create(me, CoreModel::getInstance());
}

bool SharedContactsCore::getSyncing() const {
	return mSyncing;
}

void SharedContactsCore::setSyncing(bool value) {
	if (mSyncing != value) {
		mSyncing = value;
		emit syncingChanged();
	}
}

void SharedContactsCore::start() {
	if (mStarted) return;
	mStarted = true;
	mTimer.start(3000); // give the account a moment to register first
}

void SharedContactsCore::scheduleNext() {
	mTimer.start(mVersion.isEmpty() ? kRetryIntervalMs : kRefreshIntervalMs);
}

// Same credential source as OnlineStatusCore / CrmCardCore: the default
// account's own SIP AuthInfo, falling back to HA1 once the SDK has cleared the
// plaintext password after a successful REGISTER.
void SharedContactsCore::withCredentials(
    std::function<void(const QString &username, const QString &password)> callback) {
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

void SharedContactsCore::suggestContact(const QString &name, const QStringList &phones) {
	if (phones.isEmpty()) {
		emit suggestFinished(0, 0, 1);
		return;
	}
	withCredentials([this, name, phones](const QString &username, const QString &password) {
		if (username.isEmpty() || password.isEmpty()) {
			emit suggestFinished(0, 0, phones.size());
			return;
		}
		// Shared between the per-number replies; each reply reports once.
		struct Tally {
			int remaining;
			int queued = 0;
			int known = 0;
			int failed = 0;
		};
		auto tally = QSharedPointer<Tally>::create();
		tally->remaining = phones.size();

		for (const auto &phone : phones) {
			QNetworkRequest request{QUrl(QString::fromLatin1(kSuggestUrl))};
			request.setRawHeader("Authorization", basicAuthHeader(username, password));
			request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
			QJsonObject body;
			body.insert(QStringLiteral("name"), name);
			body.insert(QStringLiteral("phone"), phone);

			auto *reply = mNetworkManager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
			connect(reply, &QNetworkReply::finished, this, [this, reply, tally]() {
				reply->deleteLater();
				const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
				if (reply->error() != QNetworkReply::NoError || obj.contains(QStringLiteral("error"))) {
					++tally->failed;
				} else if (obj.value(QStringLiteral("status")).toString() == QStringLiteral("pending")) {
					++tally->queued;
				} else {
					++tally->known;
				}
				if (--tally->remaining == 0) emit suggestFinished(tally->queued, tally->known, tally->failed);
			});
		}
	});
}

void SharedContactsCore::renameClient(int clientId, const QString &name) {
	withCredentials([this, clientId, name](const QString &username, const QString &password) {
		if (username.isEmpty() || password.isEmpty()) {
			emit renameFinished(false, QString());
			return;
		}
		QNetworkRequest request{QUrl(QString::fromLatin1(kClientNameUrl).arg(clientId))};
		request.setRawHeader("Authorization", basicAuthHeader(username, password));
		request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
		QJsonObject body;
		body.insert(QStringLiteral("name"), name);

		auto *reply = mNetworkManager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
		connect(reply, &QNetworkReply::finished, this, [this, reply]() {
			reply->deleteLater();
			const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
			if (obj.value(QStringLiteral("status")).toString() == QStringLiteral("renamed")) {
				emit renameFinished(true, QString());
				mVersion.clear(); // force a full refresh so the new name shows up
				syncNow();
			} else {
				const QString error = obj.value(QStringLiteral("error")).toString();
				emit renameFinished(false, error);
			}
		});
	});
}

void SharedContactsCore::syncNow() {
	if (mSyncing) return;
	setSyncing(true);
	withCredentials([this](const QString &username, const QString &password) {
		if (username.isEmpty() || password.isEmpty()) {
			setSyncing(false);
			scheduleNext();
			return;
		}

		QUrl url(QString::fromLatin1(kContactsUrl));
		if (!mVersion.isEmpty()) {
			QUrlQuery query;
			query.addQueryItem(QStringLiteral("version"), mVersion);
			url.setQuery(query);
		}
		QNetworkRequest request(url);
		request.setRawHeader("Authorization", basicAuthHeader(username, password));

		auto *reply = mNetworkManager->get(request);
		connect(reply, &QNetworkReply::finished, this, [this, reply, username]() {
			reply->deleteLater();
			QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
			if (reply->error() != QNetworkReply::NoError || obj.contains(QStringLiteral("error"))) {
				setSyncing(false);
				emit syncFailed(obj.contains(QStringLiteral("error")) ? obj.value(QStringLiteral("error")).toString()
				                                                      : reply->errorString());
				scheduleNext();
				return;
			}
			if (obj.value(QStringLiteral("unchanged")).toBool()) {
				setSyncing(false);
				scheduleNext();
				return;
			}

			QList<SharedContactEntry> entries;
			for (const auto &value : obj.value(QStringLiteral("employees")).toArray()) {
				QJsonObject row = value.toObject();
				QString extension = row.value(QStringLiteral("extension")).toString();
				if (extension.isEmpty() || extension == username) continue; // not ourselves
				SharedContactEntry entry;
				entry.refKey = QString::fromLatin1(kRefKeyPrefix) + QStringLiteral("emp:") + extension;
				entry.name = row.value(QStringLiteral("name")).toString() + QStringLiteral(" (") + extension +
				             QStringLiteral(")");
				entry.phones << extension;
				entries << entry;
			}
			for (const auto &value : obj.value(QStringLiteral("clients")).toArray()) {
				QJsonObject row = value.toObject();
				SharedContactEntry entry;
				entry.refKey = QString::fromLatin1(kRefKeyPrefix) + QStringLiteral("cli:") +
				               QString::number(row.value(QStringLiteral("id")).toInt());
				QString phone = row.value(QStringLiteral("phone")).toString();
				if (!phone.isEmpty()) entry.phones << phone;
				for (const auto &extra : row.value(QStringLiteral("extraPhones")).toArray()) {
					QString extraPhone = extra.toString();
					if (!extraPhone.isEmpty() && !entry.phones.contains(extraPhone)) entry.phones << extraPhone;
				}
				if (entry.phones.isEmpty()) continue;
				entry.name = row.value(QStringLiteral("name")).toString();
				if (entry.name.isEmpty()) entry.name = entry.phones.value(0);
				entries << entry;
			}
			apply(obj.value(QStringLiteral("version")).toString(), entries);
		});
	});
}

// Mirrors the server list into the "kiwicall_shared" friend list (linphone
// thread): adds new entries, updates changed ones, removes the ones that
// disappeared server-side. Entries are matched by refKey.
void SharedContactsCore::apply(const QString &version, const QList<SharedContactEntry> &entries) {
	mCoreModelConnection->invokeToModel([this, version, entries]() {
		auto core = CoreModel::getInstance()->getCore();
		auto friendList = ToolModel::getFriendList(kSharedListName);
		// Presence SUBSCRIBEs for thousands of CRM clients would only be noise.
		friendList->enableSubscriptions(false);

		int added = 0, updated = 0, removed = 0;
		QSet<QString> wanted;
		for (const auto &entry : entries) {
			wanted.insert(entry.refKey);
			auto existing = friendList->findFriendByRefKey(entry.refKey.toStdString());
			if (!existing) {
				auto contact = core->createFriend();
				contact->setName(entry.name.toStdString());
				contact->setRefKey(entry.refKey.toStdString());
				for (const auto &phone : entry.phones)
					contact->addPhoneNumber(phone.toStdString());
				if (friendList->addFriend(contact) == linphone::FriendList::Status::OK) ++added;
				continue;
			}
			QStringList currentPhones;
			for (const auto &number : existing->getPhoneNumbers())
				currentPhones << Utils::coreStringToAppString(number);
			if (Utils::coreStringToAppString(existing->getName()) == entry.name &&
			    digitSet(currentPhones) == digitSet(entry.phones))
				continue;
			existing->edit();
			existing->setName(entry.name.toStdString());
			for (const auto &number : existing->getPhoneNumbers())
				existing->removePhoneNumber(number);
			for (const auto &phone : entry.phones)
				existing->addPhoneNumber(phone.toStdString());
			existing->done();
			++updated;
		}
		for (const auto &contact : friendList->getFriends()) {
			QString key = QString::fromStdString(contact->getRefKey());
			if (!key.startsWith(QString::fromLatin1(kRefKeyPrefix))) continue; // never touch foreign entries
			if (wanted.contains(key)) continue;
			friendList->removeFriend(contact);
			++removed;
		}

		mCoreModelConnection->invokeToCore([this, version, added, updated, removed]() {
			mVersion = version;
			setSyncing(false);
			emit syncFinished(added, updated, removed);
			scheduleNext();
		});
	});
}
