// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef APPSTATE_H
#define APPSTATE_H

#include <QObject>
#include <QSettings>

// Where the app was left, so it can open there again: the account overview, an account's
// folders, or a folder's messages. And which folders the unified inbox shows, and which are
// favourites. As the AppState singleton.
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
    // Folders shown on the account overview, as "accountId/folderId", in the order added.
    Q_PROPERTY(QStringList favouriteFolders READ favouriteFolders NOTIFY favouritesChanged)
    // Message lists with one row per conversation; on unless switched off in Settings.
    Q_PROPERTY(bool conversations READ conversations WRITE setConversations NOTIFY conversationsChanged)
    // New mail is signed with PGP when there's a key for its From address; on unless switched off.
    Q_PROPERTY(bool signByDefault READ signByDefault WRITE setSignByDefault NOTIFY signByDefaultChanged)
    // Signed mail carries one's public key as an attachment; off unless switched on.
    Q_PROPERTY(bool attachPublicKey READ attachPublicKey WRITE setAttachPublicKey NOTIFY attachPublicKeyChanged)

public:
    explicit AppState(QObject *parent = nullptr);

    // The state.ini file, also for the other settings.
    static QString fileName();

    QString page() const { return m_page; }
    int accountId() const { return m_accountId; }
    int folderId() const { return m_folderId; }
    bool unifiedConfigured() const { return m_unifiedConfigured; }
    QStringList unifiedFolders() const { return m_unifiedFolders; }
    QStringList favouriteFolders() const { return m_favourites; }
    bool conversations() const { return m_conversations; }
    void setConversations(bool conversations);
    bool signByDefault() const { return m_signByDefault; }
    void setSignByDefault(bool sign);
    bool attachPublicKey() const { return m_attachPublicKey; }
    void setAttachPublicKey(bool attach);

    Q_INVOKABLE void save(const QString &page, int accountId = 0, int folderId = 0);
    Q_INVOKABLE void setUnifiedFolders(const QStringList &folders);
    // Back to the inboxes of all accounts.
    Q_INVOKABLE void resetUnifiedFolders();
    Q_INVOKABLE bool isFavourite(int accountId, int folderId) const;
    Q_INVOKABLE void setFavourite(int accountId, int folderId, bool favourite);

signals:
    void changed();
    void unifiedChanged();
    void favouritesChanged();
    void conversationsChanged();
    void signByDefaultChanged();
    void attachPublicKeyChanged();

private:
    QSettings m_settings;
    QString m_page;
    int m_accountId = 0;
    int m_folderId = 0;
    bool m_unifiedConfigured = false;
    QStringList m_unifiedFolders;
    QStringList m_favourites;
    bool m_conversations = true;
    bool m_signByDefault = true;
    bool m_attachPublicKey = false;
};

#endif // APPSTATE_H
