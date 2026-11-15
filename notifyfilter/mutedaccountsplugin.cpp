// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mutedaccountsplugin.h"

#include <qmailmessage.h>
#include <qmailmessagekey.h>
#include <qmailstore.h>

#include <QDebug>
#include <QSettings>
#include <QStandardPaths>

namespace {

// Posthoorn's state.ini (AppState::fileName()). The messageserver isn't Posthoorn, so the path is
// spelled out.
QString stateFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/nl.readefries/posthoorn/state.ini");
}

} // namespace

MutedAccountsService::MutedAccountsService()
    : QMailMessageServerService()
{
    connect(QMailStore::instance(), &QMailStore::messagesAdded,
            this, &MutedAccountsService::messagesAdded);
    qDebug() << "Posthoorn: no notifications for mail of the accounts switched off in Posthoorn";
}

QMailAccountIdList MutedAccountsService::mutedAccounts()
{
    QSettings settings(stateFile(), QSettings::IniFormat);
    QMailAccountIdList accounts;
    const QStringList entries = settings.value(QStringLiteral("notifications/mutedAccounts")).toStringList();
    for (const QString &entry : entries) {
        bool ok = false;
        const quint64 id = entry.toULongLong(&ok);
        if (ok && id > 0)
            accounts.append(QMailAccountId(id));
    }
    return accounts;
}

void MutedAccountsService::messagesAdded(const QMailMessageIdList &ids)
{
    const QMailAccountIdList muted = mutedAccounts();
    if (muted.isEmpty() || ids.isEmpty())
        return;
    const QMailMessageKey key = QMailMessageKey::id(ids)
            & QMailMessageKey::parentAccountId(muted)
            & QMailMessageKey::status(QMailMessage::NoNotification, QMailDataComparator::Excludes);
    if (QMailStore::instance()->countMessages(key) == 0)
        return;
    if (!QMailStore::instance()->updateMessagesMetaData(key, QMailMessage::NoNotification, true))
        qWarning() << "Posthoorn: couldn't keep new mail of a muted account quiet";
}

MutedAccountsPlugin::MutedAccountsPlugin(QObject *parent)
    : QMailMessageServerPlugin(parent)
{
}

QString MutedAccountsPlugin::key() const
{
    // Before "notifications", see the header.
    return QStringLiteral("mutedaccounts");
}

QMailMessageServerService *MutedAccountsPlugin::createService()
{
    return new MutedAccountsService;
}
