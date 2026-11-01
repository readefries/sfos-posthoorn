// SPDX-License-Identifier: AGPL-3.0-or-later
#include "outgoingmessage.h"

#include "flowedtext.h"
#include "gpgjob.h"
#include "maildraft.h"

#include <qmailaccount.h>
#include <qmaildisconnected.h>
#include <qmailstore.h>

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace {

// The enabled account that address belongs to, as its main address or one of its aliases, and
// the address to send from (with the account's name).
QMailAccount accountFor(const QString &address, QMailAddress *sender)
{
    const QMailAccountIdList ids = QMailStore::instance()->queryAccounts(
                QMailAccountKey::messageType(QMailMessage::Email)
                & QMailAccountKey::status(QMailAccount::Enabled),
                QMailAccountSortKey::name());
    for (const QMailAccountId &id : ids) {
        const QMailAccount account(id);
        const QMailAddress main = account.fromAddress();
        if (main.address().compare(address, Qt::CaseInsensitive) == 0) {
            *sender = main;
            return account;
        }
        const QMailAddress aliases = account.fromAliases();
        const QList<QMailAddress> members = aliases.isGroup() ? aliases.groupMembers()
                                                              : QList<QMailAddress>() << aliases;
        for (const QMailAddress &alias : members) {
            if (!alias.isNull() && alias.address().compare(address, Qt::CaseInsensitive) == 0) {
                *sender = QMailAddress(alias.name().isEmpty() ? main.name() : alias.name(),
                                       alias.address());
                return account;
            }
        }
    }
    return QMailAccount();
}

// The micalg of a PGP/MIME signature, from the hash algorithm gpg reports (RFC 4880, 9.4).
QByteArray micalg(const QString &hashAlgorithm)
{
    switch (hashAlgorithm.toInt()) {
    case 2: return "pgp-sha1";
    case 3: return "pgp-ripemd160";
    case 9: return "pgp-sha384";
    case 10: return "pgp-sha512";
    case 11: return "pgp-sha224";
    default: return "pgp-sha256";
    }
}

} // namespace

OutgoingMessage::OutgoingMessage(QObject *parent)
    : QObject(parent)
{
}

void OutgoingMessage::setFrom(const QString &from)
{
    if (from != m_from) {
        m_from = from;
        emit changed();
    }
}

void OutgoingMessage::setTo(const QStringList &to)
{
    if (to != m_to) {
        m_to = to;
        emit changed();
    }
}

void OutgoingMessage::setCc(const QStringList &cc)
{
    if (cc != m_cc) {
        m_cc = cc;
        emit changed();
    }
}

void OutgoingMessage::setBcc(const QStringList &bcc)
{
    if (bcc != m_bcc) {
        m_bcc = bcc;
        emit changed();
    }
}

void OutgoingMessage::setSubject(const QString &subject)
{
    if (subject != m_subject) {
        m_subject = subject;
        emit changed();
    }
}

void OutgoingMessage::setBody(const QString &body)
{
    if (body != m_body) {
        m_body = body;
        emit changed();
    }
}

void OutgoingMessage::setResponseType(int type)
{
    if (type != m_responseType) {
        m_responseType = type;
        emit changed();
    }
}

void OutgoingMessage::setOriginalMessageId(int id)
{
    if (id != m_originalMessageId) {
        m_originalMessageId = id;
        emit changed();
    }
}

void OutgoingMessage::setAttachments(const QStringList &paths)
{
    if (paths != m_attachments) {
        m_attachments = paths;
        emit changed();
    }
}

void OutgoingMessage::setForwardedParts(const QStringList &locations)
{
    if (locations != m_forwardedParts) {
        m_forwardedParts = locations;
        emit changed();
    }
}

void OutgoingMessage::setKeptParts(const QStringList &locations)
{
    if (locations != m_keptParts) {
        m_keptParts = locations;
        emit changed();
    }
}

bool OutgoingMessage::loadDraft(int messageId)
{
    const QMailMessageId id(messageId);
    if (!id.isValid())
        return false;
    QMailMessage draft(id);
    if (!draft.id().isValid())
        return false;

    m_draftId = id;
    m_from = draft.from().address();
    m_to = QMailAddress::toStringList(draft.to());
    m_cc = QMailAddress::toStringList(draft.cc());
    m_bcc = QMailAddress::toStringList(draft.bcc());
    m_subject = draft.subject();
    m_responseType = draft.responseType();
    m_originalMessageId = int(draft.inResponseTo().toULongLong());
    m_attachments.clear();
    m_forwardedParts.clear();

    m_body.clear();
    if (const QMailMessagePartContainer *text = draft.findPlainTextContainer()) {
        m_body = text->body().data();
        if (text->contentType().parameter("format").toLower() == "flowed")
            m_body = FlowedText::decode(m_body);
    }

    m_keptParts.clear();
    for (const QMailMessagePart::Location &location : draft.findAttachmentLocations())
        m_keptParts.append(location.toString(true));
    emit changed();
    return true;
}

