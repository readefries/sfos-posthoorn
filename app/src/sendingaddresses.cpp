// SPDX-License-Identifier: AGPL-3.0-or-later
#include "sendingaddresses.h"

#include "addresspatterns.h"
#include "appstate.h"
#include "mailaddresses.h"

#include <qmailmessage.h>
#include <qmailstore.h>

namespace {

QString settingsKey(const QMailAccountId &id)
{
    return QStringLiteral("replyPatterns/") + QString::number(id.toULongLong());
}

QMailAccountIdList sendingAccounts()
{
    return QMailStore::instance()->queryAccounts(
                QMailAccountKey::messageType(QMailMessage::Email)
                & QMailAccountKey::status(QMailAccount::Enabled),
                QMailAccountSortKey::name());
}

// The account's address and its aliases, with the names to send with.
QList<QMailAddress> addressesOf(const QMailAccount &account)
{
    const QMailAddress main = account.fromAddress();
    QList<QMailAddress> result;
    if (!main.address().isEmpty())
        result.append(main);
    const QMailAddress aliases = account.fromAliases();
    const QList<QMailAddress> members = aliases.isGroup() ? aliases.groupMembers()
                                                          : QList<QMailAddress>() << aliases;
    for (const QMailAddress &alias : members) {
        if (!alias.isNull() && !alias.address().isEmpty())
            result.append(QMailAddress(alias.name().isEmpty() ? main.name() : alias.name(), alias.address()));
    }
    return result;
}

} // namespace

SendingAddresses::SendingAddresses(QObject *parent)
    : QObject(parent)
    , m_settings(AppState::fileName(), QSettings::IniFormat)
{
}

QStringList SendingAddresses::patternsOf(const QSettings &settings, const QMailAccountId &id)
{
    return AddressPatterns::parse(settings.value(settingsKey(id)).toString());
}

QStringList SendingAddresses::patterns(int accountId) const
{
    return patternsOf(m_settings, QMailAccountId(accountId));
}

bool SendingAddresses::addPattern(int accountId, const QString &pattern)
{
    const QStringList parsed = AddressPatterns::parse(pattern);
    // One pattern, and a whole one: "a@b c" isn't.
    if (parsed.size() != 1 || parsed.first() != pattern.trimmed().toLower())
        return false;
    const QMailAccountId id(accountId);
    QStringList patterns = patternsOf(m_settings, id);
    if (patterns.contains(parsed.first()))
        return false;
    patterns.append(parsed.first());
    store(id, patterns);
    return true;
}

void SendingAddresses::removePattern(int accountId, const QString &pattern)
{
    const QMailAccountId id(accountId);
    QStringList patterns = patternsOf(m_settings, id);
    if (patterns.removeAll(pattern.trimmed().toLower()) > 0)
        store(id, patterns);
}

void SendingAddresses::store(const QMailAccountId &id, const QStringList &patterns)
{
    // As text that parse() reads back.
    if (patterns.isEmpty())
        m_settings.remove(settingsKey(id));
    else
        m_settings.setValue(settingsKey(id), patterns.join(QStringLiteral(", ")));
    m_settings.sync();
    ++m_revision;
    emit changed();
}

QVariantList SendingAddresses::choices(const QString &extra) const
{
    const QString wanted = MailAddresses::address(extra);
    QVariantList result;
    for (const QMailAccountId &id : sendingAccounts()) {
        const QMailAccount account(id);
        bool listed = false;
        for (const QMailAddress &address : addressesOf(account)) {
            listed = listed || address.address().compare(wanted, Qt::CaseInsensitive) == 0;
            QVariantMap choice;
            choice.insert(QStringLiteral("address"), address.address());
            choice.insert(QStringLiteral("accountId"), int(id.toULongLong()));
            result.append(choice);
        }
        if (!wanted.isEmpty() && !listed && AddressPatterns::matches(wanted, patternsOf(m_settings, id))) {
            QVariantMap choice;
            choice.insert(QStringLiteral("address"), wanted);
            choice.insert(QStringLiteral("accountId"), int(id.toULongLong()));
            result.append(choice);
        }
    }
    return result;
}

QString SendingAddresses::replyAddress(int messageId) const
{
    const QMailMessage message{QMailMessageId(messageId)};
    if (!message.id().isValid())
        return QString();
    const QStringList patterns = patternsOf(m_settings, message.parentAccountId());
    if (patterns.isEmpty())
        return QString();
    // Where the mail was delivered: the headers servers add, then where it says it was sent.
    QStringList candidates;
    for (const char *header : { "Delivered-To", "X-Original-To", "X-Delivered-To", "Envelope-To" })
        candidates += message.headerFieldsText(QString::fromLatin1(header));
    for (const QMailAddress &address : message.to() + message.cc())
        candidates.append(address.toString());
    return AddressPatterns::firstMatch(candidates, patterns);
}

QStringList SendingAddresses::ownAmong(const QStringList &entries) const
{
    QList<QStringList> patterns;
    for (const QMailAccountId &id : sendingAccounts())
        patterns.append(patternsOf(m_settings, id));
    QStringList own;
    for (const QString &entry : entries) {
        const QString address = MailAddresses::address(entry);
        for (const QStringList &accountPatterns : patterns) {
            if (AddressPatterns::matches(address, accountPatterns)) {
                own.append(address);
                break;
            }
        }
    }
    return own;
}

QMailAccount SendingAddresses::accountFor(const QString &address, QMailAddress *sender)
{
    const QString wanted = MailAddresses::address(address);
    const QMailAccountIdList ids = sendingAccounts();
    for (const QMailAccountId &id : ids) {
        const QMailAccount account(id);
        for (const QMailAddress &own : addressesOf(account)) {
            if (own.address().compare(wanted, Qt::CaseInsensitive) == 0) {
                *sender = own;
                return account;
            }
        }
    }
    // An account's own addresses go first, so a pattern of another account can't take them.
    const QSettings settings(AppState::fileName(), QSettings::IniFormat);
    for (const QMailAccountId &id : ids) {
        if (AddressPatterns::matches(wanted, patternsOf(settings, id))) {
            const QMailAccount account(id);
            *sender = QMailAddress(account.fromAddress().name(), wanted);
            return account;
        }
    }
    return QMailAccount();
}
