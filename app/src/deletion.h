// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef DELETION_H
#define DELETION_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QVariantList>

#include <functional>

#include <qmailaccount.h>
#include <qmailmessage.h>

class ImapSession;

// Deleting mail, as the Deletion singleton: to the account's Trash, or for good when it's in
// Trash or Spam already, or the account has no Trash.
//
// Not through QMF: its move to Trash copies the mail there and marks it \Deleted where it was,
// but doesn't expunge it there when that was the last (or only) folder it moved mail from, so the
// next sync fetches it again. Here the server does it on a connection of Posthoorn's own
// (ImapSession): UID MOVE, or else UID COPY, \Deleted and UID EXPUNGE (EXPUNGE without UIDPLUS).
// Meanwhile the mail is hidden on the phone (QMailMessage::Removed); once the server is done it's
// removed from the phone, and Trash gets the copy on its next sync. If the server can't be
// reached it shows again, and failed() says why.
class Deletion : public QObject
{
    Q_OBJECT

public:
    explicit Deletion(QObject *parent = nullptr);

    Q_INVOKABLE void remove(const QVariantList &messageIds);
    // Once per account: EXPUNGE in each of its folders, for the mail QMF left marked \Deleted.
    Q_INVOKABLE void expungeLeftoversOnce();

signals:
    void failed(const QString &reason);
    // Mail of the account went to its Trash on the server.
    void movedToTrash(int accountId, int trashFolderId);

private:
    // Mail of one folder on the server, to go to Trash or (trashPath empty) for good.
    struct Batch {
        QByteArray folderPath;
        QByteArray trashPath;
        QList<QByteArray> uids;
        QMailMessageIdList ids;
    };
    struct Job {
        QMailAccountId account;
        QList<Batch> batches;
        // Folders to expunge, for expungeLeftoversOnce().
        QList<QByteArray> expunge;
    };

    void enqueue(const Job &job);
    void runNext();
    // The job's next batch or folder to expunge, or signing out.
    void step();
    // One after the other, stopping at the first the server refuses.
    void runCommands(QList<QByteArray> commands, const std::function<void(bool, QByteArray)> &done);
    void finishBatch(const Batch &batch, bool ok, const QByteArray &text);
    void jobDone();

    QSettings m_settings;
    QList<Job> m_jobs;
    QPointer<ImapSession> m_session;
};

#endif // DELETION_H
