// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef ARCHIVEFOLDER_H
#define ARCHIVEFOLDER_H

#include <QStringList>

// QMF has no archive among its standard folders, so it's recognised by name.
namespace ArchiveFolder {

// Which of an account's folder paths ("Archive", "INBOX.Archief", "[Gmail]/All Mail") is its
// archive, or -1. A folder called archive wins over Gmail's All Mail, and one higher up over one
// further down.
int guess(const QStringList &paths);

} // namespace ArchiveFolder

#endif // ARCHIVEFOLDER_H