QVariantList OutgoingMessage::draftAttachments() const
{
    QVariantList result;
    if (!m_draftId.isValid())
        return result;
    QMailMessage draft(m_draftId);
    for (const QMailMessagePart::Location &location : draft.findAttachmentLocations()) {
        const QMailMessagePart &part = draft.partAt(location);
        QVariantMap attachment;
        attachment.insert(QStringLiteral("location"), location.toString(true));
        attachment.insert(QStringLiteral("name"), part.displayName());
        const int size = part.contentDisposition().size();
        attachment.insert(QStringLiteral("size"), size > 0 ? size : int(part.body().length()));
        result.append(attachment);
    }
    return result;
}

QVariantMap OutgoingMessage::draftParts() const
{
    const MailDraft draft = MailDraft::split(m_body);
    QVariantMap parts;
    parts.insert(QStringLiteral("text"), draft.text);
    parts.insert(QStringLiteral("signature"), draft.signature);
    parts.insert(QStringLiteral("attribution"), draft.attribution);
    parts.insert(QStringLiteral("quote"), draft.quote);
    parts.insert(QStringLiteral("writtenBelowQuote"), draft.writtenBelowQuote);
    return parts;
}

void OutgoingMessage::setSigningKey(const QString &fingerprint)
{
    if (fingerprint == m_signingKey)
        return;
    m_signingKey = fingerprint;
    m_prepared = false;
    emit changed();
}

void OutgoingMessage::setAttachKey(bool attach)
{
    if (attach == m_attachKey)
        return;
    m_attachKey = attach;
    m_prepared = false;
    emit changed();
}

void OutgoingMessage::setEncrypt(bool encrypt)
{
    if (encrypt == m_encrypt)
        return;
    m_encrypt = encrypt;
    m_prepared = false;
    emit changed();
}

void OutgoingMessage::setRecipientKeys(const QStringList &fingerprints)
{
    if (fingerprints == m_recipientKeys)
        return;
    m_recipientKeys = fingerprints;
    m_prepared = false;
    emit changed();
}

void OutgoingMessage::setSign(bool sign)
{
    if (sign == m_sign)
        return;
    m_sign = sign;
    m_prepared = false;
    emit changed();
}

bool OutgoingMessage::fail(const QString &error)
{
    if (!error.isEmpty())
        qWarning("%s", qPrintable(error));
    m_error = error;
    emit errorChanged();
    return false;
}

bool OutgoingMessage::build(QMailMessage *message, QMailMessagePartContainer *content)
{
    if (!content)
        content = message;
    if (!m_error.isEmpty()) {
        m_error.clear();
        emit errorChanged();
    }

    QMailAddress sender;
    const QMailAccount account = accountFor(m_from, &sender);
    if (!account.id().isValid())
        return fail(tr("No account sends from %1").arg(m_from));

    message->setMessageType(QMailMessage::Email);
    message->setParentAccountId(account.id());
    message->setFrom(sender);
    message->setTo(QMailAddress::fromStringList(m_to));
    message->setCc(QMailAddress::fromStringList(m_cc));
    message->setBcc(QMailAddress::fromStringList(m_bcc));
    message->setSubject(m_subject);
    message->setDate(QMailTimeStamp::currentDateTime());

    QMailMessageContentType type("text/plain");
    type.setParameter("charset", "UTF-8");
    type.setParameter("format", "flowed");
    content->setBody(QMailMessageBody::fromData(FlowedText::encode(m_body), type,
                                                QMailMessageBody::QuotedPrintable));

    const QMailMessageId originalId(m_originalMessageId);
    QMailMessage original;
    if (originalId.isValid()) {
        original = QMailMessage(originalId);
        message->setInResponseTo(originalId);
        message->setResponseType(static_cast<QMailMessage::ResponseType>(m_responseType));
        // In-Reply-To and References, so clients show the reply in its thread.
        message->setReplyReferences(original);
    }

    // Both turn the message into multipart/mixed, with the text as its first part. Rather not
    // sent than sent without what it says is attached.
    QMailMessage draft;
    if (m_draftId.isValid() && !m_keptParts.isEmpty())
        draft = QMailMessage(m_draftId);
    QList<const QMailMessagePart *> parts;
    auto take = [&parts](const QMailMessage &from, const QStringList &locations) {
        for (const QString &location : locations) {
            const QMailMessagePart::Location partLocation(location);
            if (!from.contains(partLocation))
                return false;
            parts.append(&from.partAt(partLocation));
        }
        return true;
    };
    if (!take(original, m_forwardedParts))
        return fail(tr("An attachment of the original is gone"));
    if (!take(draft, m_keptParts))
        return fail(tr("An attachment of the draft is gone"));
    for (const QMailMessagePart *part : parts) {
        if (!part->hasBody())
            return fail(tr("%1 hasn't been downloaded").arg(part->displayName()));
    }
    if (!parts.isEmpty())
        content->setAttachments(parts);
    if (!m_attachments.isEmpty()) {
        for (const QString &path : m_attachments) {
            if (!QFileInfo(path).isFile())
                return fail(tr("%1 can't be found").arg(QFileInfo(path).fileName()));
        }
        content->addAttachments(m_attachments);
    }

    message->setStatus(QMailMessage::ContentAvailable, true);
    message->setStatus(QMailMessage::PartialContentAvailable, true);
    message->setStatus(QMailMessage::Read, true);
    // Only on the device until it's sent or synced to the server's Drafts.
    message->setStatus(QMailMessage::LocalOnly, true);
    message->setSize(message->indicativeSize() * 1024);
    return true;
}

