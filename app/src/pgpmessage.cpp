// SPDX-License-Identifier: AGPL-3.0-or-later
#include "pgpmessage.h"

#include "gpgjob.h"
#include "imappartfetcher.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegExp>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <qmailstore.h>

namespace {

const QByteArray signedMarker("-----BEGIN PGP SIGNED MESSAGE-----");
const QByteArray messageMarker("-----BEGIN PGP MESSAGE-----");
const QByteArray messageEnd("-----END PGP MESSAGE-----");

// The parts PGP/MIME needs are there: the signature or ciphertext (the second part) and, for a
// signature, the signed part (as sent, or the content, for when it can't be fetched as sent).
bool hasPgpParts(const QMailMessage &message)
{
    if (message.partCount() < 2)
        return false;
    const QMailMessagePart &second = message.partAt(1);
    if (!second.hasBody() || second.body().data(QMailMessageBody::Decoded).isEmpty())
        return false;
    if (message.multipartType() == QMailMessagePartContainer::MultipartEncrypted)
        return true;
    return !message.partAt(0).undecodedData().isEmpty() || message.contentAvailable();
}

bool isProtocol(const QMailMessagePartContainer &container, const char *protocol)
{
    return container.contentType().parameter("protocol").trimmed().toLower() == protocol;
}

// The first text part of subtype ("plain", "html"), decoded.
QString findText(const QMailMessagePartContainer &container, const QByteArray &subtype)
{
    if (container.multipartType() == QMailMessagePartContainer::MultipartNone) {
        if (container.hasBody() && container.contentType().type().toLower() == "text"
                && container.contentType().subType().toLower() == subtype)
            return container.body().data();
        return QString();
    }
    for (uint i = 0; i < container.partCount(); ++i) {
        const QMailMessagePart &part = container.partAt(i);
        // Not what's attached.
        if (part.contentDisposition().type() == QMailMessageContentDisposition::Attachment)
            continue;
        const QString text = findText(part, subtype);
        if (!text.isEmpty())
            return text;
    }
    return QString();
}

// A public key attached to the mail (application/pgp-keys), as Posthoorn and Thunderbird send it.
QByteArray findKey(const QMailMessagePartContainer &container)
{
    if (container.multipartType() == QMailMessagePartContainer::MultipartNone) {
        if (container.hasBody() && container.contentType().content().toLower() == "application/pgp-keys")
            return container.body().data(QMailMessageBody::Decoded);
        return QByteArray();
    }
    for (uint i = 0; i < container.partCount(); ++i) {
        const QByteArray key = findKey(container.partAt(i));
        if (!key.isEmpty())
            return key;
    }
    return QByteArray();
}

// The first part of a multipart entity as it is in raw, between its first boundary and the next.
QByteArray firstPart(const QByteArray &raw, const QByteArray &boundary)
{
    if (boundary.isEmpty())
        return QByteArray();
    const QByteArray delimiter = "\n--" + boundary;
    int start = raw.startsWith(delimiter.mid(1)) ? 0 : raw.indexOf(delimiter);
    if (start < 0)
        return QByteArray();
    start = raw.indexOf('\n', start + 1);
    if (start < 0)
        return QByteArray();
    ++start;
    int end = raw.indexOf(delimiter, start);
    if (end < 0)
        return QByteArray();
    // The line break before a boundary belongs to the boundary.
    if (end > start && raw.at(end - 1) == '\r')
        --end;
    return raw.mid(start, end - start);
}

// Where a signed part fetched from the server is kept, so it's fetched once.
QString signedPartCache(const QMailMessageId &id)
{
    const QMailMessageMetaData metaData(id);
    QString uid = metaData.serverUid();
    uid.replace(QRegExp(QStringLiteral("[^A-Za-z0-9]")), QStringLiteral("-"));
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + QStringLiteral("/signed/%1-%2").arg(id.toULongLong()).arg(uid);
}

} // namespace

PgpMessage::PgpMessage(QObject *parent)
    : QObject(parent)
{
    connect(QMailStore::instance(), &QMailStore::messagesUpdated, this, &PgpMessage::onMessagesUpdated);
}

void PgpMessage::setMessageId(int id)
{
    if (id == m_messageId)
        return;
    m_messageId = id;
    emit messageIdChanged();
    inspect();
}

GpgJob *PgpMessage::newJob()
{
    if (m_job)
        m_job->deleteLater();
    m_job = new GpgJob(this);
    return m_job;
}

