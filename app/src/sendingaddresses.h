// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef SENDINGADDRESSES_H
#define SENDINGADDRESSES_H

#include <QObject>
#include <QSettings>
#include <QVariantList>

#include <qmailaccount.h>
#include <qmailaddress.h>

// The addresses one sends from, as the SendingAddresses singleton: each account's address and
// aliases from Settings → Accounts, and the addresses matching patterns chosen per account in
// Posthoorn's Settings (AddressPatterns), such as *@example.org for a domain that delivers all its
// mail to the account. A reply goes out from the address the mail was delivered to when it
// matches one.
class SendingAddresses : public QObject
{
    Q_OBJECT
    // Bumped when patterns change: patterns() doesn't notify.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit SendingAddresses(QObject *parent = nullptr);

    int revision() const { return m_revision; }

    // The account's patterns, in the order added.
    Q_INVOKABLE QStringList patterns(int accountId) const;
    // Adds one pattern, in lower case. False, changing nothing, when it isn't one
    // (AddressPatterns::parse leaves it out: it would match far too much) or the account has it.
    Q_INVOKABLE bool addPattern(int accountId, const QString &pattern);
    Q_INVOKABLE void removePattern(int accountId, const QString &pattern);

    // { address, accountId } for each address of each enabled account, and extra (an address
    // matching an account's patterns, as a reply's) after its account's when it isn't one of them.
    Q_INVOKABLE QVariantList choices(const QString &extra) const;
    // The address of the account's patterns the mail was delivered to (Delivered-To,
    // X-Original-To, …, then To and Cc), or "".
    Q_INVOKABLE QString replyAddress(int messageId) const;
    // The addresses among entries that match one of the accounts' patterns, for leaving them out
    // of a reply to all.
    Q_INVOKABLE QStringList ownAmong(const QStringList &entries) const;

    // The enabled account address belongs to, as its address, an alias or one of its patterns, and
    // the address to send from (with the account's name); an invalid account when none.
    static QMailAccount accountFor(const QString &address, QMailAddress *sender);

signals:
    void changed();

private:
    static QStringList patternsOf(const QSettings &settings, const QMailAccountId &id);
    void store(const QMailAccountId &id, const QStringList &patterns);

    QSettings m_settings;
    int m_revision = 0;
};

#endif // SENDINGADDRESSES_H
