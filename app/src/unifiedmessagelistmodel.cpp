// SPDX-License-Identifier: AGPL-3.0-or-later
#include "unifiedmessagelistmodel.h"

#include <qmailaccount.h>
#include <qmailfolder.h>
#include <qmailserviceaction.h>
#include <qmailstore.h>
#include <qmailthread.h>

#include <algorithm>

// QmfClient doesn't export QMailMessageModelImplementation (no QMF_EXPORT), so its destructor,
// which is its key function, isn't there for us to link to, and nor are its vtable and typeinfo.
// Defining the (empty) destructor here makes the compiler emit them in this binary; the library's
// own copies are hidden, so they don't clash.
QMailMessageModelImplementation::~QMailMessageModelImplementation()
{
}

// The rows: the messages of key, ordered by UnifiedMessageListModel::orderedIds(), loaded when
// first asked for. Changes from the store are handled by the model (reorder(), removeRows()).
class UnifiedMessageListModel::Rows : public QMailMessageModelImplementation
{
public:
    explicit Rows(UnifiedMessageListModel &model) : m_model(model) {}

    QMailMessageKey key() const override { return m_key; }
    void setKey(const QMailMessageKey &key) override { m_key = key; }
    // Unused: the order is the model's.
    QMailMessageSortKey sortKey() const override { return m_sortKey; }
    void setSortKey(const QMailMessageSortKey &sortKey) override { m_sortKey = sortKey; }
    uint limit() const override { return 0; }
    void setLimit(uint) override {}
    int totalCount() const override { return ids().size(); }
    bool isEmpty() const override { return ids().isEmpty(); }
    int rowCount(const QModelIndex &idx) const override { return idx.isValid() ? 0 : ids().size(); }
    int columnCount(const QModelIndex &) const override { return 1; }

    QMailMessageId idFromIndex(const QModelIndex &index) const override
    {
        const QMailMessageIdList &list = ids();
        return index.isValid() && index.row() >= 0 && index.row() < list.size() ? list.at(index.row())
                                                                                : QMailMessageId();
    }

    QModelIndex indexFromId(const QMailMessageId &id) const override
    {
        const int row = rowOf(id);
        return row >= 0 ? m_model.index(row) : QModelIndex();
    }

    Qt::CheckState checkState(const QModelIndex &) const override { return Qt::Unchecked; }
    void setCheckState(const QModelIndex &, Qt::CheckState) override {}
    void reset() override { m_loaded = false; }
    bool ignoreMailStoreUpdates() const override { return false; }
    bool setIgnoreMailStoreUpdates(bool) override { return false; }

    bool processMessagesAdded(const QMailMessageIdList &ids) override
    {
        if (m_loaded && !m_key.isNonMatching()
                && QMailStore::instance()->countMessages(m_key & QMailMessageKey::id(ids)) > 0)
            m_model.m_refreshTimer.start();
        return true;
    }

    bool processMessagesUpdated(const QMailMessageIdList &ids) override
    {
        if (!m_loaded || m_key.isNonMatching())
            return true;
        bool relevant = false;
        for (const QMailMessageId &id : ids) {
            if (rowOf(id) >= 0) {
                m_model.m_changed.insert(id);
                relevant = true;
            }
        }
        // Or mail that's now part of the list, as one marked unread is for the cover.
        if (relevant || QMailStore::instance()->countMessages(m_key & QMailMessageKey::id(ids)) > 0)
            m_model.m_refreshTimer.start();
        return true;
    }

    bool processMessagesRemoved(const QMailMessageIdList &ids) override
    {
        if (m_loaded)
            m_model.removeRows(ids);
        return true;
    }

    const QMailMessageIdList &ids() const
    {
        if (!m_loaded) {
            m_loaded = true;
            m_ids = m_key.isNonMatching() ? QMailMessageIdList() : m_model.orderedIds(m_key);
            m_rowsDirty = true;
        }
        return m_ids;
    }

    int rowOf(const QMailMessageId &id) const
    {
        const QMailMessageIdList &list = ids();
        if (m_rowsDirty) {
            m_rows.clear();
            m_rows.reserve(list.size());
            for (int row = 0; row < list.size(); ++row)
                m_rows.insert(list.at(row), row);
            m_rowsDirty = false;
        }
        return m_rows.value(id, -1);
    }

    bool loaded() const { return m_loaded; }

    // For the model's changes, after which rowsChanged(); ids() is loaded.
    QMailMessageIdList &list()
    {
        m_rowsDirty = true;
        return m_ids;
    }
    // A view may have looked up rows while they moved.
    void rowsChanged() { m_rowsDirty = true; }

private:
    UnifiedMessageListModel &m_model;
    QMailMessageKey m_key = QMailMessageKey::nonMatchingKey();
    QMailMessageSortKey m_sortKey = QMailMessageSortKey::id();
    mutable bool m_loaded = false;
    mutable QMailMessageIdList m_ids;
    mutable QHash<QMailMessageId, int> m_rows;
    mutable bool m_rowsDirty = true;
};

