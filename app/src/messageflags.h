// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MESSAGEFLAGS_H
#define MESSAGEFLAGS_H

#include <QObject>

#include <qmaildisconnected.h>
#include <qmailmessage.h>

// Starring mail, as the MessageFlags singleton: QMF's Important flag, which is IMAP's \Flagged.
// The email plugin can't set it. The server gets it with the account's next
// EmailAgent.exportUpdates().
class MessageFlags : public QObject
{
    Q_OBJECT

public:
    explicit MessageFlags(QObject *parent = nullptr) : QObject(parent) { }

    Q_INVOKABLE bool isStarred(int messageId) const
    {
        return messageId > 0
                && (QMailMessageMetaData(QMailMessageId(messageId)).status() & QMailMessage::Important) != 0;
    }

    // Returns the account to export the change for.
    Q_INVOKABLE int setStarred(int messageId, bool starred) const
    {
        const QMailMessageId id(messageId);
        QMailDisconnected::flagMessage(id, starred ? QMailMessage::Important : 0,
                                       starred ? 0 : QMailMessage::Important,
                                       starred ? QStringLiteral("Starring message")
                                               : QStringLiteral("Unstarring message"));
        return int(QMailMessageMetaData(id).parentAccountId().toULongLong());
    }
};

#endif // MESSAGEFLAGS_H