int OutgoingMessage::send()
{
    QMailMessage message;
    if (m_sign || m_encrypt) {
        if (!m_prepared) {
            fail(m_encrypt ? tr("The mail wasn't encrypted") : tr("The mail wasn't signed"));
            return 0;
        }
        message = m_signedMessage;
    } else if (!build(&message)) {
        return 0;
    }
    // What the messageserver looks for in the outbox.
    message.setStatus(QMailMessage::Outgoing, true);
    message.setStatus(QMailMessage::Outbox | QMailMessage::Draft, true);
    message.setParentFolderId(QMailFolderId::LocalStorageFolderId);
    if (!QMailStore::instance()->addMessage(&message)) {
        fail(tr("The mail store didn't take it"));
        return 0;
    }
    // The signed part goes out as it was signed only when the store kept it so.
    if (m_sign && !m_encrypt) {
        const QMailMessage stored(message.id());
        if (stored.partCount() < 1 || stored.partAt(0).undecodedData() != m_signedData) {
            QMailStore::instance()->removeMessage(message.id(), QMailStore::NoRemovalRecord);
            fail(tr("The mail store changed the signed mail"));
            return 0;
        }
    }

    if (m_draftId.isValid()) {
        QMailStore::instance()->removeMessage(m_draftId, QMailStore::CreateRemovalRecord);
        m_draftId = QMailMessageId();
    }

    const QMailMessageId original(m_originalMessageId);
    if (original.isValid()) {
        quint64 flag = 0;
        switch (m_responseType) {
        case QMailMessage::Reply:
            flag = QMailMessage::Replied;
            break;
        case QMailMessage::ReplyToAll:
            flag = QMailMessage::RepliedAll;
            break;
        case QMailMessage::Forward:
        case QMailMessage::ForwardPart:
            flag = QMailMessage::Forwarded;
            break;
        default:
            break;
        }
        if (flag)
            QMailDisconnected::flagMessage(original, flag, 0, QStringLiteral("Marking message as replied/forwarded"));
    }
    return int(message.parentAccountId().toULongLong());
}

void OutgoingMessage::prepare(const QString &passphrase)
{
    m_prepared = false;
    if (!m_attachKey) {
        protect(passphrase, QByteArray(), QString());
        return;
    }
    // The public key first, to go along in what's signed.
    GpgJob *job = new GpgJob(this);
    connect(job, &GpgJob::finished, this, [this, job, passphrase]() {
        job->deleteLater();
        const QString fingerprint = job->statusArguments(QStringLiteral("EXPORTED")).value(0);
        if (job->exitCode() != 0 || job->output().isEmpty() || fingerprint.isEmpty()) {
            qWarning() << "gpg --export:" << job->status() << job->errors();
            fail(tr("Your public key couldn't be attached"));
            emit prepareFailed(false);
            return;
        }
        protect(passphrase, job->output(), fingerprint);
    });
    job->start(QStringList() << QStringLiteral("--armor") << QStringLiteral("--export-options")
               << QStringLiteral("export-minimal") << QStringLiteral("--export") << signer());
}