void PgpMessage::setState(State state, const QString &error)
{
    m_state = state;
    m_error = error;
    emit changed();
}

void PgpMessage::inspect()
{
    m_form = None;
    m_kind = NotPgp;
    m_signature = NoSignature;
    m_signer.clear();
    m_fingerprint.clear();
    m_trusted = false;
    m_plainText.clear();
    m_html.clear();
    m_armored.clear();
    m_attachedKey.clear();
    // Saved for importing, which is done by now.
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/attached-key.asc"));
    m_triedWithoutPassphrase = false;
    if (m_messageId <= 0) {
        setState(Idle);
        return;
    }

    const QMailMessage message{QMailMessageId(m_messageId)};
    if (message.multipartType() == QMailMessagePartContainer::MultipartSigned
            && isProtocol(message, "application/pgp-signature")) {
        m_form = MimeSigned;
        m_kind = Signed;
    } else if (message.multipartType() == QMailMessagePartContainer::MultipartEncrypted
               && isProtocol(message, "application/pgp-encrypted")) {
        m_form = MimeEncrypted;
        m_kind = Encrypted;
    } else {
        const QByteArray text = findText(message, "plain").toUtf8();
        const int begin = text.indexOf(messageMarker);
        if (begin >= 0) {
            const int end = text.indexOf(messageEnd, begin);
            m_form = InlineEncrypted;
            m_kind = Encrypted;
            m_armored = text.mid(begin, end < 0 ? -1 : end - begin + messageEnd.size()) + '\n';
        } else if (text.contains(signedMarker)) {
            m_form = InlineSigned;
            m_kind = Signed;
            m_armored = text.mid(text.indexOf(signedMarker));
        }
    }
    if (m_form == None) {
        setState(Idle);
        return;
    }
    if (!GpgJob::available()) {
        setState(Failed, tr("PGP needs posthoorn-gnupg, which isn't installed"));
        return;
    }
    // PGP/MIME needs its parts; inline PGP is in the text, which is there already.
    if ((m_form == MimeSigned || m_form == MimeEncrypted) && !hasPgpParts(message)) {
        setState(NeedsContent);
        return;
    }

    m_attachedKey = findKey(message);
    switch (m_form) {
    case MimeSigned:
        checkSigned(message);
        break;
    case MimeEncrypted:
        m_armored = message.partCount() > 1 ? message.partAt(1).body().data(QMailMessageBody::Decoded)
                                            : QByteArray();
        decrypt();
        break;
    case InlineSigned: {
        // Decrypting clearsigned text checks the signature and gives the text without armor.
        GpgJob *job = newJob();
        connect(job, &GpgJob::finished, this, &PgpMessage::onDecrypted);
        setState(Working);
        job->start(QStringList() << QStringLiteral("--decrypt"), m_armored);
        break;
    }
    case InlineEncrypted:
        decrypt();
        break;
    case None:
        break;
    }
}

void PgpMessage::recheck()
{
    inspect();
}

QString PgpMessage::saveAttachedKey()
{
    if (m_attachedKey.isEmpty())
        return QString();
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cache);
    const QString path = cache + QStringLiteral("/attached-key.asc");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(m_attachedKey) != m_attachedKey.size())
        return QString();
    return path;
}

void PgpMessage::onMessagesUpdated(const QMailMessageIdList &ids)
{
    if (m_state == NeedsContent && ids.contains(QMailMessageId(m_messageId))
            && hasPgpParts(QMailMessage(QMailMessageId(m_messageId))))
        inspect();
}

void PgpMessage::checkSigned(const QMailMessage &message)
{
    // The signed part as it was sent: QMF's copy when it has one (it got the whole mail), what was
    // fetched before, or else from the server. QMF's own version of it is rebuilt, and its headers
    // can differ from the original's.
    const QByteArray original = message.partCount() > 0 ? message.partAt(0).undecodedData() : QByteArray();
    if (!original.isEmpty()) {
        verifyMime(message, original, false);
        return;
    }
    QFile cached(signedPartCache(message.id()));
    if (cached.open(QIODevice::ReadOnly)) {
        verifyMime(message, cached.readAll(), false);
        return;
    }

    if (m_fetcher)
        m_fetcher->deleteLater();
    m_fetcher = new ImapPartFetcher(this);
    const QMailMessageId id = message.id();
    ImapPartFetcher *fetcher = m_fetcher;
    connect(fetcher, &ImapPartFetcher::finished, this, [this, id, fetcher](const QByteArray &data) {
        if (fetcher != m_fetcher || id != QMailMessageId(m_messageId))
            return;
        const QString path = signedPartCache(id);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
            file.write(data);
        verifyMime(QMailMessage(id), data, false);
    });
    connect(fetcher, &ImapPartFetcher::failed, this, [this, id, fetcher](const QString &reason) {
        if (fetcher != m_fetcher || id != QMailMessageId(m_messageId))
            return;
        qWarning() << "Message" << m_messageId << "signed part not fetched:" << reason;
        // QMF's version then, which can only show a good signature, not a bad one.
        const QMailMessage message(id);
        verifyMime(message, message.partCount() > 0 ? message.partAt(0).toRfc2822() : QByteArray(), true);
    });
    setState(Working);
    m_fetcher->fetch(id, QStringLiteral("1"));
}

