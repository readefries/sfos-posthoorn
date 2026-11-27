// SPDX-License-Identifier: AGPL-3.0-or-later
#include "deletion.h"

#include "appstate.h"
#include "imapsession.h"

#include <QDebug>
#include <QMap>

#include <qmailaccountconfiguration.h>
#include <qmailfolder.h>
#include <qmailstore.h>

namespace {

bool isTrashOrSpam(const QMailAccount &account, const QMailFolderId &folderId)
{
    // Where the email plugin put mail of an account without Trash.
    if (folderId == QMailFolderId(QMailFolderId::LocalStorageFolderId))
        return true;
    if (folderId == account.standardFolder(QMailFolder::TrashFolder)
            || folderId == account.standardFolder(QMailFolder::JunkFolder))
        return true;
    return (QMailFolder(folderId).status() & (QMailFolder::Trash | QMailFolder::Junk)) != 0;
}

QMailFolderId serverFolderId(const QMailMessageMetaData &message)
{
    bool ok = false;
    const quint64 number = message.serverUid().section(QLatin1Char('|'), 0, 0).toULongLong(&ok);
    return ok ? QMailFolderId(number) : message.parentFolderId();
}

void setHidden(const QMailMessageIdList &ids, bool hidden)
{
    if (!ids.isEmpty())
        QMailStore::instance()->updateMessagesMetaData(QMailMessageKey::id(ids), QMailMessage::Removed, hidden);
}

QString expungedKey(const QMailAccountId &account)
{
    return QStringLiteral("deletion/expunged-%1").arg(account.toULongLong());
}

const QString pendingKey = QStringLiteral("deletion/pending");

QByteArray copiedKey(const QByteArray &folderPath, const QByteArray &uidValidity, const QByteArray &uid)
{
    return folderPath + ' ' + uidValidity + ' ' + uid;
}

// "* OK [UIDVALIDITY 1234] UIDs valid": 1234.
QByteArray uidValidityIn(const QByteArray &text)
{
    const QByteArray code("[UIDVALIDITY ");
    const int at = text.toUpper().indexOf(code);
    if (at < 0)
        return QByteArray();
    const int start = at + code.size();
    const int end = text.indexOf(']', start);
    return end < 0 ? QByteArray() : text.mid(start, end - start).trimmed();
}

} // namespace

Deletion::Deletion(QObject *parent)
    : QObject(parent)
    , m_settings(AppState::fileName(), QSettings::IniFormat)
{
    restorePending();
}

void Deletion::restorePending()
{
    // Hidden when the app stopped before the server was done: shown again, to delete again. Mail
    // the server did delete goes on QMF's next sync.
    QMailMessageIdList ids;
    for (const QString &id : m_settings.value(pendingKey).toStringList()) {
        const QMailMessageId messageId(id.toULongLong());
        if (messageId.isValid())
            ids.append(messageId);
    }
    if (ids.isEmpty())
        return;
    qInfo() << "Showing" << ids.size() << "message(s) again that weren't deleted on the server yet";
    setHidden(ids, false);
    m_settings.remove(pendingKey);
    m_settings.sync();
}

void Deletion::setPending(const QMailMessageIdList &ids, bool pending)
{
    QStringList list = m_settings.value(pendingKey).toStringList();
    for (const QMailMessageId &id : ids) {
        const QString text = QString::number(id.toULongLong());
        if (pending) {
            if (!list.contains(text))
                list.append(text);
        } else {
            list.removeAll(text);
        }
    }
    if (list.isEmpty())
        m_settings.remove(pendingKey);
    else
        m_settings.setValue(pendingKey, list);
    // Before the mail is hidden, in case the app stops right after.
    m_settings.sync();
}

void Deletion::unhide(const QMailMessageIdList &ids)
{
    setHidden(ids, false);
    setPending(ids, false);
}

