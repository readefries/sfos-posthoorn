// SPDX-License-Identifier: AGPL-3.0-or-later
#include "unifiedmessagelistmodel.h"

#include <qmailaccount.h>
#include <qmailfolder.h>
#include <qmailserviceaction.h>
#include <qmailstore.h>

UnifiedMessageListModel::UnifiedMessageListModel(QObject *parent)
    : QMailMessageListModel(parent)
{
    setSortKey(QMailMessageSortKey::timeStamp(Qt::DescendingOrder));

    connect(this, &QAbstractItemModel::rowsInserted, this, &UnifiedMessageListModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &UnifiedMessageListModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &UnifiedMessageListModel::countChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this, &UnifiedMessageListModel::unreadCountChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &UnifiedMessageListModel::unreadCountChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &UnifiedMessageListModel::unreadCountChanged);
    connect(this, &QAbstractItemModel::dataChanged, this, &UnifiedMessageListModel::unreadCountChanged);
    connect(QMailStore::instance(), &QMailStore::messagesRemoved,
            this, &UnifiedMessageListModel::onMessagesRemoved);
}

UnifiedMessageListModel::~UnifiedMessageListModel()
{
    cancelSearches();
    // What the servers found was only stored for showing it here.
    if (m_hadServerResults)
        QMailStore::instance()->removeMessages(QMailSearchAction::temporaryKey(), QMailStore::NoRemovalRecord);
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
    roles.insert(StarredRole, "starred");
    roles.insert(SelectedRole, "selected");
    return roles;
}

void UnifiedMessageListModel::setFolderIds(const QVariantList &ids)
{
    if (ids == m_folderIds)
        return;
    m_folderIds = ids;
    updateKey();
    emit folderIdsChanged();
}

void UnifiedMessageListModel::setUnreadOnly(bool unreadOnly)
{
    if (unreadOnly == m_unreadOnly)
        return;
    m_unreadOnly = unreadOnly;
    updateKey();
    emit unreadOnlyChanged();
}

void UnifiedMessageListModel::setSearch(const QVariantMap &search)
{
    if (search == m_search)
        return;
    m_search = search;
    updateKey();
    emit searchChanged();
}

void UnifiedMessageListModel::cancelSearches()
{
    if (m_bodySearch) {
        m_bodySearch->cancelOperation();
        m_bodySearch->deleteLater();
        m_bodySearch.clear();
        emit searchingBodiesChanged();
    }
    const bool serverSearch = m_serverSearched || !m_serverSearches.isEmpty();
    for (QMailSearchAction *search : m_serverSearches) {
        search->cancelOperation();
        search->deleteLater();
    }
    m_serverSearches.clear();
    m_serverSearched = false;
    if (serverSearch)
        emit serverSearchChanged();
}

