// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef UNIFIEDMESSAGELISTMODEL_H
#define UNIFIEDMESSAGELISTMODEL_H

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

#include <qmailmessagelistmodel.h>

class QMailSearchAction;

// The messages of any set of folders, of any accounts, newest first: a folder, the unified inbox,
// or a search. The email plugin's model only does one folder or all inboxes, and doesn't tell
// whether a mail is starred.
//
// Has the role names, selection and count of the plugin's EmailMessageListModel. Acting on
// messages is left to EmailAgent.
class UnifiedMessageListModel : public QMailMessageListModel
{
    Q_OBJECT
    Q_PROPERTY(QVariantList folderIds READ folderIds WRITE setFolderIds NOTIFY folderIdsChanged)
    // Searches instead of showing folderIds when not empty: all mail matching everything set of
    // "text" (in the subject, sender and recipients), "body" (true to look for the text in the
    // mail's text too), "unread", "starred", "attachments" (all true or left out) and "accountId"
    // (0 or left out for all). Without text or a flag nothing matches. Trash and Junk are left
    // out. Only mail on the phone is found, and in the text only of mail whose text is downloaded.
    Q_PROPERTY(QVariantMap search READ search WRITE setSearch NOTIFY searchChanged)
    // Looking in the text of mails, which takes a while.
    Q_PROPERTY(bool searchingBodies READ searchingBodies NOTIFY searchingBodiesChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedMessageCount READ selectedMessageCount NOTIFY selectedMessageCountChanged)

public:
    enum Roles {
        IdRole = QMailMessageModelBase::MessageIdRole + 1,
        SenderDisplayNameRole,
        SenderEmailAddressRole,
        PreviewRole,
        DateTimeRole,
        TimeSectionRole,
        ReadStatusRole,
        AccountIdRole,
        FolderIdRole,
        FolderNameRole,
        HasAttachmentsRole,
        StarredRole,
        SelectedRole
    };

    explicit UnifiedMessageListModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    // Not hidden by the override below.
    using QMailMessageListModel::data;

    QVariantList folderIds() const { return m_folderIds; }
    void setFolderIds(const QVariantList &ids);
    QVariantMap search() const { return m_search; }
    void setSearch(const QVariantMap &search);
    bool searchingBodies() const { return !m_bodySearch.isNull(); }
    int count() const { return rowCount(); }
    int selectedMessageCount() const { return m_selected.size(); }

    Q_INVOKABLE void selectMessage(int index);
    Q_INVOKABLE void deselectMessage(int index);
    Q_INVOKABLE void selectAllMessages();
    Q_INVOKABLE void deselectAllMessages();
    // Message ids, to act on through EmailAgent.
    Q_INVOKABLE QVariantList selectedMessageIds() const;
    Q_INVOKABLE QVariantList unreadMessageIds() const;
    // The accounts the selected messages belong to: moving needs them all in one.
    Q_INVOKABLE QVariantList selectedAccountIds() const;

signals:
    void folderIdsChanged();
    void searchChanged();
    void searchingBodiesChanged();
    void countChanged();
    void selectedMessageCountChanged();

protected:
    QVariant data(const QMailMessageMetaData &metaData, int role) const override;

private:
    void onMessagesRemoved(const QMailMessageIdList &ids);
    void updateKey();
    void onBodiesMatched(const QMailMessageIdList &ids);
    void onBodySearchActivity();

    QVariantList m_folderIds;
    QVariantMap m_search;
    // Found in the mail's text, besides what matches the search's key.
    QMailMessageIdList m_bodyMatches;
    // What the search looks in, and what it looks for in the subject, sender and recipients.
    QMailMessageKey m_searchFilter;
    QMailMessageKey m_textKey;
    QPointer<QMailSearchAction> m_bodySearch;
    QSet<QMailMessageId> m_selected;
    mutable QHash<QMailFolderId, QString> m_folderNames;
};

#endif // UNIFIEDMESSAGELISTMODEL_H
