// SPDX-License-Identifier: AGPL-3.0-or-later
#include "unifiedmessagelistmodel.h"

#include <qmailaccount.h>
#include <qmailfolder.h>
#include <qmailserviceaction.h>
#include <qmailstore.h>
#include <qmailthread.h>

#include <algorithm>

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

    // Grouping again after changes, once a burst of them (a sync) is over.
    m_regroupTimer.setSingleShot(true);
    m_regroupTimer.setInterval(300);
    connect(&m_regroupTimer, &QTimer::timeout, this, [this]() { regroup(); });
    QMailStore *store = QMailStore::instance();
    connect(store, &QMailStore::messagesAdded, this, &UnifiedMessageListModel::onStoreChanged);
    connect(store, &QMailStore::messagesUpdated, this, &UnifiedMessageListModel::onStoreChanged);
    connect(store, &QMailStore::messagesRemoved, this, &UnifiedMessageListModel::onStoreChanged);
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
    roles.insert(ThreadIdRole, "threadId");
    roles.insert(ThreadCountRole, "threadCount");
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

void UnifiedMessageListModel::setThreaded(bool threaded)
{
    if (threaded == m_threaded)
        return;
    m_threaded = threaded;
    updateKey();
    emit threadedChanged();
}

void UnifiedMessageListModel::setHiddenIds(const QVariantList &ids)
{
    if (ids == m_hiddenIds)
        return;
    m_hiddenIds = ids;
    if (m_search.isEmpty())
        updateKey();
    emit hiddenIdsChanged();
}

namespace {

QMailMessageIdList messageIdList(const QVariantList &ids)
{
    QMailMessageIdList list;
    for (const QVariant &id : ids)
        list.append(QMailMessageId(id.toULongLong()));
    return list;
}

} // namespace

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
    m_threadedKey = QMailMessageKey();
    m_rows.clear();
    m_members.clear();
    m_unread.clear();
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
        if (!m_hiddenIds.isEmpty())
            key &= QMailMessageKey::id(messageIdList(m_hiddenIds), QMailDataComparator::Excludes);
        if (folders.isEmpty()) {
            setKey(QMailMessageKey::nonMatchingKey());
        } else if (m_threaded) {
            m_threadedKey = key;
            regroup(true);
        } else {
            setKey(key);
        }
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
    if (m_search.contains(QStringLiteral("thread"))) {
        filter &= QMailMessageKey::parentThreadId(QMailThreadId(m_search.value(QStringLiteral("thread")).toULongLong()));
        anything = true;
    }
    if (m_search.contains(QStringLiteral("ids"))) {
        const QVariantList ids = m_search.value(QStringLiteral("ids")).toList();
        filter &= ids.isEmpty() ? QMailMessageKey::nonMatchingKey() : QMailMessageKey::id(messageIdList(ids));
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
        if (m_members.contains(metaData.id())) {
            for (const QMailMessageId &id : m_members.value(metaData.id())) {
                if (m_unread.contains(id))
                    return false;
            }
            return true;
        }
        return (metaData.status() & QMailMessage::Read) != 0;
    case ThreadIdRole:
        return int(metaData.parentThreadId().toULongLong());
    case ThreadCountRole: {
        const QMailThreadId thread = metaData.parentThreadId();
        if (!thread.isValid())
            return 1;
        auto size = m_threadSizes.constFind(thread);
        if (size == m_threadSizes.constEnd())
            size = m_threadSizes.insert(thread, qMax(1, int(QMailThread(thread).messageCount())));
        return *size;
    }
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
    for (const QMailMessageId &id : m_selected) {
        for (const QMailMessageId &member : membersOf(id))
            ids.append(int(member.toULongLong()));
    }
    return ids;
}

QVariantList UnifiedMessageListModel::messageIdsOf(int messageId) const
{
    QVariantList ids;
    for (const QMailMessageId &id : membersOf(QMailMessageId(messageId)))
        ids.append(int(id.toULongLong()));
    return ids;
}

QMailMessageIdList UnifiedMessageListModel::membersOf(const QMailMessageId &id) const
{
    const auto members = m_members.constFind(id);
    return members != m_members.constEnd() ? *members : QMailMessageIdList() << id;
}

void UnifiedMessageListModel::onStoreChanged()
{
    if (!m_threadedKey.isEmpty())
        m_regroupTimer.start();
}

void UnifiedMessageListModel::regroup(bool force)
{
    if (m_threadedKey.isEmpty())
        return;
    QMailMessageMetaDataList messages = QMailStore::instance()->messagesMetaData(
                m_threadedKey, QMailMessageKey::Id | QMailMessageKey::ParentThreadId
                | QMailMessageKey::Status | QMailMessageKey::TimeStamp);
    std::sort(messages.begin(), messages.end(), [](const QMailMessageMetaData &a, const QMailMessageMetaData &b) {
        return a.date().toUTC() > b.date().toUTC();
    });

    // The newest message of each conversation is its row.
    QMailMessageIdList rows;
    QHash<QMailThreadId, QMailMessageId> newest;
    m_members.clear();
    m_unread.clear();
    m_threadSizes.clear();
    for (const QMailMessageMetaData &message : messages) {
        const QMailMessageId id = message.id();
        if (!(message.status() & QMailMessage::Read))
            m_unread.insert(id);
        const QMailThreadId thread = message.parentThreadId();
        const auto row = thread.isValid() ? newest.constFind(thread) : newest.constEnd();
        if (row != newest.constEnd()) {
            m_members[*row].append(id);
            continue;
        }
        if (thread.isValid())
            newest.insert(thread, id);
        rows.append(id);
        m_members.insert(id, QMailMessageIdList() << id);
    }

    // Setting the key resets the list, so only when the rows changed; otherwise only how they look.
    if (force || rows != m_rows) {
        m_rows = rows;
        setKey(rows.isEmpty() ? QMailMessageKey::nonMatchingKey() : QMailMessageKey::id(rows));
    } else if (rowCount() > 0) {
        emit dataChanged(index(0), index(rowCount() - 1), QVector<int>() << ReadStatusRole << ThreadCountRole);
    }
}

QVariantList UnifiedMessageListModel::unreadMessageIds() const
{
    QVariantList ids;
    for (int row = 0; row < rowCount(); ++row) {
        const QModelIndex idx = index(row);
        if (data(idx, ReadStatusRole).toBool())
            continue;
        // A conversation's unread messages, or the row's own.
        const QMailMessageId rowId = idFromIndex(idx);
        for (const QMailMessageId &id : membersOf(rowId)) {
            if (!m_members.contains(rowId) || m_unread.contains(id))
                ids.append(int(id.toULongLong()));
        }
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
