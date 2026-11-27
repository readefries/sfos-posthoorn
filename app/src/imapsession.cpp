// SPDX-License-Identifier: AGPL-3.0-or-later
#include "imapsession.h"

#include <QDataStream>
#include <QDebug>

#include <qmailauthenticator.h>
#include <qmailcredentials.h>
#include <qmailfolder.h>
#include <qmailnamespace.h>
#include <qmailserviceaction.h>
#include <qmailserviceconfiguration.h>
#include <qmailtransport.h>

namespace {

const QString imapService = QStringLiteral("imap4");
const int timeoutSeconds = 60;

// The size of the literal a response line announces at its end ("... {123}"), or -1.
int literalSize(const QByteArray &line)
{
    if (!line.endsWith('}'))
        return -1;
    const int open = line.lastIndexOf('{');
    if (open < 0)
        return -1;
    bool ok = false;
    const int size = line.mid(open + 1, line.size() - open - 2).toInt(&ok);
    return ok && size >= 0 ? size : -1;
}

} // namespace

ImapSession::ImapSession(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(timeoutSeconds * 1000);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        if (m_phase == LoggingOut) {
            m_phase = Idle;
            m_transport->close();
        } else {
            fail(tr("The mail server didn't answer in time"));
        }
    });
}

ImapSession::~ImapSession()
{
    if (m_transport && m_transport->inUse())
        m_transport->close();
}

QByteArray ImapSession::quoted(const QByteArray &input)
{
    static const QByteArray specials("(){ *%\\\"]\t\r\n");
    bool atom = !input.isEmpty();
    for (const char c : input) {
        if (specials.contains(c)) {
            atom = false;
            break;
        }
    }
    if (atom)
        return input;
    QByteArray result("\"");
    for (const char c : input) {
        if (c == '\\' || c == '"')
            result += '\\';
        result += c;
    }
    return result + '"';
}

QString ImapSession::folderPath(const QMailMessageMetaData &message)
{
    // The server's UID comes after the id of the folder it belongs to ("28|4145"), which can
    // differ from the folder the mail is in on the phone while a move isn't on the server yet.
    const QString serverUid = message.serverUid();
    if (serverUid.isEmpty())
        return QString();
    QMailFolderId folderId = message.parentFolderId();
    if (serverUid.contains(QLatin1Char('|'))) {
        bool ok = false;
        const quint64 number = serverUid.section(QLatin1Char('|'), 0, 0).toULongLong(&ok);
        if (ok)
            folderId = QMailFolderId(number);
    }
    return folderId.isValid() ? QMailFolder(folderId).path() : QString();
}

QByteArray ImapSession::uid(const QMailMessageMetaData &message)
{
    return message.serverUid().section(QLatin1Char('|'), -1).toLatin1();
}

bool ImapSession::open(const QMailAccountId &account)
{
    m_buffer.clear();
    m_queue.clear();
    m_waiting = false;
    m_capabilities.clear();
    m_config = QMailAccountConfiguration(account);
    if (!m_config.services().contains(imapService))
        return false;

    m_phase = Credentials;
    m_timeout.start();
    if (m_credentials)
        m_credentials->deleteLater();
    m_credentials = QMailCredentialsFactory::getCredentialsHandlerForAccount(m_config, this);
    if (!m_credentials) {
        // After returning, so the caller is connected to failed() by then.
        QTimer::singleShot(0, this, [this]() { fail(tr("The account's credentials aren't available")); });
        return true;
    }
    connect(m_credentials.data(), &QMailCredentialsInterface::statusChanged,
            this, &ImapSession::onCredentialsStatus);
    m_credentials->init(QMailServiceConfiguration(&m_config, imapService));
    QTimer::singleShot(0, this, &ImapSession::onCredentialsStatus);
    return true;
}

void ImapSession::command(const QByteArray &command, const Done &done)
{
    m_queue.append(Command { command, done });
    if (m_phase == Ready && !m_waiting)
        sendNext();
}

void ImapSession::logout()
{
    m_queue.clear();
    if (m_phase != Ready) {
        m_phase = Idle;
        m_timeout.stop();
        if (m_transport && m_transport->inUse())
            m_transport->close();
        return;
    }
    m_phase = LoggingOut;
    m_timeout.start();
    send("LOGOUT");
}

