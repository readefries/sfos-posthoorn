// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef IMAPPARTFETCHER_H
#define IMAPPARTFETCHER_H

#include <QByteArray>
#include <QList>
#include <QObject>

#include <qmailmessage.h>

class ImapSession;

// Fetches one part of a mail from its IMAP server exactly as it is there: its MIME header and its
// content (BODY.PEEK[n.MIME] and BODY.PEEK[n]), over a connection of its own (ImapSession) that
// only reads (EXAMINE) and is closed again right after.
//
// For checking a PGP/MIME signature, which covers the signed part byte for byte, while QMF keeps
// that part only as it rebuilt it. Nothing goes into QMF's store.
class ImapPartFetcher : public QObject
{
    Q_OBJECT

public:
    explicit ImapPartFetcher(QObject *parent = nullptr);

    // Fetches part ("1", "2.1", ...) of the message; finished() or failed() follows.
    void fetch(const QMailMessageId &id, const QString &part);

signals:
    void finished(const QByteArray &data);
    void failed(const QString &reason);

private:
    void handleFetch(const QByteArray &text, const QList<QByteArray> &literals);
    void fail(const QString &reason);

    ImapSession *m_session;
    QString m_part;
    QByteArray m_mime;
    QByteArray m_body;
    bool m_failed = false;
};

#endif // IMAPPARTFETCHER_H
