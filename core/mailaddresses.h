// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MAILADDRESSES_H
#define MAILADDRESSES_H

#include <QString>
#include <QStringList>

// Recipients as people type and mail headers write them: "Name <address>" or a bare address.
namespace MailAddresses {

// Splits a recipients field on commas and semicolons, but not those in a quoted name or between
// angle brackets ("Doe, Jan" <jan@example.org>). Entries are trimmed, empty ones dropped.
QStringList split(const QString &text);

// The address of an entry: what's between the angle brackets, or the entry itself. Lower case.
QString address(const QString &entry);

// Whether entry looks like something that can be sent to.
bool isValid(const QString &entry);

// entries without those whose address is in excluded, nor repeats of an address.
QStringList without(const QStringList &entries, const QStringList &excluded);

} // namespace MailAddresses

#endif // MAILADDRESSES_H
