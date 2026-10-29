// SPDX-License-Identifier: AGPL-3.0-or-later
#include "unifiedmessagelistmodel.h"

#include <qmailfolder.h>
#include <qmailstore.h>

UnifiedMessageListModel::UnifiedMessageListModel(QObject *parent)
    : QMailMessageListModel(parent)
{
    setSortKey(QMailMessageSortKey::timeStamp(Qt::DescendingOrder));

    connect(this, &QAbstractItemModel::rowsInserted, this, &UnifiedMessageListModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &UnifiedMessageListModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &UnifiedMessageListModel::countChanged);
    connect(QMailStore::instance(), &QMailStore::messagesRemoved,
            this, &UnifiedMessageListModel::onMessagesRemoved);
}

QHash<int, QByteArray> UnifiedMessageListModel::roleNames() const
{
    // Named as EmailMessageListModel names them.
    QHash<int, QByteArray> roles;
    roles.insert(MessageSubjectTextRole, "subject");
    roles.insert(IdRole, "messageId");
    roles.insert(SenderDisplayNameRole, "senderDisplayName");
    roles.insert(SenderEmailAddressRole, "senderEmailAddress");
    roles.insert(PreviewRole, "preview");
    roles.insert(DateTimeRole, "qDateTime");
    roles.insert(TimeSectionRole, "timeSection");
    roles.insert(ReadStatusRole, "readStatus");
    roles.insert(AccountIdRole, "accountId");
    roles.insert(FolderIdRole, "folderId");
    roles.insert(FolderNameRole, "folderName");
    roles.insert(HasAttachmentsRole, "hasAttachments");
    roles.insert(SelectedRole, "selected");
    return roles;
}

void UnifiedMessageListModel::setFolderIds(const QVariantList &ids)
{
    if (ids == m_folderIds)
        return;
    m_folderIds = ids;

    QMailFolderIdList folders;
    for (const QVariant &id : ids) {
        const QMailFolderId folder(id.toULongLong());
        if (folder.isValid())
            folders.append(folder);
    }
    m_selected.clear();
    setKey(folders.isEmpty() ? QMailMessageKey::nonMatchingKey()
                             : QMailMessageKey::parentFolderId(folders));
    emit folderIdsChanged();
    emit selectedMessageCountChanged();
}

QVariant UnifiedMessageListModel::data(const QMailMessageMetaData &metaData, int role) const
{
    switch (role) {
    case IdRole:
        return int(metaData.id().toULongLong());
    case SenderDisplayNameRole:
        return metaData.from().name().isEmpty() ? metaData.from().address() : metaData.from().name();
    case SenderEmailAddressRole:
        return metaData.from().address();
    case PreviewRole:
        return metaData.preview().simplified();
    case DateTimeRole:
        return metaData.date().toLocalTime();
    case TimeSectionRole:
        return metaData.date().toLocalTime().date();
    case ReadStatusRole:
        return (metaData.status() & QMailMessage::Read) != 0;
    case AccountIdRole:
        return int(metaData.parentAccountId().toULongLong());
    case FolderIdRole:
        return int(metaData.parentFolderId().toULongLong());
    case FolderNameRole: {
        const QMailFolderId folder = metaData.parentFolderId();
        if (!m_folderNames.contains(folder))
            m_folderNames.insert(folder, QMailFolder(folder).displayName());
        return m_folderNames.value(folder);
    }
    case HasAttachmentsRole:
        return (metaData.status() & QMailMessage::HasAttachments) != 0;
    case SelectedRole:
        return m_selected.contains(metaData.id());
    default:
        return QMailMessageListModel::data(metaData, role);
    }
}

void UnifiedMessageListModel::selectMessage(int row)
{
    const QModelIndex idx = index(row);
    const QMailMessageId id = idFromIndex(idx);
    if (id.isValid() && !m_selected.contains(id)) {
        m_selected.insert(id);
        emit dataChanged(idx, idx, QVector<int>() << SelectedRole);
        emit selectedMessageCountChanged();
    }
}

void UnifiedMessageListModel::deselectMessage(int row)
{
    const QModelIndex idx = index(row);
    if (m_selected.remove(idFromIndex(idx))) {
        emit dataChanged(idx, idx, QVector<int>() << SelectedRole);
        emit selectedMessageCountChanged();
    }
}

void UnifiedMessageListModel::selectAllMessages()
{
    for (int row = 0; row < rowCount(); ++row)
        m_selected.insert(idFromIndex(index(row)));
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1), QVector<int>() << SelectedRole);
    emit selectedMessageCountChanged();
}

void UnifiedMessageListModel::deselectAllMessages()
{
    if (m_selected.isEmpty())
        return;
    m_selected.clear();
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1), QVector<int>() << SelectedRole);
    emit selectedMessageCountChanged();
}

QVariantList UnifiedMessageListModel::selectedMessageIds() const
{
    QVariantList ids;
    for (const QMailMessageId &id : m_selected)
        ids.append(int(id.toULongLong()));
    return ids;
}

QVariantList UnifiedMessageListModel::unreadMessageIds() const
{
    QVariantList ids;
    for (int row = 0; row < rowCount(); ++row) {
        const QModelIndex idx = index(row);
        if (!data(idx, ReadStatusRole).toBool())
            ids.append(int(idFromIndex(idx).toULongLong()));
    }
    return ids;
}

QVariantList UnifiedMessageListModel::selectedAccountIds() const
{
    QVariantList accounts;
    for (const QMailMessageId &id : m_selected) {
        const int account = int(QMailMessageMetaData(id).parentAccountId().toULongLong());
        if (!accounts.contains(account))
            accounts.append(account);
    }
    return accounts;
}

void UnifiedMessageListModel::onMessagesRemoved(const QMailMessageIdList &ids)
{
    bool changed = false;
    for (const QMailMessageId &id : ids)
        changed = m_selected.remove(id) || changed;
    if (changed)
        emit selectedMessageCountChanged();
}
