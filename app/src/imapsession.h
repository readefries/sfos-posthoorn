// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef IMAPSESSION_H
#define IMAPSESSION_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>

#include <qmailaccountconfiguration.h>
#include <qmailmessage.h>

class QMailCredentialsInterface;
class QMailTransport;

// A connection of Posthoorn's own to an account's IMAP server, next to QMF's messageserver, for
// what QMF doesn't do right. Signs in as the account, with the credentials the messageserver
// uses (QMailCredentialsFactory), then runs commands one after the other.
class ImapSession : public QObject
{
    Q_OBJECT

public:
    // The tagged answer to a command: OK or not, and its text ("OK [READ-WRITE] ...").
    using Done = std::function<void(bool ok, const QByteArray &text)>;

    // Stands in for a literal ({n} and its bytes) in a response's text.
    static const char literalMarker = '\x01';

    explicit ImapSession(QObject *parent = nullptr);
    ~ImapSession();

    // Signs in; ready() or failed() follows. False, with neither, when it's no IMAP account.
    bool open(const QMailAccountId &account);
    // Sends a command once the previous ones are answered.
    void command(const QByteArray &command, const Done &done);
    // Signs out and closes the connection, without waiting for an answer.
    void logout();

    // What the server said it can do (upper case), known once ready().
    bool hasCapability(const QByteArray &capability) const;

    // An IMAP string: an atom when it can be one, quoted otherwise (as QMF's ImapProtocol does).
    static QByteArray quoted(const QByteArray &input);
    // Where a message is on the server: the path of its folder and its UID there. Empty when it
    // isn't on the server.
    static QString folderPath(const QMailMessageMetaData &message);
    static QByteArray uid(const QMailMessageMetaData &message);

signals:
    void ready();
    void failed(const QString &reason);
    // Untagged responses ("* ...") to the command running.
    void untagged(const QByteArray &text, const QList<QByteArray> &literals);

private:
    enum Phase { Idle, Credentials, Greeting, StartingTls, Securing, Authenticating, Capabilities,
                 Ready, LoggingOut };
    struct Command {
        QByteArray text;
        Done done;
    };

    void onCredentialsStatus();
    void connectToServer();
    void onConnected(int encryption);
    void onReadyRead();
    void handleResponse(const QByteArray &text, const QList<QByteArray> &literals);
    void handleTagged(const QByteArray &status, const QByteArray &text);
    void handleContinuation(const QByteArray &text);
    void authenticate();
    void sendNext();
    void send(const QByteArray &command);
    void sendLine(const QByteArray &line);
    void fail(const QString &reason);

    Phase m_phase = Idle;
    QMailAccountConfiguration m_config;
    QPointer<QMailCredentialsInterface> m_credentials;
    QMailTransport *m_transport = nullptr;
    QTimer m_timeout;
    QByteArray m_buffer;
    QByteArray m_tag;
    int m_tagCount = 0;
    // What's still to be sent in answer to the server's continuations while signing in.
    QList<QByteArray> m_authLines;
    QList<QByteArray> m_capabilities;
    QList<Command> m_queue;
    bool m_waiting = false;
};

#endif // IMAPSESSION_H
