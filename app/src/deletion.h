// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef DELETION_H
#define DELETION_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
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
// reached it shows again, and failed() says why. What's hidden is noted in the app's settings, so
// that mail still hidden when the app stopped mid-way shows again on the next start.
//
// The UIDs are QMF's: nothing is done in a folder whose UIDVALIDITY on the server isn't the one
// QMF synced, as they could be other mail's now.
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
        // The folder's UIDVALIDITY as QMF synced it, which the UIDs are of.
        QByteArray uidValidity;
        QByteArray trashPath;
        QList<QByteArray> uids;
        QMailMessageIdList ids;
    };
    struct Job {
        QMailAccountId account;
        QList<Batch> batches;
        // For expungeLeftoversOnce(): the folders still to expunge, and whether one failed.
        bool cleanup = false;
        bool expungeFailed = false;
        QList<QByteArray> expunge;
    };

    void enqueue(const Job &job);
    void runNext();
    // The job's next batch or folder to expunge, or signing out.
    void step();
    // One after the other, stopping at the first the server refuses.
    void runCommands(QList<QByteArray> commands, const std::function<void(bool, QByteArray)> &done);
    // The batch's folder is selected: to Trash, or for good.
    void deleteSelected(const Batch &batch);
    // The job's first batch is done, or not: on to the next.
    void batchDone(const Batch &batch, bool ok, const QByteArray &text);
    void finishBatch(const Batch &batch, bool ok, const QByteArray &text);
    void jobDone();
    // Shows what's hidden, as the server didn't delete it.
    void unhide(const QMailMessageIdList &ids);
    // What's hidden while the server deletes it, in the settings.
    void setPending(const QMailMessageIdList &ids, bool pending);
    void restorePending();

    QSettings m_settings;
    QList<Job> m_jobs;
    QPointer<ImapSession> m_session;
    // UIDVALIDITY of the folder last selected.
    QByteArray m_uidValidity;
    // Mail copied to Trash where the rest failed ("folder uidvalidity uid"), not to copy again.
    QSet<QByteArray> m_copied;
};

#endif // DELETION_H
