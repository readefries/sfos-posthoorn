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
    // How message lists are ordered: 0 newest first, 1 by sender A to Z, 2 largest first, 3 oldest
    // first, 4 by sender Z to A (see UnifiedMessageListModel::SortOrder).
    Q_PROPERTY(int sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)
    // Starred mail at the top of message lists; off unless switched on.
    Q_PROPERTY(bool starredFirst READ starredFirst WRITE setStarredFirst NOTIFY starredFirstChanged)
    // Unread mail at the top of message lists (below starred mail with starredFirst).
    Q_PROPERTY(bool unreadFirst READ unreadFirst WRITE setUnreadFirst NOTIFY unreadFirstChanged)
    // Accounts whose new mail is announced by no notification, as account ids. The messageserver
    // reads them too, with the posthoorn-notify-filter plugin (notifyfilter/).
    Q_PROPERTY(QStringList mutedAccounts READ mutedAccounts NOTIFY mutedAccountsChanged)
    // Whether that plugin is installed: without it, muting an account does nothing.
    Q_PROPERTY(bool notifyFilterInstalled READ notifyFilterInstalled CONSTANT)

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
    int sortOrder() const { return m_sortOrder; }
    void setSortOrder(int order);
    bool starredFirst() const { return m_starredFirst; }
    void setStarredFirst(bool first);
    bool unreadFirst() const { return m_unreadFirst; }
    void setUnreadFirst(bool first);
    QStringList mutedAccounts() const { return m_mutedAccounts; }
    bool notifyFilterInstalled() const;

    Q_INVOKABLE void save(const QString &page, int accountId = 0, int folderId = 0);
    Q_INVOKABLE void setUnifiedFolders(const QStringList &folders);
    // Back to the inboxes of all accounts.
    Q_INVOKABLE void resetUnifiedFolders();
    Q_INVOKABLE bool isFavourite(int accountId, int folderId) const;
    Q_INVOKABLE void setFavourite(int accountId, int folderId, bool favourite);
    Q_INVOKABLE bool isMuted(int accountId) const;
    Q_INVOKABLE void setMuted(int accountId, bool muted);

signals:
    void changed();
    void unifiedChanged();
    void favouritesChanged();
    void conversationsChanged();
    void signByDefaultChanged();
    void attachPublicKeyChanged();
    void sortOrderChanged();
    void starredFirstChanged();
    void unreadFirstChanged();
    void mutedAccountsChanged();

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
    int m_sortOrder = 0;
    bool m_starredFirst = false;
    bool m_unreadFirst = false;
    QStringList m_mutedAccounts;
};

#endif // APPSTATE_H
