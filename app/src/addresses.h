// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef ADDRESSES_H
#define ADDRESSES_H

#include "mailaddresses.h"

#include <QObject>

// MailAddresses for QML, as the Addresses singleton.
class Addresses : public QObject
{
    Q_OBJECT

public:
    explicit Addresses(QObject *parent = nullptr) : QObject(parent) { }

    Q_INVOKABLE QStringList split(const QString &text) const { return MailAddresses::split(text); }
    Q_INVOKABLE QString address(const QString &entry) const { return MailAddresses::address(entry); }
    Q_INVOKABLE bool isValid(const QString &entry) const { return MailAddresses::isValid(entry); }
    Q_INVOKABLE QStringList without(const QStringList &entries, const QStringList &excluded) const
    {
        return MailAddresses::without(entries, excluded);
    }
};

#endif // ADDRESSES_H
