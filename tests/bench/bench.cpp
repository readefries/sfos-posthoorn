// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Times UnifiedMessageListModel on a large folder:
//
//   QMF_DATA=/tmp/bench-qmf bench-messagelist [messages]      default: 15000
//
// Only in the store QMF_DATA points to, which must be set: the phone's mail is never touched. The
// first run fills it with a folder of that many generated mails (a third of them replies, a tenth
// unread) and a small other folder; later runs reuse them. Their account is a row written straight
// into that store's database: made through QMF, on Sailfish it would go to the system's accounts
// database.
//
// Reports the process's CPU time for: loading the list, flat and by conversation; mail in the
// folder marked read and unread; mail in another folder changed; mail moved into the folder. As the
// messages page does, unreadCount is read again each time it's announced.

#include "unifiedmessagelistmodel.h"

#include <qmailfolder.h>
#include <qmailmessage.h>
#include <qmailstore.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace {

double cpuMs()
{
    timespec now;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
    return now.tv_sec * 1000.0 + now.tv_nsec / 1e6;
}

// Lets the model's timers run and their work finish.
void settle(int ms = 1500)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

QSqlDatabase db;
qint64 account = 0;

void exec(QSqlQuery &query)
{
    if (!query.exec()) {
        std::fprintf(stderr, "Benchmark store: %s\n", qPrintable(query.lastError().text()));
        std::exit(1);
    }
}

qint64 insert(const QString &sql, const QVariantList &values)
{
    QSqlQuery query(db);
    query.prepare(sql);
    for (const QVariant &value : values)
        query.addBindValue(value);
    exec(query);
    return query.lastInsertId().toLongLong();
}

// The benchmark's account, in the store's own table: made through QMF, on Sailfish it would go to
// the system's accounts database.
void openStore(const QString &data)
{
    db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("bench"));
    db.setDatabaseName(data + QStringLiteral("/database/qmailstore.db"));
    if (!db.open()) {
        std::fprintf(stderr, "Can't open the benchmark's store: %s\n", qPrintable(db.lastError().text()));
        std::exit(1);
    }
    QSqlQuery query(db);
    query.exec(QStringLiteral("SELECT id FROM mailaccounts WHERE name = 'Bench'"));
    account = query.next() ? query.value(0).toLongLong()
                           : insert(QStringLiteral("INSERT INTO mailaccounts (type, name, emailaddress, status) "
                                                   "VALUES (0, 'Bench', 'me@example.com', 0)"), {});
}

QMailFolderId folderNamed(const QString &name)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT id FROM mailfolders WHERE name = ?"));
    query.addBindValue(name);
    exec(query);
    if (query.next())
        return QMailFolderId(query.value(0).toULongLong());
    return QMailFolderId(insert(QStringLiteral("INSERT INTO mailfolders (name, parentid, parentaccountid, status, "
                                               "displayname, servercount, serverunreadcount, "
                                               "serverundiscoveredcount) VALUES (?, 0, ?, 0, ?, 0, 0, 0)"),
                                { name, account, name }));
}

// Written straight into the store, as QMF refuses an account it doesn't know: a third of them
// replies, in conversations of three, a tenth unread, one in fifty starred.
void fill(const QMailFolderId &folder, int count, int offset)
{
    const QDateTime start = QDateTime::currentDateTimeUtc().addDays(-count / 20);
    db.transaction();
    qint64 thread = 0;
    for (int i = 0; i < count; ++i) {
        const int n = offset + i;
        const int topic = n / 3;
        const QString subject = (n % 3 ? QStringLiteral("Re: Topic %1") : QStringLiteral("Topic %1")).arg(topic);
        const QDateTime date = start.addSecs(qint64(i) * 180);
        if (n % 3 == 0 || thread == 0) {
            thread = insert(QStringLiteral("INSERT INTO mailthreads (messagecount, unreadcount, serveruid, "
                                           "parentaccountid, subject, preview, senders, lastDate, startedDate, "
                                           "status) VALUES (3, 0, '', ?, ?, '', '', ?, ?, 0)"),
                            { account, subject, date, date });
        }
        quint64 status = 0;
        if (n % 10 != 0)
            status |= QMailMessage::Read;
        if (n % 50 == 0)
            status |= QMailMessage::Important;
        const QString sender = QMailAddress(QStringLiteral("Sender %1").arg(n % 300),
                                            QStringLiteral("sender%1@example.com").arg(n % 300)).toString();
        insert(QStringLiteral("INSERT INTO mailmessages (type, parentfolderid, previousparentfolderid, sender, "
                              "recipients, subject, stamp, status, parentaccountid, frommailbox, mailfile, "
                              "serveruid, size, contenttype, responseid, responsetype, receivedstamp, rfcid, "
                              "preview, parentthreadid) VALUES (?, ?, 0, ?, 'me@example.com', ?, ?, ?, ?, '', '', "
                              "?, ?, 0, 0, 0, ?, ?, ?, ?)"),
               { int(QMailMessage::Email), folder.toULongLong(), sender, subject, date, status, account,
                 QStringLiteral("bench-%1").arg(n), 2000 + (n * 37) % 50000, date,
                 QStringLiteral("<bench-%1@example.com>").arg(n),
                 QStringLiteral("Mail number %1 of the benchmark").arg(n), thread });
    }
    db.commit();
}

