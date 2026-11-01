// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef IMAPPARTFETCHER_H
#define IMAPPARTFETCHER_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <qmailaccountconfiguration.h>
#include <qmailmessage.h>

class QMailCredentialsInterface;
class QMailTransport;

// Fetches one part of a mail from its IMAP server exactly as it is there: its MIME header and its
// content (BODY.PEEK[n.MIME] and BODY.PEEK[n]), over a connection of its own that only reads
// (EXAMINE) and is closed again right after.
//
// For checking a PGP/MIME signature, which covers the signed part byte for byte, while QMF keeps
// that part only as it rebuilt it. Signs in as the account, with the credentials QMF's
// messageserver uses (QMailCredentialsFactory). Nothing goes into QMF's store.
class ImapPartFetcher : public QObject
{
    Q_OBJECT

public:
    explicit ImapPartFetcher(QObject *parent = nullptr);
    ~ImapPartFetcher();

    // Fetches part ("1", "2.1", ...) of the message; finished() or failed() follows.
    void fetch(const QMailMessageId &id, const QString &part);

signals:
    void finished(const QByteArray &data);
    void failed(const QString &reason);

private:
    enum Phase { Idle, Credentials, Greeting, StartingTls, Securing, Authenticating, Examining,
                 Fetching, LoggingOut };

    void onCredentialsStatus();
    void connectToServer();
    void onConnected(int encryption);
    void onReadyRead();
    void handleResponse(const QByteArray &text, const QList<QByteArray> &literals);
    void handleFetch(const QByteArray &text, const QList<QByteArray> &literals);
    void handleTagged(const QByteArray &status, const QByteArray &text);
    void handleContinuation(const QByteArray &text);
    void authenticate();
    void send(const QByteArray &command);
    void sendLine(const QByteArray &line);
    void fail(const QString &reason);
    void finish();

    Phase m_phase = Idle;
    QMailAccountConfiguration m_config;
    QString m_folder;
    QString m_uid;
    QString m_part;
    QPointer<QMailCredentialsInterface> m_credentials;
    QMailTransport *m_transport = nullptr;
    QTimer m_timeout;
    QByteArray m_buffer;
    QByteArray m_tag;
    int m_tagCount = 0;
    // What's still to be sent in answer to the server's continuations while signing in.
    QList<QByteArray> m_authLines;
    QByteArray m_mime;
    QByteArray m_body;
    bool m_fetched = false;
};

#endif // IMAPPARTFETCHER_H
