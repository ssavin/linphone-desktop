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

#ifndef SHARED_CONTACTS_CORE_H_
#define SHARED_CONTACTS_CORE_H_

#include "model/core/CoreModel.hpp"
#include "tool/AbstractObject.hpp"
#include "tool/thread/SafeConnection.hpp"
#include <QNetworkAccessManager>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <functional>

struct SharedContactEntry {
	QString refKey;
	QString name;
	QStringList phones;
};

// Shared (call-center wide) address book: clients and colleagues of the
// operator's clinic, pulled from kiwicall.ru (/api/mobile/contacts) with the
// account's own SIP credentials and mirrored into a dedicated read-only
// friend list. The server sends a version token, so polling is cheap when
// nothing changed.
class SharedContactsCore : public QObject, public AbstractObject {
	Q_OBJECT
	Q_PROPERTY(bool syncing READ getSyncing NOTIFY syncingChanged)

public:
	static QSharedPointer<SharedContactsCore> create();
	SharedContactsCore(QObject *parent = nullptr);
	~SharedContactsCore();

	void setSelf(QSharedPointer<SharedContactsCore> me);

	bool getSyncing() const;

	// Idempotent: starts the periodic sync (first attempt shortly after start).
	Q_INVOKABLE void start();
	Q_INVOKABLE void syncNow();
	// Opt-in "В общую книгу": sends only the name and the chosen numbers to the
	// clinic admin's moderation queue. Emits suggestFinished(queued, known, failed).
	Q_INVOKABLE void suggestContact(const QString &name, const QStringList &phones);

signals:
	void suggestFinished(int queued, int known, int failed);
	void syncingChanged();
	void syncFinished(int added, int updated, int removed);
	void syncFailed(const QString &message);

private:
	void setSyncing(bool value);
	void scheduleNext();
	void withCredentials(std::function<void(const QString &username, const QString &password)> callback);
	void apply(const QString &version, const QList<SharedContactEntry> &entries);

	QNetworkAccessManager *mNetworkManager = nullptr;
	QTimer mTimer;
	QString mVersion;
	bool mSyncing = false;
	bool mStarted = false;
	QSharedPointer<SafeConnection<SharedContactsCore, CoreModel>> mCoreModelConnection;

	DECLARE_ABSTRACT_OBJECT
};
Q_DECLARE_METATYPE(QSharedPointer<SharedContactsCore>);

#endif // SHARED_CONTACTS_CORE_H_
