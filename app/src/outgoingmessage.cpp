// SPDX-License-Identifier: AGPL-3.0-or-later
#include "outgoingmessage.h"

#include "flowedtext.h"

#include <qmailaccount.h>
#include <qmaildisconnected.h>
#include <qmailstore.h>

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

bool OutgoingMessage::build(QMailMessage *message) const
{
    QMailAddress sender;
    const QMailAccount account = accountFor(m_from, &sender);
    if (!account.id().isValid()) {
        qWarning("no enabled account sends from %s", qPrintable(m_from));
        return false;
    }

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

    const QMailMessageId original(m_originalMessageId);
    if (original.isValid()) {
        message->setInResponseTo(original);
        message->setResponseType(static_cast<QMailMessage::ResponseType>(m_responseType));
        // In-Reply-To and References, so clients show the reply in its thread.
        message->setReplyReferences(QMailMessage(original));
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
        qWarning("could not put the message in the outbox");
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

    bool stored;
    if (m_draftId.isValid()) {
        message.setId(m_draftId);
        stored = QMailStore::instance()->updateMessage(&message);
    } else {
        stored = QMailStore::instance()->addMessage(&message);
    }
    if (!stored) {
        qWarning("could not save the draft");
        return 0;
    }
    m_draftId = message.id();

    // So the next sync puts it in the server's Drafts folder.
    QMailDisconnected::flagMessage(message.id(), QMailMessage::Draft, QMailMessage::Temporary,
                                   QStringLiteral("Flagging message as draft"));
    QMailDisconnected::moveToFolder(QMailMessageIdList() << message.id(), message.parentFolderId());
    return int(message.parentAccountId().toULongLong());
}
