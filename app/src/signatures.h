// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef SIGNATURES_H
#define SIGNATURES_H

#include <QObject>
#include <QSettings>
#include <QVariantList>

// Signatures of one's own, kept by Posthoorn next to the account's signature from Settings →
// Accounts, and which one each sending address uses. As the Signatures singleton.
//
// A choice is "account" (the account's signature, if it has one switched on: what Posthoorn did
// before there were signatures of its own), "none", or the id of one of these signatures. An
// address without a choice uses "account".
//
// Kept in ~/.config/nl.readefries/posthoorn/signatures.ini.
class Signatures : public QObject
{
    Q_OBJECT
    // { id, name, text } for each signature, in the order made.
    Q_PROPERTY(QVariantList list READ list NOTIFY changed)
    // Bumped on every change, for bindings that call the functions below.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit Signatures(QObject *parent = nullptr);

    QVariantList list() const;
    int revision() const { return m_revision; }

    // Returns the new signature's id.
    Q_INVOKABLE QString add(const QString &name, const QString &text);
    Q_INVOKABLE void update(const QString &id, const QString &name, const QString &text);
    // Addresses that used it go back to the account's signature.
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE bool exists(const QString &id) const;
    Q_INVOKABLE QString name(const QString &id) const;
    Q_INVOKABLE QString text(const QString &id) const;
    // The id of the signature with this text (leading and trailing white space aside), or "".
    Q_INVOKABLE QString find(const QString &text) const;

    Q_INVOKABLE QString choiceFor(const QString &address) const;
    Q_INVOKABLE void setChoiceFor(const QString &address, const QString &choice);

signals:
    void changed();

private:
    struct Signature
    {
        QString id;
        QString name;
        QString text;
    };

    int indexOf(const QString &id) const;
    void store();

    QSettings m_settings;
    QList<Signature> m_signatures;
    // Lower-case address to choice.
    QHash<QString, QString> m_choices;
    int m_nextId = 1;
    int m_revision = 0;
};

#endif // SIGNATURES_H
