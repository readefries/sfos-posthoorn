// SPDX-License-Identifier: AGPL-3.0-or-later
#include "pgpkeys.h"

#include "gpgjob.h"

#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QVariantMap>

#include <algorithm>

namespace {

QString algorithmName(const QString &number, const QString &curve)
{
    if (!curve.isEmpty())
        return curve;
    switch (number.toInt()) {
    case 1:
    case 2:
    case 3:
        return QStringLiteral("RSA");
    case 16:
        return QStringLiteral("ElGamal");
    case 17:
        return QStringLiteral("DSA");
    default:
        return number;
    }
}

QDateTime fromEpoch(const QString &value)
{
    return value.isEmpty() ? QDateTime() : QDateTime::fromMSecsSinceEpoch(value.toLongLong() * 1000);
}

// gpg --with-colons escapes some characters in user ids as \xNN.
QString unescape(const QString &value)
{
    QByteArray result;
    const QByteArray bytes = value.toUtf8();
    for (int i = 0; i < bytes.size(); ++i) {
        if (bytes.at(i) == '\\' && i + 3 < bytes.size() && bytes.at(i + 1) == 'x') {
            bool ok = false;
            const char c = char(bytes.mid(i + 2, 2).toInt(&ok, 16));
            if (ok) {
                result += c;
                i += 3;
                continue;
            }
        }
        result += bytes.at(i);
    }
    return QString::fromUtf8(result);
}

} // namespace

PgpKeys::PgpKeys(QObject *parent)
    : QObject(parent)
{
    refresh();
}

bool PgpKeys::available() const
{
    return GpgJob::available();
}

