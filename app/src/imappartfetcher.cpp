// SPDX-License-Identifier: AGPL-3.0-or-later
#include "imappartfetcher.h"

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
// Stands in for a literal ({n} and its bytes) in a response's text.
const char literalMarker = '\x01';

// An IMAP string: an atom when it can be one, quoted otherwise (as QMF's ImapProtocol does).
QByteArray quoted(const QByteArray &input)
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

ImapPartFetcher::ImapPartFetcher(QObject *parent)
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

ImapPartFetcher::~ImapPartFetcher()
{
    if (m_transport && m_transport->inUse())
        m_transport->close();
}

void ImapPartFetcher::fetch(const QMailMessageId &id, const QString &part)
{
    m_part = part;
    m_buffer.clear();
    m_mime.clear();
    m_body.clear();
    m_fetched = false;

    // The server's UID comes after the id of the folder it belongs to ("28|4145").
    const QMailMessageMetaData metaData(id);
    const QString serverUid = metaData.serverUid();
    m_uid = serverUid.section(QLatin1Char('|'), -1);
    QMailFolderId folderId = metaData.parentFolderId();
    if (serverUid.contains(QLatin1Char('|'))) {
        bool ok = false;
        const quint64 number = serverUid.section(QLatin1Char('|'), 0, 0).toULongLong(&ok);
        if (ok)
            folderId = QMailFolderId(number);
    }
    m_folder = QMailFolder(folderId).path();
    m_config = QMailAccountConfiguration(metaData.parentAccountId());
    if (!m_config.services().contains(imapService) || m_uid.isEmpty() || m_folder.isEmpty()) {
        fail(tr("This mail isn't on an IMAP server"));
        return;
    }

    m_phase = Credentials;
    m_timeout.start();
    if (m_credentials)
        m_credentials->deleteLater();
    m_credentials = QMailCredentialsFactory::getCredentialsHandlerForAccount(m_config, this);
    if (!m_credentials) {
        fail(tr("The account's credentials aren't available"));
        return;
    }
    connect(m_credentials.data(), &QMailCredentialsInterface::statusChanged,
            this, &ImapPartFetcher::onCredentialsStatus);
    m_credentials->init(QMailServiceConfiguration(&m_config, imapService));
    onCredentialsStatus();
}

void ImapPartFetcher::onCredentialsStatus()
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

void ImapPartFetcher::connectToServer()
{
    const QMailServiceConfiguration config(&m_config, imapService);
    if (!m_transport) {
        m_transport = new QMailTransport("Posthoorn-IMAP");
        m_transport->setParent(this);
        connect(m_transport, &QMailTransport::readyRead, this, &ImapPartFetcher::onReadyRead);
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

void ImapPartFetcher::onConnected(int encryption)
{
    // After STARTTLS.
    if (m_phase == Securing && encryption == QMailTransport::Encrypt_TLS)
        authenticate();
}

void ImapPartFetcher::onReadyRead()
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

void ImapPartFetcher::handleResponse(const QByteArray &text, const QList<QByteArray> &literals)
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
    if (text.startsWith(m_tag + ' ')) {
        const QByteArray rest = text.mid(m_tag.size() + 1);
        handleTagged(rest.left(rest.indexOf(' ')).toUpper(), rest);
        return;
    }
    if (m_phase == Fetching && text.startsWith("* ") && text.toUpper().contains(" FETCH ("))
        handleFetch(text, literals);
}

void ImapPartFetcher::handleFetch(const QByteArray &text, const QList<QByteArray> &literals)
{
    // Each literal follows its item's name: "BODY[1.MIME] {n}".
    const QList<QByteArray> before = text.split(literalMarker);
    const QByteArray mimeSection = m_part.toLatin1() + ".MIME";
    const QByteArray bodySection = m_part.toLatin1();
    for (int i = 0; i < literals.size() && i < before.size(); ++i) {
        const QByteArray item = before.at(i).toUpper();
        const int start = item.lastIndexOf("BODY[");
        const int end = item.indexOf(']', start);
        if (start < 0 || end < 0)
            continue;
        const QByteArray section = item.mid(start + 5, end - start - 5);
        if (section == mimeSection)
            m_mime = literals.at(i);
        else if (section == bodySection)
            m_body = literals.at(i);
    }
    m_fetched = !m_mime.isEmpty() && !m_body.isEmpty();
}

void ImapPartFetcher::handleTagged(const QByteArray &status, const QByteArray &text)
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
        m_phase = Examining;
        // Read-only: nothing changes on the server, not even the \Seen flag.
        send("EXAMINE " + quoted(m_folder.toUtf8()));
        break;
    case Examining:
        if (!ok) {
            qWarning() << "IMAP EXAMINE:" << text;
            fail(tr("The mail's folder isn't on the server any more"));
            return;
        }
        m_phase = Fetching;
        send("UID FETCH " + m_uid.toLatin1() + " (BODY.PEEK[" + m_part.toLatin1() + ".MIME] BODY.PEEK["
             + m_part.toLatin1() + "])");
        break;
    case Fetching:
        if (!ok || !m_fetched) {
            qWarning() << "IMAP FETCH:" << text << "complete:" << m_fetched;
            fail(tr("The mail isn't on the server any more"));
            return;
        }
        finish();
        break;
    case LoggingOut:
        m_phase = Idle;
        m_timeout.stop();
        m_transport->close();
        break;
    default:
        break;
    }
}

void ImapPartFetcher::handleContinuation(const QByteArray &text)
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

void ImapPartFetcher::authenticate()
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

void ImapPartFetcher::send(const QByteArray &command)
{
    m_tag = 'p' + QByteArray::number(++m_tagCount);
    sendLine(m_tag + ' ' + command);
}

void ImapPartFetcher::sendLine(const QByteArray &line)
{
    const QByteArray data = line + "\r\n";
    m_transport->stream().writeRawData(data.constData(), data.size());
}

void ImapPartFetcher::fail(const QString &reason)
{
    m_phase = Idle;
    m_timeout.stop();
    m_authLines.clear();
    if (m_transport && m_transport->inUse())
        m_transport->close();
    emit failed(reason);
}

void ImapPartFetcher::finish()
{
    m_phase = LoggingOut;
    m_timeout.start();
    send("LOGOUT");
    emit finished(m_mime + m_body);
}