bool ImapSession::hasCapability(const QByteArray &capability) const
{
    return m_capabilities.contains(capability.toUpper());
}

void ImapSession::onCredentialsStatus()
{
    if (m_phase != Credentials || !m_credentials)
        return;
    switch (m_credentials->status()) {
    case QMailCredentialsInterface::Ready:
        connectToServer();
        break;
    case QMailCredentialsInterface::Failed:
        qWarning() << "No credentials for IMAP:" << m_credentials->lastError();
        fail(tr("The account's credentials aren't available"));
        break;
    default:
        break;
    }
}

void ImapSession::connectToServer()
{
    const QMailServiceConfiguration config(&m_config, imapService);
    if (!m_transport) {
        m_transport = new QMailTransport("Posthoorn-IMAP");
        m_transport->setParent(this);
        connect(m_transport, &QMailTransport::readyRead, this, &ImapSession::onReadyRead);
        connect(m_transport, &QMailTransport::connected, this, [this](QMailTransport::EncryptType type) {
            onConnected(type);
        });
        connect(m_transport, &QMailTransport::socketErrorOccurred, this, [this](int, const QString &message) {
            if (m_phase == LoggingOut || m_phase == Idle)
                return;
            qWarning() << "IMAP connection:" << message;
            fail(tr("Couldn't reach the mail server"));
        });
        connect(m_transport, &QMailTransport::sslErrorOccurred, this, [this]() {
            fail(tr("The mail server's certificate isn't trusted"));
        });
    }
    m_transport->setAcceptUntrustedCertificates(
                config.value(QStringLiteral("acceptUntrustedCertificates"), QStringLiteral("0")).toInt() != 0);
    m_phase = Greeting;
    m_transport->open(config.value(QStringLiteral("server")),
                      config.value(QStringLiteral("port"), QStringLiteral("993")).toInt(),
                      static_cast<QMailTransport::EncryptType>(
                          config.value(QStringLiteral("encryption"), QStringLiteral("0")).toInt()));
}

void ImapSession::onConnected(int encryption)
{
    // After STARTTLS.
    if (m_phase == Securing && encryption == QMailTransport::Encrypt_TLS)
        authenticate();
}

void ImapSession::onReadyRead()
{
    m_buffer += m_transport->readAll();
    // A response is a line, or lines with literals between them: "... {n}", CRLF, n bytes, more.
    int position = 0;
    QByteArray text;
    QList<QByteArray> literals;
    while (m_phase != Idle) {
        const int end = m_buffer.indexOf("\r\n", position);
        if (end < 0)
            return;
        const QByteArray line = m_buffer.mid(position, end - position);
        const int size = literalSize(line);
        if (size >= 0) {
            if (m_buffer.size() < end + 2 + size)
                return;
            text += line.left(line.lastIndexOf('{')) + literalMarker;
            literals.append(m_buffer.mid(end + 2, size));
            position = end + 2 + size;
            continue;
        }
        text += line;
        position = end + 2;
        m_buffer.remove(0, position);
        position = 0;
        handleResponse(text, literals);
        text.clear();
        literals.clear();
    }
}

void ImapSession::handleResponse(const QByteArray &text, const QList<QByteArray> &literals)
{
    if (m_phase == Greeting) {
        if (text.startsWith("* OK")) {
            const QMailServiceConfiguration config(&m_config, imapService);
            if (config.value(QStringLiteral("encryption")).toInt() == QMailTransport::Encrypt_TLS) {
                m_phase = StartingTls;
                send("STARTTLS");
            } else {
                authenticate();
            }
        } else if (text.startsWith("* PREAUTH")) {
            m_phase = Authenticating;
            handleTagged("OK", text);
        } else {
            qWarning() << "IMAP greeting:" << text;
            fail(tr("The mail server refused the connection"));
        }
        return;
    }
    if (text.startsWith("+")) {
        handleContinuation(text);
        return;
    }
    if (!m_tag.isEmpty() && text.startsWith(m_tag + ' ')) {
        const QByteArray rest = text.mid(m_tag.size() + 1);
        handleTagged(rest.left(rest.indexOf(' ')).toUpper(), rest);
        return;
    }
    if (m_phase == Capabilities && text.toUpper().startsWith("* CAPABILITY ")) {
        m_capabilities = text.toUpper().mid(13).split(' ');
        return;
    }
    if (m_phase == Ready && text.startsWith("* "))
        emit untagged(text, literals);
}