void PgpKeys::setBusy(bool busy)
{
    if (busy != m_busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

bool PgpKeys::hasOwnKey() const
{
    for (const QVariant &entry : m_keys) {
        if (entry.toMap().value(QStringLiteral("secret")).toBool())
            return true;
    }
    return false;
}

QString PgpKeys::signingKeyFor(const QString &address) const
{
    if (address.isEmpty())
        return QString();
    const QString wanted = QLatin1Char('<') + address.toLower() + QLatin1Char('>');
    for (const QVariant &entry : m_keys) {
        const QVariantMap key = entry.toMap();
        if (!key.value(QStringLiteral("secret")).toBool() || key.value(QStringLiteral("expired")).toBool()
                || key.value(QStringLiteral("revoked")).toBool())
            continue;
        for (const QString &userId : key.value(QStringLiteral("userIds")).toStringList()) {
            if (userId.toLower().contains(wanted))
                return key.value(QStringLiteral("fingerprint")).toString();
        }
    }
    return QString();
}

void PgpKeys::refresh()
{
    if (!available())
        return;
    setBusy(true);
    listPublic();
}

void PgpKeys::listPublic()
{
    GpgJob *job = new GpgJob(this);
    connect(job, &GpgJob::finished, this, [this, job]() { onPublicListed(job); });
    job->start(QStringList() << QStringLiteral("--with-colons") << QStringLiteral("--fixed-list-mode")
                             << QStringLiteral("--list-keys"));
}

void PgpKeys::onPublicListed(GpgJob *job)
{
    job->deleteLater();
    // pub, then fpr and uid lines belonging to it; sub keys are left out.
    m_pending.clear();
    QVariantMap key;
    bool inSubkey = false;
    auto flush = [&]() {
        if (!key.isEmpty())
            m_pending.append(key);
        key.clear();
    };
    for (const QByteArray &rawLine : job->output().split('\n')) {
        const QStringList fields = QString::fromUtf8(rawLine).split(QLatin1Char(':'));
        const QString type = fields.value(0);
        if (type == QLatin1String("pub")) {
            flush();
            inSubkey = false;
            const QString validity = fields.value(1);
            key.insert(QStringLiteral("keyId"), fields.value(4));
            key.insert(QStringLiteral("algorithm"), algorithmName(fields.value(3), fields.value(16)));
            key.insert(QStringLiteral("created"), fromEpoch(fields.value(5)));
            key.insert(QStringLiteral("expires"), fromEpoch(fields.value(6)));
            key.insert(QStringLiteral("expired"), validity == QLatin1String("e"));
            key.insert(QStringLiteral("revoked"), validity == QLatin1String("r"));
            key.insert(QStringLiteral("userIds"), QStringList());
            key.insert(QStringLiteral("secret"), false);
        } else if (type == QLatin1String("sub")) {
            inSubkey = true;
        } else if (type == QLatin1String("fpr") && !inSubkey && !key.contains(QStringLiteral("fingerprint"))) {
            key.insert(QStringLiteral("fingerprint"), fields.value(9));
        } else if (type == QLatin1String("uid") && !key.isEmpty()) {
            QStringList userIds = key.value(QStringLiteral("userIds")).toStringList();
            userIds.append(unescape(fields.value(9)));
            key.insert(QStringLiteral("userIds"), userIds);
            if (!key.contains(QStringLiteral("name")))
                key.insert(QStringLiteral("name"), userIds.first());
        }
    }
    flush();

    GpgJob *secret = new GpgJob(this);
    connect(secret, &GpgJob::finished, this, [this, secret]() { onSecretListed(secret); });
    secret->start(QStringList() << QStringLiteral("--with-colons") << QStringLiteral("--fixed-list-mode")
                                << QStringLiteral("--list-secret-keys"));
}

void PgpKeys::onSecretListed(GpgJob *job)
{
    job->deleteLater();
    QSet<QString> secret;
    bool inSubkey = false;
    for (const QByteArray &rawLine : job->output().split('\n')) {
        const QStringList fields = QString::fromUtf8(rawLine).split(QLatin1Char(':'));
        if (fields.value(0) == QLatin1String("sec"))
            inSubkey = false;
        else if (fields.value(0) == QLatin1String("ssb"))
            inSubkey = true;
        else if (fields.value(0) == QLatin1String("fpr") && !inSubkey)
            secret.insert(fields.value(9));
    }
    for (QVariant &entry : m_pending) {
        QVariantMap key = entry.toMap();
        key.insert(QStringLiteral("secret"), secret.contains(key.value(QStringLiteral("fingerprint")).toString()));
        entry = key;
    }
    // One's own first, then by name.
    std::stable_sort(m_pending.begin(), m_pending.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap left = a.toMap();
        const QVariantMap right = b.toMap();
        if (left.value(QStringLiteral("secret")).toBool() != right.value(QStringLiteral("secret")).toBool())
            return left.value(QStringLiteral("secret")).toBool();
        return left.value(QStringLiteral("name")).toString().compare(
                    right.value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
    });
    // Only a real change, as open mail checks its signature again on one.
    const bool changed = m_pending != m_keys;
    m_keys = m_pending;
    m_pending.clear();
    setBusy(false);
    if (changed)
        emit keysChanged();
}

void PgpKeys::importFile(const QString &path)
{
    if (!available()) {
        emit imported(0, tr("PGP needs posthoorn-gnupg, which isn't installed"));
        return;
    }
    setBusy(true);
    GpgJob *job = new GpgJob(this);
    connect(job, &GpgJob::finished, this, [this, job]() {
        job->deleteLater();
        // IMPORT_RES: count, no user id, imported, …, secret read, secret imported, …
        const QStringList result = job->statusArguments(QStringLiteral("IMPORT_RES"));
        const int considered = result.value(0).toInt();
        // Fingerprints of the secret keys among them.
        QStringList own;
        for (const QString &line : job->status()) {
            if (!line.startsWith(QLatin1String("IMPORT_OK ")))
                continue;
            const QStringList arguments = line.split(QLatin1Char(' '));
            // Reason flags: 16 is a secret key.
            if (arguments.value(1).toInt() & 16)
                own.append(arguments.value(2));
        }
        if (considered == 0) {
            qWarning() << "gpg --import:" << job->status() << job->errors();
            setBusy(false);
            emit imported(0, tr("There's no key in it"));
            return;
        }
        if (!own.isEmpty())
            trustOwnKeys(own);
        refresh();
        emit imported(considered, QString());
    });
    job->start(QStringList() << QStringLiteral("--import") << path);
}

void PgpKeys::trustOwnKeys(const QStringList &fingerprints)
{
    QByteArray trust;
    for (const QString &fingerprint : fingerprints)
        trust += fingerprint.toLatin1() + ":6:\n";
    GpgJob *job = new GpgJob(this);
    connect(job, &GpgJob::finished, job, &QObject::deleteLater);
    job->start(QStringList() << QStringLiteral("--import-ownertrust"), trust);
}

void PgpKeys::remove(const QString &fingerprint)
{
    setBusy(true);
    GpgJob *job = new GpgJob(this);
    connect(job, &GpgJob::finished, this, [this, job]() {
        job->deleteLater();
        if (job->exitCode() != 0) {
            qWarning() << "gpg --delete-secret-and-public-key:" << job->status() << job->errors();
            setBusy(false);
            emit removed(tr("The key couldn't be removed"));
            return;
        }
        refresh();
        emit removed(QString());
    });
    job->start(QStringList() << QStringLiteral("--yes") << QStringLiteral("--delete-secret-and-public-key")
                             << fingerprint);
}
