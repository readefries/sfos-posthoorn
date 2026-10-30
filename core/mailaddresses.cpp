// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mailaddresses.h"

#include <QRegularExpression>
#include <QSet>

namespace MailAddresses {

QStringList split(const QString &text)
{
    QStringList result;
    QString current;
    bool quoted = false;
    bool bracketed = false;
    for (const QChar c : text) {
        if (c == QLatin1Char('"') && !bracketed) {
            quoted = !quoted;
        } else if (c == QLatin1Char('<') && !quoted) {
            bracketed = true;
        } else if (c == QLatin1Char('>') && !quoted) {
            bracketed = false;
        } else if ((c == QLatin1Char(',') || c == QLatin1Char(';')) && !quoted && !bracketed) {
            if (!current.trimmed().isEmpty())
                result.append(current.trimmed());
            current.clear();
            continue;
        }
        current += c;
    }
    if (!current.trimmed().isEmpty())
        result.append(current.trimmed());
    return result;
}

QString address(const QString &entry)
{
    const int open = entry.lastIndexOf(QLatin1Char('<'));
    const int close = entry.lastIndexOf(QLatin1Char('>'));
    if (open >= 0 && close > open)
        return entry.mid(open + 1, close - open - 1).trimmed().toLower();
    return entry.trimmed().toLower();
}

bool isValid(const QString &entry)
{
    static const QRegularExpression valid(QStringLiteral("^[^\\s@<>\"]+@[^\\s@<>\"]+\\.[^\\s@<>\"]+$"));
    return valid.match(address(entry)).hasMatch();
}

QString format(const QString &name, const QString &address)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed.compare(address, Qt::CaseInsensitive) == 0)
        return address;
    static const QRegularExpression special(QStringLiteral("[()<>\\[\\]:;@\\\\,.\"]"));
    QString shown = trimmed;
    if (special.match(shown).hasMatch()) {
        shown.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
        shown.replace(QLatin1Char('"'), QLatin1String("\\\""));
        shown = QLatin1Char('"') + shown + QLatin1Char('"');
    }
    return shown + QLatin1String(" <") + address + QLatin1Char('>');
}

QStringList without(const QStringList &entries, const QStringList &excluded)
{
    QSet<QString> seen;
    for (const QString &entry : excluded)
        seen.insert(address(entry));
    QStringList result;
    for (const QString &entry : entries) {
        const QString key = address(entry);
        if (key.isEmpty() || seen.contains(key))
            continue;
        seen.insert(key);
        result.append(entry);
    }
    return result;
}

} // namespace MailAddresses
