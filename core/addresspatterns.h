// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef ADDRESSPATTERNS_H
#define ADDRESSPATTERNS_H

#include <QString>
#include <QStringList>

// Addresses one can send from without each being an alias, as Thunderbird's "Reply from this
// identity when delivery headers match": patterns such as *@example.org or me+*@example.org, where
// * stands for any characters but @. Case doesn't matter.
namespace AddressPatterns {

// The patterns in text, separated by commas, semicolons or white space, in lower case. Those
// without exactly one @, with nothing before it or with a domain of only * are left out: they'd
// match far more than meant.
QStringList parse(const QString &text);

// Whether address (bare, as in MailAddresses::address) matches one of patterns.
bool matches(const QString &address, const QStringList &patterns);

// The first of candidates (header values: "Name <address>" or bare addresses, possibly several
// per value) whose address matches one of patterns, as that bare address in lower case; "" when
// none does.
QString firstMatch(const QStringList &candidates, const QStringList &patterns);

} // namespace AddressPatterns

#endif // ADDRESSPATTERNS_H
