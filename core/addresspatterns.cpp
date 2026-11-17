// SPDX-License-Identifier: AGPL-3.0-or-later
#include "addresspatterns.h"

#include "mailaddresses.h"

#include <QRegularExpression>

namespace AddressPatterns {

QStringList parse(const QString &text)
{
    QStringList patterns;
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[,;\\s]+")),
                                         QString::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString pattern = part.toLower();
        const QStringList sides = pattern.split(QLatin1Char('@'));
        if (sides.size() != 2)
            continue;
        // Something before the @, and a domain that's more than *.
        QString domain = sides.at(1);
        domain.remove(QLatin1Char('*'));
        if (!sides.at(0).isEmpty() && !domain.isEmpty() && !patterns.contains(pattern))
            patterns.append(pattern);
    }
    return patterns;
}

bool matches(const QString &address, const QStringList &patterns)
{
    const QString bare = address.trimmed().toLower();
    if (bare.isEmpty())
        return false;
    for (const QString &pattern : patterns) {
        QStringList literals = pattern.split(QLatin1Char('*'));
        for (QString &literal : literals)
            literal = QRegularExpression::escape(literal);
        const QRegularExpression re(QLatin1Char('^') + literals.join(QStringLiteral("[^@]*")) + QLatin1Char('$'));
        if (re.match(bare).hasMatch())
            return true;
    }
    return false;
}

QString firstMatch(const QStringList &candidates, const QStringList &patterns)
{
    if (patterns.isEmpty())
        return QString();
    for (const QString &value : candidates) {
        for (const QString &entry : MailAddresses::split(value)) {
            const QString address = MailAddresses::address(entry);
            if (matches(address, patterns))
                return address;
        }
    }
    return QString();
}

} // namespace AddressPatterns
