// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef APPSTATE_H
#define APPSTATE_H

#include <QObject>
#include <QSettings>

// Where the app was left, so it can open there again: the account overview, an account's
// folders, or a folder's messages. And which folders the unified inbox shows. As the AppState
// singleton.
//
// Kept in ~/.config/nl.readefries/posthoorn/state.ini: QSettings' default file is one level up,
// outside what the sandbox lets the app write.
class AppState : public QObject
{
    Q_OBJECT
    // "accounts", "folders", "messages" or "unified".
    Q_PROPERTY(QString page READ page NOTIFY changed)
    Q_PROPERTY(int accountId READ accountId NOTIFY changed)
    Q_PROPERTY(int folderId READ folderId NOTIFY changed)
    // Until the unified inbox's folders are chosen, it shows the inboxes of all accounts, also
    // those added later.
    Q_PROPERTY(bool unifiedConfigured READ unifiedConfigured NOTIFY unifiedChanged)
    // The folders chosen for it, as "accountId/folderId".
    Q_PROPERTY(QStringList unifiedFolders READ unifiedFolders NOTIFY unifiedChanged)

public:
    explicit AppState(QObject *parent = nullptr);

    // The state.ini file, also for the other settings.
    static QString fileName();

    QString page() const { return m_page; }
    int accountId() const { return m_accountId; }
    int folderId() const { return m_folderId; }
    bool unifiedConfigured() const { return m_unifiedConfigured; }
    QStringList unifiedFolders() const { return m_unifiedFolders; }

    Q_INVOKABLE void save(const QString &page, int accountId = 0, int folderId = 0);
    Q_INVOKABLE void setUnifiedFolders(const QStringList &folders);
    // Back to the inboxes of all accounts.
    Q_INVOKABLE void resetUnifiedFolders();

signals:
    void changed();
    void unifiedChanged();

private:
    QSettings m_settings;
    QString m_page;
    int m_accountId = 0;
    int m_folderId = 0;
    bool m_unifiedConfigured = false;
    QStringList m_unifiedFolders;
};

#endif // APPSTATE_H
