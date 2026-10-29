// SPDX-License-Identifier: AGPL-3.0-or-later
#include "maildraft.h"

#include <QStringList>

namespace {

bool isQuoted(const QString &line)
{
    return line.startsWith(QLatin1Char('>'));
}

// What the composer puts above a quote ends with a colon, in any language ("… wrote:").
bool isAttribution(const QString &line)
{
    return line.trimmed().endsWith(QLatin1Char(':'));
}

bool anyQuoted(const QStringList &lines, int from, int to)
{
    for (int i = from; i < to; ++i) {
        if (isQuoted(lines.at(i)))
            return true;
    }
    return false;
}

QString joined(const QStringList &lines, int from, int to)
{
    return lines.mid(from, to - from).join(QLatin1Char('\n')).trimmed();
}

// Takes the signature ("-- " and what follows) off the end of text.
void splitSignature(QString *text, QString *signature)
{
    const QStringList lines = text->split(QLatin1Char('\n'));
    for (int i = lines.size() - 1; i >= 0; --i) {
        if (lines.at(i) == QLatin1String("-- ")) {
            *signature = joined(lines, i + 1, lines.size());
            *text = joined(lines, 0, i);
            return;
        }
    }
}

} // namespace

MailDraft MailDraft::split(const QString &body)
{
    MailDraft draft;
    QString normalized = body;
    normalized.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    const QStringList lines = normalized.split(QLatin1Char('\n'));

    // Written above: a quote that runs to the end, after an introduction on the line before it,
    // and no quoting in the text (that would be a reply written in between the quote).
    int end = lines.size();
    while (end > 0 && lines.at(end - 1).trimmed().isEmpty())
        --end;
    int start = end;
    while (start > 0 && isQuoted(lines.at(start - 1)))
        --start;
    if (start < end && start > 0 && isAttribution(lines.at(start - 1))
            && !anyQuoted(lines, 0, start - 1)) {
        draft.attribution = lines.at(start - 1).trimmed();
        draft.quote = lines.mid(start, end - start).join(QLatin1Char('\n'));
        draft.text = joined(lines, 0, start - 1);
        splitSignature(&draft.text, &draft.signature);
        return draft;
    }

    // Written below: an introduction on the first line, then the quote.
    int quoteEnd = 1;
    while (quoteEnd < lines.size() && isQuoted(lines.at(quoteEnd)))
        ++quoteEnd;
    if (lines.size() > 1 && isAttribution(lines.first()) && quoteEnd > 1
            && !anyQuoted(lines, quoteEnd, lines.size())) {
        draft.attribution = lines.first().trimmed();
        draft.quote = lines.mid(1, quoteEnd - 1).join(QLatin1Char('\n'));
        draft.text = joined(lines, quoteEnd, lines.size());
        draft.writtenBelowQuote = true;
        splitSignature(&draft.text, &draft.signature);
        return draft;
    }

    draft.text = normalized.trimmed();
    splitSignature(&draft.text, &draft.signature);
    return draft;
}
