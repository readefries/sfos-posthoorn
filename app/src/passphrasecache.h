// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PASSPHRASECACHE_H
#define PASSPHRASECACHE_H

#include <QDateTime>
#include <QObject>
#include <QSettings>

// How long the passphrase of one's key is remembered after it worked, and where: in this
// process, or by gpg-agent, which keeps it in memory that isn't swapped out and never hands it
// back to Posthoorn, also after Posthoorn closes (posthoorn-gpg-agent). Optionally forgotten as
// soon as the phone locks. As the PassphraseCache singleton; GpgJob asks it for the passphrase.
//
// Settings in state.ini (AppState::fileName()), under "pgp/".
class PassphraseCache : public QObject
{
    Q_OBJECT
    // Minutes after its last use: 0 never remembered, -1 until Posthoorn closes. 10 by default.
    Q_PROPERTY(int minutes READ minutes WRITE setMinutes NOTIFY minutesChanged)
    // Forgotten when the phone locks (with a lock code); off unless switched on.
    Q_PROPERTY(bool forgetOnLock READ forgetOnLock WRITE setForgetOnLock NOTIFY forgetOnLockChanged)
    // Kept by gpg-agent rather than by Posthoorn, when posthoorn-gpg-agent is there (outlivesApp);
    // off unless switched on.
    Q_PROPERTY(bool useAgent READ useAgent WRITE setUseAgent NOTIFY useAgentChanged)
    // Whether gpg-agent can keep it, also after Posthoorn closes: with posthoorn-gpg-agent,
    // outside the sandbox (GpgJob::setUpSystemAgent()).
    Q_PROPERTY(bool outlivesApp READ outlivesApp CONSTANT)

public:
    // The one instance, made by main().
    static PassphraseCache *instance();
    explicit PassphraseCache(QObject *parent = nullptr);
    ~PassphraseCache() override;

    int minutes() const { return m_minutes; }
    void setMinutes(int minutes);
    bool forgetOnLock() const { return m_forgetOnLock; }
    void setForgetOnLock(bool forget);
    bool useAgent() const { return m_useAgent; }
    void setUseAgent(bool use);
    bool outlivesApp() const;

    // The passphrase that last worked: empty when there's none, it expired, or gpg-agent keeps
    // it (gpg then runs without one and the agent fills it in).
    QString passphrase();
    void remember(const QString &passphrase);
    // Whether gpg-agent keeps it: when chosen and posthoorn-gpg-agent is there. An agent in the
    // sandbox is never kept running, as it would stop the app from starting again (GpgJob), so
    // without posthoorn-gpg-agent Posthoorn remembers it itself.
    bool agentKeeps() const;

    // Forgets the passphrase, wherever it's kept.
    Q_INVOKABLE void forget();
    // When the app closes: forgotten, unless gpg-agent outside the sandbox keeps it.
    void close();

    // posthoorn --forget-on-lock, as the posthoorn-forget-on-lock service, outside the sandbox:
    // stops posthoorn-gpg-agent when the phone locks, if the settings say so. Runs the event loop
    // of the QCoreApplication made for it.
    static int watchDeviceLock();


signals:
    void minutesChanged();
    void forgetOnLockChanged();
    void useAgentChanged();

private slots:
    void onDeviceLocked();

private:
    // After a change in settings: forgotten, as it was remembered under the old ones.
    void restart();
    // gpg-agent's settings, in GpgJob::homeDirectory(): whether it keeps the passphrase, and how
    // long.
    void writeAgentConfig(bool agentKeeps) const;
    void clearMemory();

    QSettings m_settings;
    int m_minutes = 10;
    bool m_forgetOnLock = false;
    bool m_useAgent = false;
    QString m_passphrase;
    QDateTime m_expiry;
};

#endif // PASSPHRASECACHE_H
