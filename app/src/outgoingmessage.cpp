// SPDX-License-Identifier: AGPL-3.0-or-later
#include "outgoingmessage.h"

#include "flowedtext.h"
#include "maildraft.h"

#include <qmailaccount.h>
#include <qmaildisconnected.h>
#include <qmailstore.h>

#include <QFileInfo>

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

bool OutgoingMessage::fail(const QString &error)
{
    qWarning("%s", qPrintable(error));
    m_error = error;
    emit errorChanged();
    return false;
}

bool OutgoingMessage::build(QMailMessage *message)
{
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
    message->setBody(QMailMessageBody::fromData(FlowedText::encode(m_body), type,
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
        message->setAttachments(parts);
    if (!m_attachments.isEmpty()) {
        for (const QString &path : m_attachments) {
            if (!QFileInfo(path).isFile())
                return fail(tr("%1 can't be found").arg(QFileInfo(path).fileName()));
        }
        message->addAttachments(m_attachments);
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
    if (!build(&message))
        return 0;
    // What the messageserver looks for in the outbox.
    message.setStatus(QMailMessage::Outgoing, true);
    message.setStatus(QMailMessage::Outbox | QMailMessage::Draft, true);
    message.setParentFolderId(QMailFolderId::LocalStorageFolderId);
    if (!QMailStore::instance()->addMessage(&message)) {
        fail(tr("The mail store didn't take it"));
        return 0;
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
