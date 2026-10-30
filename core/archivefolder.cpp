// SPDX-License-Identifier: AGPL-3.0-or-later
#include "archivefolder.h"

#include <QRegularExpression>

namespace ArchiveFolder {

namespace {

// The levels of a path: IMAP servers separate them with '/' or '.'.
QStringList levels(const QString &path)
{
    static const QRegularExpression separator(QStringLiteral("[/.]"));
    return path.split(separator, QString::SkipEmptyParts);
}

// The folder's own name.
QString leaf(const QString &path)
{
    const QStringList parts = levels(path);
    return parts.isEmpty() ? QString() : parts.last().trimmed().toLower();
}

// Lower is better, -1 for not an archive.
int rank(const QString &path)
{
    static const QStringList archives = {
        QStringLiteral("archive"), QStringLiteral("archives"), QStringLiteral("archief"),
        QStringLiteral("gearchiveerd"), QStringLiteral("archiv"), QStringLiteral("archivio"),
        QStringLiteral("archivo"), QStringLiteral("arkiv"), QStringLiteral("arkisto"),
    };
    // Gmail archives by taking the Inbox label off, which over IMAP is a move to All Mail.
    static const QStringList allMail = {
        QStringLiteral("all mail"), QStringLiteral("alle berichten"), QStringLiteral("alle nachrichten"),
        QStringLiteral("tous les messages"), QStringLiteral("todos"), QStringLiteral("tutti i messaggi"),
    };
    const QString name = leaf(path);
    if (archives.contains(name))
        return 0;
    if (allMail.contains(name) && path.startsWith(QLatin1Char('[')))
        return 1;
    return -1;
}

} // namespace

int guess(const QStringList &paths)
{
    int best = -1;
    for (int i = 0; i < paths.size(); ++i) {
        const int r = rank(paths.at(i));
        if (r < 0)
            continue;
        if (best < 0 || r < rank(paths.at(best))
                || (r == rank(paths.at(best)) && levels(paths.at(i)).size() < levels(paths.at(best)).size()))
            best = i;
    }
    return best;
}

} // namespace ArchiveFolder