void ImapSession::handleTagged(const QByteArray &status, const QByteArray &text)
{
    const bool ok = status == "OK";
    switch (m_phase) {
    case StartingTls:
        if (!ok) {
            fail(tr("The mail server doesn't offer encryption"));
            return;
        }
        m_phase = Securing;
        m_transport->switchToEncrypted();
        break;
    case Authenticating:
        if (!ok) {
            qWarning() << "IMAP sign-in:" << text;
            fail(tr("Couldn't sign in to the mail server"));
            return;
        }
        m_phase = Capabilities;
        send("CAPABILITY");
        break;
    case Capabilities:
        m_phase = Ready;
        emit ready();
        if (m_phase == Ready && !m_waiting)
            sendNext();
        break;
    case Ready: {
        m_waiting = false;
        if (m_queue.isEmpty())
            break;
        const Command finished = m_queue.takeFirst();
        if (finished.done)
            finished.done(ok, text);
        if (m_phase == Ready && !m_waiting)
            sendNext();
        if (m_phase == Ready && !m_waiting)
            m_timeout.stop();
        break;
    }
    case LoggingOut:
        m_phase = Idle;
        m_timeout.stop();
        m_transport->close();
        break;
    default:
        break;
    }
}

void ImapSession::handleContinuation(const QByteArray &text)
{
    if (m_phase != Authenticating)
        return;
    if (!m_authLines.isEmpty()) {
        sendLine(m_authLines.takeFirst());
        return;
    }
    // A challenge (CRAM-MD5), or the reason a sign-in failed (XOAUTH2), which wants an empty line.
    const QByteArray challenge = QByteArray::fromBase64(text.mid(2));
    const QByteArray response = QMailAuthenticator::getResponse(
                QMailServiceConfiguration(&m_config, imapService), challenge, *m_credentials);
    sendLine(response.isEmpty() ? QByteArray() : response.toBase64());
}

void ImapSession::authenticate()
{
    // As QMF's ImapAuthenticator does.
    m_phase = Authenticating;
    m_authLines.clear();
    const QMailServiceConfiguration config(&m_config, imapService);
    const QByteArray mechanism = QMailAuthenticator::getAuthentication(config, *m_credentials);
    const int authentication = config.value(QStringLiteral("authentication"), QStringLiteral("0")).toInt();
    if (!mechanism.isEmpty()) {
        send("AUTHENTICATE " + mechanism);
    } else if (authentication == QMail::XOAuth2Mechanism) {
        m_authLines << QString::fromLatin1("user=%1\001auth=Bearer %2\001\001")
                       .arg(m_credentials->username(), m_credentials->accessToken()).toUtf8().toBase64();
        send("AUTHENTICATE XOAUTH2");
    } else if (authentication == QMail::PlainMechanism) {
        const QByteArray username = m_credentials->username().toUtf8();
        m_authLines << QByteArray(username + '\0' + username + '\0'
                                  + m_credentials->password().toUtf8()).toBase64();
        send("AUTHENTICATE PLAIN");
    } else {
        send("LOGIN " + quoted(m_credentials->username().toLatin1()) + ' '
             + quoted(m_credentials->password().toLatin1()));
    }
}

void ImapSession::sendNext()
{
    if (m_queue.isEmpty())
        return;
    m_waiting = true;
    m_timeout.start();
    send(m_queue.first().text);
}

void ImapSession::send(const QByteArray &command)
{
    m_tag = 'p' + QByteArray::number(++m_tagCount);
    sendLine(m_tag + ' ' + command);
}

void ImapSession::sendLine(const QByteArray &line)
{
    const QByteArray data = line + "\r\n";
    m_transport->stream().writeRawData(data.constData(), data.size());
}

void ImapSession::fail(const QString &reason)
{
    m_phase = Idle;
    m_timeout.stop();
    m_authLines.clear();
    m_queue.clear();
    m_waiting = false;
    if (m_transport && m_transport->inUse())
        m_transport->close();
    emit failed(reason);
}