struct Run {
    UnifiedMessageListModel model;
    int announced = 0;

    explicit Run(bool threaded)
    {
        model.setThreaded(threaded);
        QObject::connect(&model, &UnifiedMessageListModel::unreadCountChanged, [this] {
            ++announced;
            model.unreadCount();
        });
    }
};

void report(const char *what, double ms, int announced)
{
    std::printf("  %-40s %8.0f ms CPU, unreadCount announced %d times\n", what, ms, announced);
    std::fflush(stdout);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QString data = QString::fromLocal8Bit(qgetenv("QMF_DATA"));
    if (data.isEmpty() || !QDir(data).exists()) {
        std::fprintf(stderr, "Set QMF_DATA to an existing, empty folder for the benchmark's own store\n");
        return 2;
    }
    if (qgetenv("QT_LOGGING_RULES").isEmpty())
        QLoggingCategory::setFilterRules(QStringLiteral("posthoorn.list.debug=false"));
    const int count = argc > 1 ? QString::fromLocal8Bit(argv[1]).toInt() : 15000;

    QMailStore *store = QMailStore::instance();
    openStore(data);
    const QMailFolderId inbox = folderNamed(QStringLiteral("BenchInbox"));
    const QMailFolderId other = folderNamed(QStringLiteral("BenchOther"));
    const int have = store->countMessages(QMailMessageKey::parentFolderId(inbox));
    if (have < count) {
        std::printf("Adding %d messages...\n", count - have);
        std::fflush(stdout);
        fill(inbox, count - have, have);
    }
    if (store->countMessages(QMailMessageKey::parentFolderId(other)) == 0)
        fill(other, 200, 1000000);
    settle(500);
    const QMailMessageIdList inboxIds = store->queryMessages(QMailMessageKey::parentFolderId(inbox),
                                                             QMailMessageSortKey::id(Qt::DescendingOrder), 20);
    const QMailMessageIdList otherIds = store->queryMessages(QMailMessageKey::parentFolderId(other),
                                                             QMailMessageSortKey::id(), 20);
    std::printf("%d messages in the folder\n", store->countMessages(QMailMessageKey::parentFolderId(inbox)));

    for (const bool threaded : { false, true }) {
        std::printf("%s:\n", threaded ? "By conversation" : "Flat");
        Run run(threaded);
        double start = cpuMs();
        run.model.setFolderIds(QVariantList() << QVariant(inbox.toULongLong()));
        const int rows = run.model.rowCount();
        const int unread = run.model.unreadCount();
        settle();
        report(qPrintable(QStringLiteral("load (%1 rows, %2 unread)").arg(rows).arg(unread)),
               cpuMs() - start, run.announced);

        for (const bool read : { false, true }) {
            run.announced = 0;
            start = cpuMs();
            store->updateMessagesMetaData(QMailMessageKey::id(inboxIds), QMailMessage::Read, read);
            settle();
            report(read ? "20 mails in the folder marked read" : "20 mails in the folder marked unread",
                   cpuMs() - start, run.announced);
        }

        run.announced = 0;
        start = cpuMs();
        store->updateMessagesMetaData(QMailMessageKey::id(otherIds), QMailMessage::Important, !threaded);
        settle();
        report("20 mails in another folder starred", cpuMs() - start, run.announced);

        // As new mail coming in: moved in from the other folder, through QMF, and back.
        const QMailMessageIdList arriving = threaded ? otherIds.mid(5, 5) : otherIds.mid(0, 5);
        run.announced = 0;
        start = cpuMs();
        QMailMessageMetaData moved;
        moved.setParentFolderId(inbox);
        store->updateMessagesMetaData(QMailMessageKey::id(arriving), QMailMessageKey::ParentFolderId, moved);
        settle();
        report("5 mails moved into the folder", cpuMs() - start, run.announced);
        moved.setParentFolderId(other);
        store->updateMessagesMetaData(QMailMessageKey::id(arriving), QMailMessageKey::ParentFolderId, moved);
        settle();
    }
    return 0;
}