void PgpMessage::verifyMime(const QMailMessagePartContainer &container, const QByteArray &signedData,
                            bool rebuilt)
{
    if (container.partCount() < 2 || signedData.isEmpty()) {
        setState(Failed, tr("The signed mail is incomplete"));
        return;
    }
    // The signature in a file, the signed part on stdin.
    const QByteArray signature = container.partAt(1).body().data(QMailMessageBody::Decoded);
    QTemporaryFile file(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                        + QStringLiteral("/signature-XXXXXX.asc"));
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    file.setAutoRemove(false);
    if (!file.open() || file.write(signature) != signature.size()) {
        setState(Failed, tr("The signature couldn't be stored for checking"));
        return;
    }
    file.close();
    if (!m_tempFile.isEmpty())
        QFile::remove(m_tempFile);
    m_tempFile = file.fileName();
    m_rebuilt = rebuilt;
    // QMF downloads attachments only when they're opened, so a key attached to the signed part is
    // looked for in what's checked, which has everything.
    if (m_attachedKey.isEmpty() && !rebuilt)
        m_attachedKey = findKey(QMailMessage::fromRfc2822(signedData));

    GpgJob *job = newJob();
    connect(job, &GpgJob::finished, this, &PgpMessage::onVerified);
    setState(Working);
    job->start(QStringList() << QStringLiteral("--verify") << m_tempFile << QStringLiteral("-"),
               GpgJob::canonical(signedData));
}

void PgpMessage::onVerified()
{
    GpgJob *job = qobject_cast<GpgJob *>(sender());
    if (!job || job != m_job)
        return;
    QFile::remove(m_tempFile);
    m_tempFile.clear();
    readSignature(job);
    if (m_signature == NoSignature) {
        qWarning() << "gpg --verify:" << job->status() << job->errors();
        m_signature = SignatureError;
    }
    // Not a sign of a changed mail when what was checked was rebuilt.
    if (m_signature == BadSignature && m_rebuilt) {
        qWarning() << "Message" << m_messageId << "doesn't match its signature as rebuilt";
        m_signature = SignatureError;
        setState(Done, tr("This mail wasn't stored exactly as it was sent, so its signature can't be checked"));
        return;
    }
    setState(Done);
}

void PgpMessage::decrypt(const QString &passphrase)
{
    if (m_kind != Encrypted || m_armored.isEmpty()) {
        if (m_kind == Encrypted)
            setState(Failed, tr("The encrypted part is missing"));
        return;
    }
    // The ciphertext in a file, as stdin carries the passphrase.
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    QTemporaryFile file(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                        + QStringLiteral("/encrypted-XXXXXX.asc"));
    file.setAutoRemove(false);
    if (!file.open() || file.write(m_armored) != m_armored.size()) {
        setState(Failed, tr("The encrypted mail couldn't be stored for decrypting"));
        return;
    }
    file.close();
    if (!m_tempFile.isEmpty())
        QFile::remove(m_tempFile);
    m_tempFile = file.fileName();

    QStringList arguments;
    QByteArray input;
    // The one that worked a moment ago, if any.
    const QString given = passphrase.isEmpty() ? GpgJob::passphrase() : passphrase;
    if (!given.isEmpty()) {
        arguments << QStringLiteral("--passphrase-fd") << QStringLiteral("0");
        input = given.toUtf8() + '\n';
    } else {
        m_triedWithoutPassphrase = true;
    }
    arguments << QStringLiteral("--decrypt") << m_tempFile;
    GpgJob *job = newJob();
    connect(job, &GpgJob::finished, this, &PgpMessage::onDecrypted);
    job->setProperty("withPassphrase", !passphrase.isEmpty());
    job->setProperty("passphrase", given);
    setState(Working);
    job->start(arguments, input);
}

