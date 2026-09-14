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

#ifndef CONTACT_IMPORT_CORE_H_
#define CONTACT_IMPORT_CORE_H_

#include "model/core/CoreModel.hpp"
#include "tool/AbstractObject.hpp"
#include "tool/thread/SafeConnection.hpp"
#include <QNetworkAccessManager>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

struct ImportedContactEntry {
	QString name;
	QStringList phones;
};

// Bulk contact import: from a local CSV file, or by pulling the clinic's
// client list from the KiwiCall web CRM (same login as kiwicall.ru).
class ContactImportCore : public QObject, public AbstractObject {
	Q_OBJECT

public:
	static QSharedPointer<ContactImportCore> create();
	ContactImportCore(QObject *parent = nullptr);
	~ContactImportCore();

	void setSelf(QSharedPointer<ContactImportCore> me);

	Q_INVOKABLE void importFromCsv(const QString &filePath);
	Q_INVOKABLE void importFromKiwiCall(const QString &email, const QString &password);

signals:
	void importStarted();
	void importFinished(int successCount, int errorCount);
	void importError(const QString &message);

private:
	void createFriends(const QList<ImportedContactEntry> &contacts);
	void
	fetchClientsPage(const QString &sessionCookie, int page, QSharedPointer<QList<ImportedContactEntry>> accumulator);

	QNetworkAccessManager *mNetworkManager = nullptr;
	QSharedPointer<SafeConnection<ContactImportCore, CoreModel>> mCoreModelConnection;

	DECLARE_ABSTRACT_OBJECT
};
Q_DECLARE_METATYPE(QSharedPointer<ContactImportCore>);

#endif // CONTACT_IMPORT_CORE_H_
