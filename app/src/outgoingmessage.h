// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef OUTGOINGMESSAGE_H
#define OUTGOINGMESSAGE_H

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

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
    // Paths of files to attach.
    Q_PROPERTY(QStringList attachments READ attachments WRITE setAttachments NOTIFY changed)
    // Attachments of the original to send along, as the email plugin's part locations. They must
    // have been downloaded.
    Q_PROPERTY(QStringList forwardedParts READ forwardedParts WRITE setForwardedParts NOTIFY changed)
    // Attachments of the draft loaded with loadDraft() to keep, as part locations.
    Q_PROPERTY(QStringList keptParts READ keptParts WRITE setKeptParts NOTIFY changed)
    // Sent signed with PGP/MIME, by the key for the From address; prepare() first.
    Q_PROPERTY(bool sign READ sign WRITE setSign NOTIFY changed)
    // Sent encrypted with PGP/MIME, to recipientKeys and to one's own key (signingKey); signed
    // inside when sign is set. prepare() first.
    Q_PROPERTY(bool encrypt READ encrypt WRITE setEncrypt NOTIFY changed)
    // The fingerprints of the recipients' keys.
    Q_PROPERTY(QStringList recipientKeys READ recipientKeys WRITE setRecipientKeys NOTIFY changed)
    // The fingerprint of the key to sign with; empty for the one for the From address.
    Q_PROPERTY(QString signingKey READ signingKey WRITE setSigningKey NOTIFY changed)
    // The signing key's public key goes along, as an attachment that's signed too.
    Q_PROPERTY(bool attachKey READ attachKey WRITE setAttachKey NOTIFY changed)
    // Why the last send(), saveDraft() or prepare() failed.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

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
    QStringList attachments() const { return m_attachments; }
    void setAttachments(const QStringList &paths);
    QStringList forwardedParts() const { return m_forwardedParts; }
    void setForwardedParts(const QStringList &locations);
    QStringList keptParts() const { return m_keptParts; }
    void setKeptParts(const QStringList &locations);
    bool sign() const { return m_sign; }
    void setSign(bool sign);
    bool encrypt() const { return m_encrypt; }
    void setEncrypt(bool encrypt);
    QStringList recipientKeys() const { return m_recipientKeys; }
    void setRecipientKeys(const QStringList &fingerprints);
    QString signingKey() const { return m_signingKey; }
    void setSigningKey(const QString &fingerprint);
    bool attachKey() const { return m_attachKey; }
    void setAttachKey(bool attach);
    QString error() const { return m_error; }

    // Continues a saved draft: sets everything from it, and makes saveDraft() replace it and
    // send() remove it. The body is as it was put together, see draftParts().
    Q_INVOKABLE bool loadDraft(int messageId);
    // The loaded draft's attachments: { location, name, size }.
    Q_INVOKABLE QVariantList draftAttachments() const;
    // The body split into { text, signature, attribution, quote, writtenBelowQuote }.
    Q_INVOKABLE QVariantMap draftParts() const;

    // Builds the message and signs and/or encrypts it, with the passphrase of one's key when
    // signing and it isn't remembered; prepared() or prepareFailed() follows. send() then sends
    // what was made.
    Q_INVOKABLE void prepare(const QString &passphrase = QString());

    // Puts the message in the outbox. Returns its account's id, 0 when it couldn't be stored.
    Q_INVOKABLE int send();
    // Stores the message in the account's Drafts folder, replacing what an earlier call saved or
    // the draft loaded. Returns its account's id, 0 when it couldn't be stored.
    Q_INVOKABLE int saveDraft();
    // The draft saveDraft() stored or loadDraft() loaded, 0 for none.
    Q_INVOKABLE int draftId() const { return int(m_draftId.toULongLong()); }

signals:
    void changed();
    void errorChanged();
    void prepared();
    // needsPassphrase: asking for the key's passphrase and trying again could do.
    void prepareFailed(bool needsPassphrase);

private:
    // The message, with its content (text, attachments) in content when that's given.
    bool build(QMailMessage *message, QMailMessagePartContainer *content = nullptr);
    bool fail(const QString &error);
    // What gpg is told to sign with: the key's fingerprint, or the From address.
    QString signer() const;
    void protect(const QString &passphrase, const QByteArray &publicKey, const QString &fingerprint);

    QString m_from;
    QStringList m_to;
    QStringList m_cc;
    QStringList m_bcc;
    QString m_subject;
    QString m_body;
    int m_responseType = 0;
    int m_originalMessageId = 0;
    QStringList m_attachments;
    QStringList m_forwardedParts;
    QStringList m_keptParts;
    QString m_error;
    QMailMessageId m_draftId;
    bool m_sign = false;
    bool m_encrypt = false;
    QStringList m_recipientKeys;
    QString m_signingKey;
    bool m_attachKey = false;
    // What prepare() made: the message, and its signed part's bytes when only signed.
    QMailMessage m_signedMessage;
    QByteArray m_signedData;
    bool m_prepared = false;
};

#endif // OUTGOINGMESSAGE_H
