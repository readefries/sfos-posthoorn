// SPDX-License-Identifier: AGPL-3.0-or-later
#include "flowedtext.h"

#include <QStringList>

namespace FlowedText {

namespace {

// Lines starting with a space, or with "From " (which some systems mangle), get a space put in
// front; readers of flowed text take it off again.
QString stuffed(const QString &line)
{
    if (line.startsWith(QLatin1Char(' ')) || line.startsWith(QLatin1String("From ")))
        return QLatin1Char(' ') + line;
    return line;
}

} // namespace

QString encode(const QString &text, int width)
{
    QString normalized = text;
    normalized.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QStringList output;
    for (const QString &line : normalized.split(QLatin1Char('\n'))) {
        if (line == QLatin1String("-- ")) {
            output.append(line);
            continue;
        }

        int depth = 0;
        while (depth < line.size() && line.at(depth) == QLatin1Char('>'))
            ++depth;
        QString content = line.mid(depth);
        // The space after the quote markers separates them from the text; it's put back below.
        if (depth > 0 && content.startsWith(QLatin1Char(' ')))
            content.remove(0, 1);
        // A trailing space would make the line flow into the next one.
        int end = content.size();
        while (end > 0 && content.at(end - 1) == QLatin1Char(' '))
            --end;
        content.truncate(end);

        const QString prefix = depth > 0 ? QString(depth, QLatin1Char('>')) + QLatin1Char(' ') : QString();
        if (content.isEmpty()) {
            output.append(QString(depth, QLatin1Char('>')));
            continue;
        }

        const int room = qMax(width - prefix.size(), 20);
        while (content.size() > room) {
            // Break after the last space that fits, or else the first one there is.
            int breakAt = content.lastIndexOf(QLatin1Char(' '), room - 1);
            if (breakAt <= 0)
                breakAt = content.indexOf(QLatin1Char(' '), room);
            if (breakAt <= 0)
                break;
            const QString chunk = content.left(breakAt + 1);
            output.append(depth > 0 ? prefix + chunk : stuffed(chunk));
            content = content.mid(breakAt + 1);
        }
        output.append(depth > 0 ? prefix + content : stuffed(content));
    }
    return output.join(QLatin1Char('\n'));
}

} // namespace FlowedText
