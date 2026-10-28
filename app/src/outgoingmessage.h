// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef OUTGOINGMESSAGE_H
#define OUTGOINGMESSAGE_H

#include <QObject>
#include <QStringList>

#include <qmailmessage.h>

// A mail being written, built with QMF instead of the email plugin, which only sends base64
// text/plain: the text goes out as format=flowed, quoted-printable.
//
// send() and saveDraft() store the message the way the plugin does; the caller then has the
// messageserver act on it through EmailAgent (processSendingQueue, exportUpdates) with the
// account id they return.
class OutgoingMessage : public QObject
{
    Q_OBJECT
    // The address to send from: one of an account's addresses or aliases, which picks the account.
    Q_PROPERTY(QString from READ from WRITE setFrom NOTIFY changed)
    Q_PROPERTY(QStringList to READ to WRITE setTo NOTIFY changed)
    Q_PROPERTY(QStringList cc READ cc WRITE setCc NOTIFY changed)
    Q_PROPERTY(QStringList bcc READ bcc WRITE setBcc NOTIFY changed)
    Q_PROPERTY(QString subject READ subject WRITE setSubject NOTIFY changed)
    // One line per paragraph, '>' lines for quotes.
    Q_PROPERTY(QString body READ body WRITE setBody NOTIFY changed)
    // EmailMessage.ResponseType, which has QMailMessage's values.
    Q_PROPERTY(int responseType READ responseType WRITE setResponseType NOTIFY changed)
    // The mail replied to or forwarded, 0 for none.
    Q_PROPERTY(int originalMessageId READ originalMessageId WRITE setOriginalMessageId NOTIFY changed)

public:
    explicit OutgoingMessage(QObject *parent = nullptr);

    QString from() const { return m_from; }
    void setFrom(const QString &from);
    QStringList to() const { return m_to; }
    void setTo(const QStringList &to);
    QStringList cc() const { return m_cc; }
    void setCc(const QStringList &cc);
    QStringList bcc() const { return m_bcc; }
    void setBcc(const QStringList &bcc);
    QString subject() const { return m_subject; }
    void setSubject(const QString &subject);
    QString body() const { return m_body; }
    void setBody(const QString &body);
    int responseType() const { return m_responseType; }
    void setResponseType(int type);
    int originalMessageId() const { return m_originalMessageId; }
    void setOriginalMessageId(int id);

    // Puts the message in the outbox. Returns its account's id, 0 when it couldn't be stored.
    Q_INVOKABLE int send();
    // Stores the message in the account's Drafts folder, replacing what an earlier call saved.
    // Returns its account's id, 0 when it couldn't be stored.
    Q_INVOKABLE int saveDraft();

signals:
    void changed();

private:
    bool build(QMailMessage *message) const;

    QString m_from;
    QStringList m_to;
    QStringList m_cc;
    QStringList m_bcc;
    QString m_subject;
    QString m_body;
    int m_responseType = 0;
    int m_originalMessageId = 0;
    QMailMessageId m_draftId;
};

#endif // OUTGOINGMESSAGE_H
