// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PGPMESSAGE_H
#define PGPMESSAGE_H

#include <QObject>
#include <QPointer>

#include <qmailmessage.h>

class GpgJob;
class ImapPartFetcher;

// A mail's OpenPGP side, for the reader: PGP/MIME (RFC 3156) or inline, signed or encrypted.
// Checks the signature as soon as the mail's content is there; decrypts it when asked, or by
// itself when gpg-agent still has the passphrase. With Posthoorn's own GnuPG (GpgJob).
class PgpMessage : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int messageId READ messageId WRITE setMessageId NOTIFY messageIdChanged)
    Q_PROPERTY(Kind kind READ kind NOTIFY changed)
    Q_PROPERTY(State state READ state NOTIFY changed)
    Q_PROPERTY(Signature signature READ signature NOTIFY changed)
    // Who signed it, as the key says ("Jan <jan@example.org>"), or the key's id when unknown.
    Q_PROPERTY(QString signer READ signer NOTIFY changed)
    Q_PROPERTY(QString fingerprint READ fingerprint NOTIFY changed)
    // The signing key is certified: one's own, or certified by it.
    Q_PROPERTY(bool trusted READ trusted NOTIFY changed)
    // The decrypted mail's text: plain, or HTML when that's what it has.
    Q_PROPERTY(QString plainText READ plainText NOTIFY changed)
    Q_PROPERTY(QString html READ html NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // The mail carries a public key (application/pgp-keys), such as the signer's.
    Q_PROPERTY(bool hasAttachedKey READ hasAttachedKey NOTIFY changed)

public:
    enum Kind { NotPgp, Signed, Encrypted };
    Q_ENUM(Kind)
    // NeedsContent: the mail's content has to be downloaded first.
    enum State { Idle, NeedsContent, Working, NeedsPassphrase, Done, Failed };
    Q_ENUM(State)
    enum Signature { NoSignature, GoodSignature, BadSignature, UnknownKey, ExpiredSignature,
                     RevokedKey, SignatureError };
    Q_ENUM(Signature)

    explicit PgpMessage(QObject *parent = nullptr);

    int messageId() const { return m_messageId; }
    void setMessageId(int id);
    Kind kind() const { return m_kind; }
    State state() const { return m_state; }
    Signature signature() const { return m_signature; }
    QString signer() const { return m_signer; }
    QString fingerprint() const { return m_fingerprint; }
    bool trusted() const { return m_trusted; }
    QString plainText() const { return m_plainText; }
    QString html() const { return m_html; }
    QString error() const { return m_error; }
    bool hasAttachedKey() const { return !m_attachedKey.isEmpty(); }

    // Decrypts an encrypted mail, with the passphrase of one's key when gpg-agent doesn't have it.
    Q_INVOKABLE void decrypt(const QString &passphrase = QString());
    // Checks the mail again, as after importing a key.
    Q_INVOKABLE void recheck();
    // Writes the attached key to a file in the cache, for importing; returns its path.
    Q_INVOKABLE QString saveAttachedKey();

signals:
    void messageIdChanged();
    void changed();

private:
    enum Form { None, MimeSigned, MimeEncrypted, InlineSigned, InlineEncrypted };

    void inspect();
    void onMessagesUpdated(const QMailMessageIdList &ids);
    void checkSigned(const QMailMessage &message);
    void verifyMime(const QMailMessagePartContainer &container, const QByteArray &signedData, bool rebuilt);
    void onVerified();
    void onDecrypted();
    void readSignature(const GpgJob *job);
    void showDecrypted(const QByteArray &output);
    void setState(State state, const QString &error = QString());
    GpgJob *newJob();

    int m_messageId = 0;
    Form m_form = None;
    Kind m_kind = NotPgp;
    State m_state = Idle;
    Signature m_signature = NoSignature;
    QString m_signer;
    QString m_fingerprint;
    bool m_trusted = false;
    QString m_plainText;
    QString m_html;
    QString m_error;
    // Inline PGP's armored block, or the PGP/MIME ciphertext.
    QByteArray m_armored;
    QByteArray m_attachedKey;
    bool m_triedWithoutPassphrase = false;
    // The signed part checked was QMF's rebuilt one, not the original.
    bool m_rebuilt = false;
    QPointer<GpgJob> m_job;
    QPointer<ImapPartFetcher> m_fetcher;
    QString m_tempFile;
};

#endif // PGPMESSAGE_H