UnifiedMessageListModel::UnifiedMessageListModel(QObject *parent)
    : QMailMessageModelBase(parent)
    , m_list(new Rows(*this))
{

    connect(this, &QAbstractItemModel::rowsInserted, this, &UnifiedMessageListModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &UnifiedMessageListModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &UnifiedMessageListModel::countChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this, &UnifiedMessageListModel::unreadCountChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &UnifiedMessageListModel::unreadCountChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &UnifiedMessageListModel::unreadCountChanged);
    connect(this, &QAbstractItemModel::dataChanged, this, &UnifiedMessageListModel::unreadCountChanged);
    connect(QMailStore::instance(), &QMailStore::messagesRemoved,
            this, &UnifiedMessageListModel::onMessagesRemoved);

    // After changes, once a burst of them is over: the rows put in order and drawn again.
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(100);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() { reorder(); });

    // Grouping again after changes, once a burst of them (a sync) is over.
    m_regroupTimer.setSingleShot(true);
    m_regroupTimer.setInterval(300);
    connect(&m_regroupTimer, &QTimer::timeout, this, [this]() { regroup(); });
    QMailStore *store = QMailStore::instance();
    connect(store, &QMailStore::messagesAdded, this, &UnifiedMessageListModel::onStoreChanged);
    connect(store, &QMailStore::messagesUpdated, this, &UnifiedMessageListModel::onStoreChanged);
    connect(store, &QMailStore::messagesRemoved, this, &UnifiedMessageListModel::onStoreChanged);
}

QModelIndex UnifiedMessageListModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || column != 0 || row < 0 || row >= rowCount())
        return QModelIndex();
    return createIndex(row, column);
}

QModelIndex UnifiedMessageListModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

QModelIndex UnifiedMessageListModel::generateIndex(int row, int column, void *ptr)
{
    return createIndex(row, column, ptr);
}

QMailMessageModelImplementation *UnifiedMessageListModel::impl()
{
    return m_list.get();
}