void Deletion::remove(const QVariantList &messageIds)
{
    QMailMessageIdList ids;
    for (const QVariant &id : messageIds) {
        const QMailMessageId messageId(id.toULongLong());
        if (messageId.isValid())
            ids.append(messageId);
    }
    if (ids.isEmpty())
        return;

    QMailMessageIdList localOnly;
    // Per account, per folder on the server, and whether it's for good.
    QMap<QMailAccountId, QMap<QPair<QByteArray, bool>, Batch>> batches;
    const QMailMessageMetaDataList messages = QMailStore::instance()->messagesMetaData(
                QMailMessageKey::id(ids), QMailMessageKey::Id | QMailMessageKey::ParentAccountId
                | QMailMessageKey::ParentFolderId | QMailMessageKey::ServerUid);
    for (const QMailMessageMetaData &message : messages) {
        const QByteArray folderPath = ImapSession::folderPath(message).toUtf8();
        const QByteArray uid = ImapSession::uid(message);
        const QMailAccount account(message.parentAccountId());
        if (folderPath.isEmpty() || uid.isEmpty()
                || !QMailAccountConfiguration(account.id()).services().contains(QStringLiteral("imap4"))) {
            localOnly.append(message.id());
            continue;
        }
        const QMailFolderId trash = account.standardFolder(QMailFolder::TrashFolder);
        const bool forGood = !trash.isValid() || isTrashOrSpam(account, message.parentFolderId())
                || isTrashOrSpam(account, serverFolderId(message));
        Batch &batch = batches[account.id()][qMakePair(folderPath, forGood)];
        batch.folderPath = folderPath;
        batch.uidValidity = QMailFolder(serverFolderId(message)).customField(
                    QStringLiteral("qmf-uidvalidity")).toLatin1();
        if (!forGood)
            batch.trashPath = QMailFolder(trash).path().toUtf8();
        batch.uids.append(uid);
        batch.ids.append(message.id());
    }

    if (!localOnly.isEmpty()) {
        qInfo() << "Deleting" << localOnly.size() << "message(s) only on the phone";
        QMailStore::instance()->removeMessages(QMailMessageKey::id(localOnly));
    }
    for (auto account = batches.constBegin(); account != batches.constEnd(); ++account) {
        Job job;
        job.account = account.key();
        for (const Batch &batch : account.value()) {
            // Gone from the lists right away, while the server does it.
            setPending(batch.ids, true);
            setHidden(batch.ids, true);
            job.batches.append(batch);
        }
        enqueue(job);
    }
}

void Deletion::expungeLeftoversOnce()
{
    QMailStore *store = QMailStore::instance();
    for (const QMailAccountId &account : store->queryAccounts()) {
        if (m_settings.value(expungedKey(account)).toBool()
                || !QMailAccountConfiguration(account).services().contains(QStringLiteral("imap4")))
            continue;
        bool queued = false;
        for (const Job &job : m_jobs)
            queued = queued || (job.account == account && job.cleanup);
        if (queued)
            continue;
        Job job;
        job.account = account;
        job.cleanup = true;
        for (const QMailFolderId &folder : store->queryFolders(QMailFolderKey::parentAccountId(account))) {
            const QString path = QMailFolder(folder).path();
            if (!path.isEmpty())
                job.expunge.append(path.toUtf8());
        }
        if (!job.expunge.isEmpty())
            enqueue(job);
    }
}

void Deletion::enqueue(const Job &job)
{
    m_jobs.append(job);
    if (!m_session)
        runNext();
}

void Deletion::runNext()
{
    if (m_jobs.isEmpty())
        return;
    const Job &job = m_jobs.first();
    m_session = new ImapSession(this);

    connect(m_session, &ImapSession::failed, this, [this](const QString &reason) {
        // The connection is gone: what's left of this job is undone.
        Job &job = m_jobs.first();
        for (const Batch &batch : job.batches)
            unhide(batch.ids);
        if (!job.batches.isEmpty()) {
            qWarning() << "Deleting failed:" << reason;
            emit failed(reason);
        } else if (!job.expunge.isEmpty()) {
            qWarning() << "Expunging left-over mail failed:" << reason;
        }
        job.batches.clear();
        jobDone();
    });

    connect(m_session, &ImapSession::untagged, this, [this](const QByteArray &text) {
        const QByteArray uidValidity = uidValidityIn(text);
        if (!uidValidity.isEmpty())
            m_uidValidity = uidValidity;
    });
    connect(m_session, &ImapSession::ready, this, &Deletion::step);

    if (!m_session->open(job.account)) {
        for (const Batch &batch : job.batches)
            unhide(batch.ids);
        m_session->deleteLater();
        m_session = nullptr;
        m_jobs.removeFirst();
        runNext();
    }
}

void Deletion::runCommands(QList<QByteArray> commands, const std::function<void(bool, QByteArray)> &done)
{
    if (commands.isEmpty()) {
        done(true, QByteArray());
        return;
    }
    const QByteArray command = commands.takeFirst();
    ImapSession *session = m_session;
    m_session->command(command, [this, session, commands, done](bool ok, const QByteArray &text) {
        // Answers after the job is over don't count.
        if (session != m_session)
            return;
        if (ok)
            runCommands(commands, done);
        else
            done(false, text);
    });
}