QString OutgoingMessage::signer() const
{
    if (!m_signingKey.isEmpty())
        return m_signingKey;
    QMailAddress sender;
    accountFor(m_from, &sender);
    return QLatin1Char('<') + (sender.isNull() ? m_from : sender.address()) + QLatin1Char('>');
}

void OutgoingMessage::protect(const QString &passphrase, const QByteArray &publicKey,
                              const QString &fingerprint)
{
    QMailMessage message;
    QMailMessagePart content;
    if (!build(&message, &content)) {
        emit prepareFailed(false);
        return;
    }
    // Every recipient has to have a key, or it isn't sent at all.
    if (m_encrypt && (m_recipientKeys.isEmpty() || m_recipientKeys.contains(QString())
                      || m_recipientKeys.size() != m_to.size() + m_cc.size() + m_bcc.size())) {
        fail(tr("Not every recipient has a key to encrypt to"));
        emit prepareFailed(false);
        return;
    }
    if (!publicKey.isEmpty()) {
        // As Thunderbird attaches it.
        const QByteArray name = "OpenPGP_0x" + fingerprint.right(16).toLatin1() + ".asc";
        QMailMessageContentType type("application/pgp-keys");
        type.setName(name);
        QMailMessageContentDisposition disposition(QMailMessageContentDisposition::Attachment);
        disposition.setFilename(name);
        QMailMessagePart key = QMailMessagePart::fromData(publicKey, disposition, type,
                                                          QMailMessageBody::SevenBit);
        key.setContentDescription(QStringLiteral("OpenPGP public key"));
        if (content.multipartType() != QMailMessagePartContainer::MultipartMixed) {
            QMailMessagePart text;
            text.setBody(content.body());
            content = QMailMessagePart();
            content.setMultipartType(QMailMessagePartContainer::MultipartMixed);
            content.appendPart(text);
        }
        content.appendPart(key);
    }
    // Signed as QMF writes it, and then kept so, which QMF sends as it is (as for received mail).
    // Encrypted, it's what the recipient gets back.
    const QByteArray data = GpgJob::canonical(content.toRfc2822());

    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cache);
    QTemporaryFile file(cache + QStringLiteral("/protecting-XXXXXX"));
    file.setAutoRemove(false);
    if (!file.open() || file.write(data) != data.size()) {
        fail(tr("The mail couldn't be stored for signing or encrypting"));
        emit prepareFailed(false);
        return;
    }
    file.close();
    const QString path = file.fileName();

    // The passphrase on stdin, so the data from a file.
    const QString given = !m_sign ? QString() : passphrase.isEmpty() ? GpgJob::passphrase() : passphrase;
    QStringList arguments;
    QByteArray input;
    if (!given.isEmpty()) {
        arguments << QStringLiteral("--passphrase-fd") << QStringLiteral("0");
        input = given.toUtf8() + '\n';
    }
    arguments << QStringLiteral("--armor");
    if (m_encrypt) {
        // To the recipients' keys, and one's own, so the copy in Sent can be read. Their keys
        // were chosen in the composer, certified or not.
        arguments << QStringLiteral("--encrypt") << QStringLiteral("--trust-model") << QStringLiteral("always");
        QStringList recipients = m_recipientKeys;
        recipients << signer();
        recipients.removeDuplicates();
        for (const QString &recipient : recipients)
            arguments << QStringLiteral("--recipient") << recipient;
        // Signed inside.
        if (m_sign)
            arguments << QStringLiteral("--sign");
    } else {
        arguments << QStringLiteral("--detach-sign");
    }
    if (m_sign) {
        arguments << QStringLiteral("--digest-algo") << QStringLiteral("SHA256")
                  << QStringLiteral("--local-user") << signer();
    }
    arguments << QStringLiteral("--output") << QStringLiteral("-") << path;

    const QString address = message.from().address();
    GpgJob *job = new GpgJob(this);
    connect(job, &GpgJob::finished, this, [this, job, message, content, data, path, given, address]() mutable {
        job->deleteLater();
        QFile::remove(path);
        const bool done = job->exitCode() == 0
                && (!m_sign || job->hasStatus(QStringLiteral("SIG_CREATED")))
                && (!m_encrypt || job->hasStatus(QStringLiteral("END_ENCRYPTION")));
        if (!done) {
            qWarning() << "gpg signing/encrypting:" << job->status() << job->errors();
            if (job->hasStatus(QStringLiteral("INV_RECP"))) {
                fail(tr("A recipient's key can't be used for encrypting"));
                emit prepareFailed(false);
            } else if (job->hasStatus(QStringLiteral("INV_SGNR")) || job->hasStatus(QStringLiteral("NO_SECKEY"))) {
                fail(tr("None of your keys is for %1").arg(address));
                emit prepareFailed(false);
            } else if (m_sign) {
                fail(given.isEmpty() ? QString() : tr("That passphrase didn't work"));
                emit prepareFailed(true);
            } else {
                fail(tr("The mail couldn't be encrypted"));
                emit prepareFailed(false);
            }
            return;
        }
        if (!given.isEmpty())
            GpgJob::rememberPassphrase(given);

        if (m_encrypt) {
            // RFC 3156: a version part, then the ciphertext.
            QMailMessageContentType versionType("application/pgp-encrypted");
            QMailMessagePart version = QMailMessagePart::fromData(QByteArray("Version: 1\r\n"),
                                                                  QMailMessageContentDisposition(),
                                                                  versionType, QMailMessageBody::SevenBit);
            version.setContentDescription(QStringLiteral("PGP/MIME version identification"));
            QMailMessageContentType type("application/octet-stream");
            type.setName("encrypted.asc");
            QMailMessageContentDisposition disposition(QMailMessageContentDisposition::Inline);
            disposition.setFilename("encrypted.asc");
            QMailMessagePart encrypted = QMailMessagePart::fromData(job->output(), disposition, type,
                                                                    QMailMessageBody::SevenBit);
            encrypted.setContentDescription(QStringLiteral("OpenPGP encrypted message"));
            message.setMultipartType(QMailMessagePartContainer::MultipartEncrypted,
                                     QList<QMailMessageHeaderField::ParameterType>()
                                     << qMakePair(QByteArray("protocol"), QByteArray("application/pgp-encrypted")));
            message.appendPart(version);
            message.appendPart(encrypted);
            m_signedData.clear();
        } else {
            QMailMessageContentType type("application/pgp-signature");
            type.setName("signature.asc");
            QMailMessageContentDisposition disposition(QMailMessageContentDisposition::Attachment);
            disposition.setFilename("signature.asc");
            QMailMessagePart signature = QMailMessagePart::fromData(job->output(), disposition, type,
                                                                    QMailMessageBody::SevenBit);
            signature.setContentDescription(QStringLiteral("OpenPGP digital signature"));

            content.setUndecodedData(data);
            const QStringList created = job->statusArguments(QStringLiteral("SIG_CREATED"));
            message.setMultipartType(QMailMessagePartContainer::MultipartSigned,
                                     QList<QMailMessageHeaderField::ParameterType>()
                                     << qMakePair(QByteArray("protocol"), QByteArray("application/pgp-signature"))
                                     << qMakePair(QByteArray("micalg"), micalg(created.value(2))));
            message.appendPart(content);
            message.appendPart(signature);
            m_signedData = data;
        }
        message.setSize(message.indicativeSize() * 1024);
        m_signedMessage = message;
        m_prepared = true;
        emit prepared();
    });
    job->start(arguments, input);
}