const QMailMessageModelImplementation *UnifiedMessageListModel::impl() const
{
    return m_list.get();
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
    roles.insert(SortSectionRole, "sortSection");
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

void UnifiedMessageListModel::setSortOrder(SortOrder order)
{
    if (order == m_sortOrder)
        return;
    m_sortOrder = order;
    resort();
    emit sortOrderChanged();
}

void UnifiedMessageListModel::setStarredFirst(bool first)
{
    if (first == m_starredFirst)
        return;
    m_starredFirst = first;
    resort();
    emit starredFirstChanged();
}

void UnifiedMessageListModel::setUnreadFirst(bool first)
{
    if (first == m_unreadFirst)
        return;
    m_unreadFirst = first;
    resort();
    emit unreadFirstChanged();
}

void UnifiedMessageListModel::resort()
{
    if (m_list->loaded())
        fullRefresh(false);
}

bool UnifiedMessageListModel::isUnread(const QMailMessageId &id, quint64 status) const
{
    const auto members = m_members.constFind(id);
    if (members == m_members.constEnd())
        return !(status & QMailMessage::Read);
    for (const QMailMessageId &member : *members) {
        if (m_unread.contains(member))
            return true;
    }
    return false;
}

QMailMessageIdList UnifiedMessageListModel::orderedIds(const QMailMessageKey &key) const
{
    const bool bySender = m_sortOrder == BySender || m_sortOrder == BySenderDescending;
    QMailMessageKey::Properties properties = QMailMessageKey::Id | QMailMessageKey::Status
            | QMailMessageKey::TimeStamp;
    if (bySender)
        properties |= QMailMessageKey::Sender;
    if (m_sortOrder == BySize)
        properties |= QMailMessageKey::Size;
    const QMailMessageMetaDataList messages = QMailStore::instance()->messagesMetaData(key, properties);

    struct Row {
        QMailMessageId id;
        bool starred;
        bool unread;
        // The name shown, as compared (case aside) and as shown.
        QString senderKey;
        QString sender;
        QDateTime date;
        uint size;
    };
    QVector<Row> rows;
    rows.reserve(messages.size());
    for (const QMailMessageMetaData &message : messages) {
        Row row;
        row.id = message.id();
        row.starred = message.status() & QMailMessage::Important;
        row.unread = isUnread(row.id, message.status());
        if (bySender) {
            const QMailAddress from = message.from();
            row.sender = from.name().isEmpty() ? from.address() : from.name();
            row.senderKey = row.sender.toCaseFolded();
        }
        row.date = message.date().toUTC();
        row.size = message.size();
        rows.append(row);
    }

    const SortOrder order = m_sortOrder;
    const bool starredFirst = m_starredFirst;
    const bool unreadFirst = m_unreadFirst;
    std::sort(rows.begin(), rows.end(), [=](const Row &a, const Row &b) {
        if (starredFirst && a.starred != b.starred)
            return a.starred;
        if (unreadFirst && a.unread != b.unread)
            return a.unread;
        if (bySender) {
            // The same name spelt the same stays together, so each gets one section.
            int c = QString::localeAwareCompare(a.senderKey, b.senderKey);
            if (c == 0)
                c = QString::compare(a.sender, b.sender);
            if (c != 0)
                return order == BySender ? c < 0 : c > 0;
        } else if (order == BySize && a.size != b.size) {
            return a.size > b.size;
        }
        if (a.date != b.date)
            return order == OldestFirst ? a.date < b.date : a.date > b.date;
        return a.id.toULongLong() > b.id.toULongLong();
    });

    QMailMessageIdList ids;
    ids.reserve(rows.size());
    for (const Row &row : rows)
        ids.append(row.id);
    return ids;
}

void UnifiedMessageListModel::reorder(bool allChanged)
{
    m_refreshTimer.stop();
    if (!m_list->loaded()) {
        m_changed.clear();
        return;
    }
    const QMailMessageIdList target = m_list->key().isNonMatching() ? QMailMessageIdList()
                                                                     : orderedIds(m_list->key());
    const QSet<QMailMessageId> wanted = target.toSet();
    QMailMessageIdList &rows = m_list->list();

    // What's gone, from the end so the rows before keep their place.
    for (int row = rows.size() - 1; row >= 0; --row) {
        if (!wanted.contains(rows.at(row))) {
            beginRemoveRows(QModelIndex(), row, row);
            rows.removeAt(row);
            endRemoveRows();
        }
    }
    // Then from the top: each row moved up to its place, or added there.
    for (int row = 0; row < target.size(); ++row) {
        const QMailMessageId &id = target.at(row);
        if (row < rows.size() && rows.at(row) == id)
            continue;
        const int from = rows.indexOf(id, row);
        if (from < 0) {
            beginInsertRows(QModelIndex(), row, row);
            rows.insert(row, id);
            endInsertRows();
        } else {
            beginMoveRows(QModelIndex(), from, from, QModelIndex(), row);
            rows.move(from, row);
            endMoveRows();
        }
    }
    m_list->rowsChanged();

    if (allChanged) {
        if (!rows.isEmpty())
            emit dataChanged(index(0), index(rows.size() - 1));
    } else {
        for (const QMailMessageId &id : m_changed) {
            const int row = m_list->rowOf(id);
            if (row >= 0)
                emit dataChanged(index(row), index(row));
        }
    }
    m_changed.clear();
}

void UnifiedMessageListModel::removeRows(const QMailMessageIdList &ids)
{
    QList<int> gone;
    for (const QMailMessageId &id : ids) {
        const int row = m_list->rowOf(id);
        if (row >= 0)
            gone.append(row);
    }
    std::sort(gone.begin(), gone.end());
    QMailMessageIdList &rows = m_list->list();
    for (int i = gone.size() - 1; i >= 0; --i) {
        beginRemoveRows(QModelIndex(), gone.at(i), gone.at(i));
        rows.removeAt(gone.at(i));
        endRemoveRows();
    }
    m_list->rowsChanged();
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
    case SortSectionRole:
        if (m_starredFirst && (metaData.status() & QMailMessage::Important))
            return QStringLiteral("starred");
        if (m_unreadFirst && isUnread(metaData.id(), metaData.status()))
            return QStringLiteral("unread");
        switch (m_sortOrder) {
        case ByDate:
        case OldestFirst:
            return QStringLiteral("d:") + metaData.date().toLocalTime().date().toString(Qt::ISODate);
        case BySender:
        case BySenderDescending:
            return QStringLiteral("s:") + (metaData.from().name().isEmpty() ? metaData.from().address()
                                                                           : metaData.from().name());
        case BySize:
        case SortOrderCount:
            break;
        }
        return QString();
    case ReadStatusRole:
        return !isUnread(metaData.id(), metaData.status());
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
        return QMailMessageModelBase::data(metaData, role);
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

    // Setting the key resets the list, so only for other folders; otherwise the rows are moved
    // into place, as a conversation's unread state can change its place too, and drawn again.
    m_rows = rows;
    const QMailMessageKey key = rows.isEmpty() ? QMailMessageKey::nonMatchingKey() : QMailMessageKey::id(rows);
    if (force) {
        setKey(key);
    } else {
        m_list->setKey(key);
        reorder(true);
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