void Deletion::step()
{
    Job &job = m_jobs.first();
    if (!job.batches.isEmpty()) {
        const Batch batch = job.batches.first();
        m_uidValidity.clear();
        runCommands(QList<QByteArray>() << "SELECT " + ImapSession::quoted(batch.folderPath),
                    [this, batch](bool ok, const QByteArray &text) {
            if (!ok) {
                batchDone(batch, false, text);
                return;
            }
            // The UIDs are of what QMF synced: once the folder's UIDVALIDITY is another, they
            // can be other mail's.
            if (batch.uidValidity.isEmpty() || m_uidValidity != batch.uidValidity) {
                qWarning() << "UIDVALIDITY of" << batch.folderPath << "is" << m_uidValidity
                           << "on the server, synced was" << batch.uidValidity;
                batchDone(batch, false, tr("The folder changed on the server; sync it and try again").toUtf8());
                return;
            }
            deleteSelected(batch);
        });
        return;
    }
    if (!job.expunge.isEmpty()) {
        const QByteArray folder = job.expunge.takeFirst();
        runCommands(QList<QByteArray>() << "SELECT " + ImapSession::quoted(folder),
                    [this, folder](bool ok, const QByteArray &text) {
            if (!ok) {
                // A folder that can't be selected (only holding others) is skipped.
                qInfo() << "Not selectable, skipped:" << folder << text;
                step();
                return;
            }
            runCommands(QList<QByteArray>() << "EXPUNGE", [this, folder](bool ok, const QByteArray &text) {
                if (!ok) {
                    qWarning() << "Not expunged:" << folder << text;
                    m_jobs.first().expungeFailed = true;
                }
                step();
            });
        });
        return;
    }
    if (job.cleanup) {
        if (job.expungeFailed) {
            qWarning() << "Expunging left-over mail of account" << job.account.toULongLong()
                       << "failed somewhere; again on the next start";
        } else {
            qInfo() << "Expunged left-over mail of account" << job.account.toULongLong();
            m_settings.setValue(expungedKey(job.account), true);
        }
    }
    m_session->logout();
    jobDone();
}

void Deletion::deleteSelected(const Batch &batch)
{
    const QByteArray set = batch.uids.join(',');
    const QByteArray expunge = m_session->hasCapability("UIDPLUS") ? "UID EXPUNGE " + set
                                                                   : QByteArray("EXPUNGE");
    const auto done = [this, batch](bool ok, const QByteArray &text) {
        batchDone(batch, ok, text);
    };
    const QList<QByteArray> remove = QList<QByteArray>()
            << "UID STORE " + set + " +FLAGS.SILENT (\\Deleted)" << expunge;

    if (batch.trashPath.isEmpty()) {
        runCommands(remove, done);
        return;
    }
    if (m_session->hasCapability("MOVE")) {
        runCommands(QList<QByteArray>() << "UID MOVE " + set + ' ' + ImapSession::quoted(batch.trashPath), done);
        return;
    }
    // Copied by an earlier try that failed after: not again.
    QList<QByteArray> toCopy;
    for (const QByteArray &uid : batch.uids) {
        if (!m_copied.contains(copiedKey(batch.folderPath, batch.uidValidity, uid)))
            toCopy.append(uid);
    }
    if (toCopy.isEmpty()) {
        runCommands(remove, done);
        return;
    }
    runCommands(QList<QByteArray>() << "UID COPY " + toCopy.join(',') + ' ' + ImapSession::quoted(batch.trashPath),
                [this, batch, toCopy, remove, done](bool ok, const QByteArray &text) {
        if (!ok) {
            done(false, text);
            return;
        }
        for (const QByteArray &uid : toCopy)
            m_copied.insert(copiedKey(batch.folderPath, batch.uidValidity, uid));
        runCommands(remove, done);
    });
}

void Deletion::batchDone(const Batch &batch, bool ok, const QByteArray &text)
{
    m_jobs.first().batches.removeFirst();
    finishBatch(batch, ok, text);
    step();
}

void Deletion::finishBatch(const Batch &batch, bool ok, const QByteArray &text)
{
    if (!ok) {
        qWarning() << "Deleting from" << batch.folderPath << "failed:" << text;
        unhide(batch.ids);
        emit failed(tr("The mail server didn't delete the mail: %1").arg(QString::fromUtf8(text)));
        return;
    }
    qInfo() << "Deleted" << batch.ids.size() << "message(s) from" << batch.folderPath
            << (batch.trashPath.isEmpty() ? "for good" : "to Trash");
    for (const QByteArray &uid : batch.uids)
        m_copied.remove(copiedKey(batch.folderPath, batch.uidValidity, uid));
    setPending(batch.ids, false);
    // The server has done it; no removal record, which would have QMF do it again.
    QMailStore::instance()->removeMessages(QMailMessageKey::id(batch.ids), QMailStore::NoRemovalRecord);
    if (!batch.trashPath.isEmpty()) {
        const QMailAccount account(m_jobs.first().account);
        emit movedToTrash(int(account.id().toULongLong()),
                          int(account.standardFolder(QMailFolder::TrashFolder).toULongLong()));
    }
}

void Deletion::jobDone()
{
    if (m_session) {
        m_session->deleteLater();
        m_session = nullptr;
    }
    if (!m_jobs.isEmpty())
        m_jobs.removeFirst();
    runNext();
}