void UnifiedMessageListModel::updateKey()
{
    cancelSearches();
    m_matches.clear();
    m_serverKey = QMailMessageKey::nonMatchingKey();
    m_selected.clear();
    emit selectedMessageCountChanged();

    // Deleted, but not gone from the store yet.
    const QMailMessageKey present = QMailMessageKey::status(QMailMessage::Removed, QMailDataComparator::Excludes);

    if (m_search.isEmpty()) {
        QMailFolderIdList folders;
        for (const QVariant &id : m_folderIds) {
            const QMailFolderId folder(id.toULongLong());
            if (folder.isValid())
                folders.append(folder);
        }
        QMailMessageKey key = QMailMessageKey::parentFolderId(folders) & present
                // Not what a server search found, which is only for the search.
                & ~QMailSearchAction::temporaryKey();
        if (m_unreadOnly)
            key &= QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes);
        setKey(folders.isEmpty() ? QMailMessageKey::nonMatchingKey() : key);
        return;
    }

    const QString text = m_search.value(QStringLiteral("text")).toString().simplified();
    bool anything = !text.isEmpty();
    QMailMessageKey filter = present;
    // The flags a server can search on too.
    QMailMessageKey flags;
    if (m_search.value(QStringLiteral("unread")).toBool()) {
        flags &= QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes);
        anything = true;
    }
    if (m_search.value(QStringLiteral("starred")).toBool()) {
        flags &= QMailMessageKey::status(QMailMessage::Important, QMailDataComparator::Includes);
        anything = true;
    }
    filter &= flags;
    if (m_search.value(QStringLiteral("attachments")).toBool()) {
        filter &= QMailMessageKey::status(QMailMessage::HasAttachments, QMailDataComparator::Includes);
        anything = true;
    }
    if (!anything) {
        setKey(QMailMessageKey::nonMatchingKey());
        return;
    }
    const int accountId = m_search.value(QStringLiteral("accountId")).toInt();
    if (accountId > 0)
        filter &= QMailMessageKey::parentAccountId(QMailAccountId(accountId));
    QMailFolderIdList skipped;
    for (const QMailAccountId &id : QMailStore::instance()->queryAccounts()) {
        const QMailAccount account(id);
        for (const QMailFolder::StandardFolder folder : { QMailFolder::TrashFolder, QMailFolder::JunkFolder }) {
            const QMailFolderId folderId = account.standardFolder(folder);
            if (folderId.isValid())
                skipped.append(folderId);
        }
    }
    if (!skipped.isEmpty())
        filter &= QMailMessageKey::parentFolderId(skipped, QMailDataComparator::Excludes);

    // Every word somewhere in the subject, sender or recipients.
    QMailMessageKey textKey;
    for (const QString &word : text.split(QLatin1Char(' '), QString::SkipEmptyParts)) {
        textKey &= QMailMessageKey::subject(word, QMailDataComparator::Includes)
                | QMailMessageKey::sender(word, QMailDataComparator::Includes)
                | QMailMessageKey::recipients(word, QMailDataComparator::Includes);
    }
    m_searchFilter = filter;
    m_textKey = textKey;
    // Not for attachments alone: servers can't search on those, and would send everything.
    m_serverKey = textKey.isEmpty() && flags.isEmpty() ? QMailMessageKey::nonMatchingKey() : textKey & flags;
    if (accountId > 0)
        m_serverKey &= QMailMessageKey::parentAccountId(QMailAccountId(accountId));
    setKey(textKey.isEmpty() ? filter : filter & textKey);

    const bool body = !text.isEmpty() && m_search.value(QStringLiteral("body")).toBool();
    m_bodyText = body ? text : QString();
    if (body) {
        // The messageserver looks through the text of the mails on the phone, and reports what it
        // finds as it goes.
        m_bodySearch = new QMailSearchAction(this);
        connect(m_bodySearch.data(), &QMailSearchAction::messageIdsMatched,
                this, &UnifiedMessageListModel::onMatched);
        connect(m_bodySearch.data(), &QMailServiceAction::activityChanged,
                this, &UnifiedMessageListModel::onBodySearchActivity);
        m_bodySearch->searchMessages(filter, text, QMailSearchAction::Local);
        emit searchingBodiesChanged();
    }
}

void UnifiedMessageListModel::onMatched(const QMailMessageIdList &ids)
{
    if (ids.isEmpty())
        return;
    m_matches += ids;
    const QMailMessageKey found = QMailMessageKey::id(m_matches);
    setKey(m_searchFilter & (m_textKey.isEmpty() ? found : m_textKey | found));
}

void UnifiedMessageListModel::searchServer()
{
    if (m_search.isEmpty() || m_serverKey.isNonMatching() || !m_serverSearches.isEmpty())
        return;
    const int accountId = m_search.value(QStringLiteral("accountId")).toInt();
    const QMailAccountIdList accounts = accountId > 0 ? QMailAccountIdList() << QMailAccountId(accountId)
                                                      : QMailStore::instance()->queryAccounts(
                                                            QMailAccountKey::messageType(QMailMessage::Email));
    for (const QMailAccountId &account : accounts) {
        QMailSearchAction *search = new QMailSearchAction(this);
        connect(search, &QMailSearchAction::messageIdsMatched, this, [this](const QMailMessageIdList &ids) {
            m_hadServerResults = m_hadServerResults || !ids.isEmpty();
            onMatched(ids);
        });
        connect(search, &QMailServiceAction::activityChanged, this, [this, search]() {
            onServerSearchActivity(search);
        });
        m_serverSearches.append(search);
        search->searchMessages(m_serverKey & QMailMessageKey::parentAccountId(account), m_bodyText,
                               QMailSearchAction::Remote, 100,
                               QMailMessageSortKey::timeStamp(Qt::DescendingOrder));
    }
    m_serverSearched = true;
    emit serverSearchChanged();
}

void UnifiedMessageListModel::onServerSearchActivity(QMailSearchAction *search)
{
    const QMailServiceAction::Activity activity = search->activity();
    if (activity != QMailServiceAction::Successful && activity != QMailServiceAction::Failed)
        return;
    if (m_serverSearches.removeOne(search)) {
        search->deleteLater();
        emit serverSearchChanged();
    }
}

void UnifiedMessageListModel::onBodySearchActivity()
{
    if (!m_bodySearch)
        return;
    const QMailServiceAction::Activity activity = m_bodySearch->activity();
    if (activity == QMailServiceAction::Successful || activity == QMailServiceAction::Failed) {
        m_bodySearch->deleteLater();
        m_bodySearch.clear();
        emit searchingBodiesChanged();
    }
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
    case StarredRole:
        return (metaData.status() & QMailMessage::Important) != 0;
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
