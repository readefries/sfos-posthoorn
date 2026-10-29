// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef UNIFIEDMESSAGELISTMODEL_H
#define UNIFIEDMESSAGELISTMODEL_H

#include <QHash>
#include <QSet>
#include <QVariantList>

#include <qmailmessagelistmodel.h>

// The messages of any set of folders, of any accounts, newest first: the unified inbox. The email
// plugin's model only does one folder or all inboxes.
//
// Has the role names, selection and count of the plugin's EmailMessageListModel that the message
// list uses, so the same delegate works for both. Acting on messages is left to EmailAgent.
class UnifiedMessageListModel : public QMailMessageListModel
{
    Q_OBJECT
    Q_PROPERTY(QVariantList folderIds READ folderIds WRITE setFolderIds NOTIFY folderIdsChanged)
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
        SelectedRole
    };

    explicit UnifiedMessageListModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    // Not hidden by the override below.
    using QMailMessageListModel::data;

    QVariantList folderIds() const { return m_folderIds; }
    void setFolderIds(const QVariantList &ids);
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
    void countChanged();
    void selectedMessageCountChanged();

protected:
    QVariant data(const QMailMessageMetaData &metaData, int role) const override;

private:
    void onMessagesRemoved(const QMailMessageIdList &ids);

    QVariantList m_folderIds;
    QSet<QMailMessageId> m_selected;
    mutable QHash<QMailFolderId, QString> m_folderNames;
};

#endif // UNIFIEDMESSAGELISTMODEL_H