int OutgoingMessage::saveDraft()
{
    QMailMessage message;
    if (!build(&message))
        return 0;

    const QMailAccount account(message.parentAccountId());
    const QMailFolderId drafts = account.standardFolder(QMailFolder::DraftsFolder);
    message.setParentFolderId(drafts.isValid() ? drafts : QMailFolderId::LocalStorageFolderId);
    message.setStatus(QMailMessage::Outgoing, false);
    message.setStatus(QMailMessage::Outbox, false);
    message.setStatus(QMailMessage::Draft, true);

    // A new message replaces an earlier version, rather than updating it: the freshly built
    // message lacks the stored one's content location, which updateMessage() needs.
    if (!QMailStore::instance()->addMessage(&message)) {
        fail(tr("The mail store didn't take it"));
        return 0;
    }
    if (m_draftId.isValid())
        QMailStore::instance()->removeMessage(m_draftId, QMailStore::CreateRemovalRecord);
    m_draftId = message.id();

    // So the next sync puts it in the server's Drafts folder.
    QMailDisconnected::flagMessage(message.id(), QMailMessage::Draft, QMailMessage::Temporary,
                                   QStringLiteral("Flagging message as draft"));
    QMailDisconnected::moveToFolder(QMailMessageIdList() << message.id(), message.parentFolderId());
    return int(message.parentAccountId().toULongLong());
}
