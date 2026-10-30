// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef ARCHIVEFOLDERS_H
#define ARCHIVEFOLDERS_H

#include <QHash>
#include <QObject>
#include <QSettings>

// Each account's archive folder, as the ArchiveFolders singleton: the one chosen in Settings, or
// else the one found by name (ArchiveFolder::guess), as QMF doesn't know of archives. Archiving
// is moving there.
class ArchiveFolders : public QObject
{
    Q_OBJECT
    // Bumped when an archive folder may have changed: folderId() doesn't notify.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit ArchiveFolders(QObject *parent = nullptr);

    int revision() const { return m_revision; }

    // 0 when the account has none.
    Q_INVOKABLE int folderId(int accountId) const;
    // The archive folder's name, empty when there is none.
    Q_INVOKABLE QString name(int accountId) const;
    // Whether folderId() is one chosen rather than found.
    Q_INVOKABLE bool chosen(int accountId) const;
    // 0 for no archive at all.
    Q_INVOKABLE void choose(int accountId, int folderId);
    // Back to the one found by name.
    Q_INVOKABLE void forget(int accountId);

signals:
    void changed();

private:
    void foldersChanged();
    QString key(int accountId) const;

    QSettings m_settings;
    int m_revision = 0;
    mutable QHash<int, int> m_guesses;
};

#endif // ARCHIVEFOLDERS_H