void PgpMessage::onDecrypted()
{
    GpgJob *job = qobject_cast<GpgJob *>(sender());
    if (!job || job != m_job)
        return;
    if (!m_tempFile.isEmpty()) {
        QFile::remove(m_tempFile);
        m_tempFile.clear();
    }
    // Signed inside, at the OpenPGP level, or a clearsigned text.
    readSignature(job);

    if (m_form == InlineSigned) {
        if (m_signature == NoSignature)
            m_signature = SignatureError;
        m_plainText = QString::fromUtf8(job->output());
        setState(Done);
        return;
    }
    if (job->hasStatus(QStringLiteral("DECRYPTION_OKAY"))) {
        const QString used = job->property("passphrase").toString();
        if (!used.isEmpty())
            GpgJob::rememberPassphrase(used);
        showDecrypted(job->output());
        return;
    }
    if (job->hasStatus(QStringLiteral("NO_SECKEY"))
            && !job->hasStatus(QStringLiteral("NEED_PASSPHRASE"))) {
        setState(Failed, tr("None of your keys can decrypt this mail"));
        return;
    }
    if (job->property("withPassphrase").toBool()) {
        setState(NeedsPassphrase, tr("That passphrase didn't work"));
        return;
    }
    qDebug() << "gpg --decrypt without a passphrase:" << job->status();
    setState(NeedsPassphrase);
}

void PgpMessage::showDecrypted(const QByteArray &output)
{
    if (m_form == InlineEncrypted) {
        m_plainText = QString::fromUtf8(output);
        setState(Done);
        return;
    }
    // PGP/MIME: a MIME entity, which may be signed itself.
    const QMailMessage entity = QMailMessage::fromRfc2822(output);
    m_plainText = findText(entity, "plain");
    m_html = findText(entity, "html");
    if (m_plainText.isEmpty() && m_html.isEmpty())
        m_plainText = QString::fromUtf8(output);
    if (entity.multipartType() == QMailMessagePartContainer::MultipartSigned
            && isProtocol(entity, "application/pgp-signature")) {
        if (m_attachedKey.isEmpty())
            m_attachedKey = findKey(entity);
        emit changed();
        // Its signed part from the decrypted text itself, as QMF's parser doesn't keep it.
        verifyMime(entity, firstPart(output, entity.contentType().boundary()), false);
        return;
    }
    setState(Done);
}

void PgpMessage::readSignature(const GpgJob *job)
{
    // A signature status line has the key's id first, then who it is.
    auto who = [](const QStringList &arguments) {
        return arguments.mid(1).join(QLatin1Char(' '));
    };
    if (job->hasStatus(QStringLiteral("GOODSIG"))) {
        m_signature = GoodSignature;
        m_signer = who(job->statusArguments(QStringLiteral("GOODSIG")));
    } else if (job->hasStatus(QStringLiteral("EXPKEYSIG")) || job->hasStatus(QStringLiteral("EXPSIG"))) {
        m_signature = ExpiredSignature;
        m_signer = who(job->statusArguments(job->hasStatus(QStringLiteral("EXPKEYSIG"))
                                            ? QStringLiteral("EXPKEYSIG") : QStringLiteral("EXPSIG")));
    } else if (job->hasStatus(QStringLiteral("REVKEYSIG"))) {
        m_signature = RevokedKey;
        m_signer = who(job->statusArguments(QStringLiteral("REVKEYSIG")));
    } else if (job->hasStatus(QStringLiteral("BADSIG"))) {
        m_signature = BadSignature;
        m_signer = who(job->statusArguments(QStringLiteral("BADSIG")));
    } else if (job->hasStatus(QStringLiteral("ERRSIG"))) {
        // The sixth argument is the reason, 9 for a public key that's missing (the signer's
        // fingerprint follows it, since GnuPG 2.2.7).
        const QStringList arguments = job->statusArguments(QStringLiteral("ERRSIG"));
        m_signature = arguments.value(5) == QLatin1String("9") || job->hasStatus(QStringLiteral("NO_PUBKEY"))
                ? UnknownKey : SignatureError;
        m_signer = arguments.value(0);
        m_fingerprint = arguments.value(6);
    }
    if (job->hasStatus(QStringLiteral("VALIDSIG")))
        m_fingerprint = job->statusArguments(QStringLiteral("VALIDSIG")).value(0);
    m_trusted = job->hasStatus(QStringLiteral("TRUST_ULTIMATE")) || job->hasStatus(QStringLiteral("TRUST_FULLY"));
}
