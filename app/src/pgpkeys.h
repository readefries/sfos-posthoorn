// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PGPKEYS_H
#define PGPKEYS_H

#include <QObject>
#include <QVariantList>

class GpgJob;

// The keys in Posthoorn's keyring, as the PgpKeys QML type: listing, importing (from a file, such
// as an exported key or one attached to a mail) and removing them. One's own keys, those with a
// secret key, are trusted fully, so their signatures count as certified.
class PgpKeys : public QObject
{
    Q_OBJECT
    // { fingerprint, keyId, name (the first user id), userIds, secret, algorithm, created, expires,
    // expired, revoked }, one's own first.
    Q_PROPERTY(QVariantList keys READ keys NOTIFY keysChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // One of the keys is one's own, with its secret key.
    Q_PROPERTY(bool hasOwnKey READ hasOwnKey NOTIFY keysChanged)
    // Posthoorn's GnuPG (posthoorn-gnupg) is installed.
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit PgpKeys(QObject *parent = nullptr);

    QVariantList keys() const { return m_keys; }
    bool busy() const { return m_busy; }
    bool hasOwnKey() const;
    bool available() const;

    Q_INVOKABLE void refresh();
    // The fingerprint of one's own key (with its secret key) for address that can sign, empty
    // for none.
    Q_INVOKABLE QString signingKeyFor(const QString &address) const;
    // Imports the keys in the file at path; imported() tells how it went.
    Q_INVOKABLE void importFile(const QString &path);
    Q_INVOKABLE void remove(const QString &fingerprint);

signals:
    void keysChanged();
    void busyChanged();
    // count keys imported or updated; error is empty when it went well.
    void imported(int count, const QString &error);
    void removed(const QString &error);

private:
    void setBusy(bool busy);
    void listPublic();
    void onPublicListed(GpgJob *job);
    void onSecretListed(GpgJob *job);
    void trustOwnKeys(const QStringList &fingerprints);

    QVariantList m_keys;
    QVariantList m_pending;
    bool m_busy = false;
};

#endif // PGPKEYS_H
