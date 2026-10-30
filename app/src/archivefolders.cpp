// SPDX-License-Identifier: AGPL-3.0-or-later
#include "archivefolders.h"

#include "appstate.h"
#include "archivefolder.h"

#include <qmailfolder.h>
#include <qmailstore.h>

ArchiveFolders::ArchiveFolders(QObject *parent)
    : QObject(parent)
    , m_settings(AppState::fileName(), QSettings::IniFormat)
{
    // Folders turn up with the account's first sync, and can be renamed or removed.
    QMailStore *store = QMailStore::instance();
    connect(store, &QMailStore::foldersAdded, this, &ArchiveFolders::foldersChanged);
    connect(store, &QMailStore::foldersUpdated, this, &ArchiveFolders::foldersChanged);
    connect(store, &QMailStore::foldersRemoved, this, &ArchiveFolders::foldersChanged);
}

int ArchiveFolders::folderId(int accountId) const
{
    if (accountId <= 0)
        return 0;
    if (chosen(accountId)) {
        const int id = m_settings.value(key(accountId)).toInt();
        // A chosen folder that's gone counts as not chosen.
        if (id == 0 || QMailStore::instance()->countFolders(QMailFolderKey::id(QMailFolderId(id))) > 0)
            return id;
    }
    const auto cached = m_guesses.constFind(accountId);
    if (cached != m_guesses.constEnd())
        return *cached;

    const QMailFolderIdList ids = QMailStore::instance()->queryFolders(
                QMailFolderKey::parentAccountId(QMailAccountId(accountId)));
    QStringList paths;
    for (const QMailFolderId &id : ids)
        paths.append(QMailFolder(id).path());
    const int index = ArchiveFolder::guess(paths);
    const int id = index >= 0 ? int(ids.at(index).toULongLong()) : 0;
    m_guesses.insert(accountId, id);
    return id;
}

QString ArchiveFolders::name(int accountId) const
{
    const int id = folderId(accountId);
    return id > 0 ? QMailFolder(QMailFolderId(id)).displayName() : QString();
}

bool ArchiveFolders::chosen(int accountId) const
{
    return m_settings.contains(key(accountId));
}

void ArchiveFolders::choose(int accountId, int folderId)
{
    if (chosen(accountId) && m_settings.value(key(accountId)).toInt() == folderId)
        return;
    m_settings.setValue(key(accountId), folderId);
    m_settings.sync();
    ++m_revision;
    emit changed();
}

void ArchiveFolders::forget(int accountId)
{
    if (!chosen(accountId))
        return;
    m_settings.remove(key(accountId));
    m_settings.sync();
    ++m_revision;
    emit changed();
}

void ArchiveFolders::foldersChanged()
{
    m_guesses.clear();
    ++m_revision;
    emit changed();
}

QString ArchiveFolders::key(int accountId) const
{
    return QStringLiteral("archive/%1").arg(accountId);
}
