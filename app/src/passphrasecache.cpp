// SPDX-License-Identifier: AGPL-3.0-or-later
#include "passphrasecache.h"

#include "appstate.h"
#include "devicelock.h"
#include "gpgjob.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QSaveFile>

namespace {

PassphraseCache *theInstance = nullptr;

// For gpg-agent, "until Posthoorn closes" and "no hard limit": a year, in seconds. Posthoorn
// stops the agent when it closes (close()).
const int agentForever = 365 * 24 * 60 * 60;

} // namespace

PassphraseCache *PassphraseCache::instance()
{
    return theInstance;
}

PassphraseCache::PassphraseCache(QObject *parent)
    : QObject(parent)
    , m_settings(AppState::fileName(), QSettings::IniFormat)
{
    theInstance = this;
    m_minutes = qMax(-1, m_settings.value(QStringLiteral("pgp/passphraseMinutes"), 10).toInt());
    m_forgetOnLock = m_settings.value(QStringLiteral("pgp/forgetPassphraseOnLock"), false).toBool();
    m_useAgent = m_settings.value(QStringLiteral("pgp/passphraseInAgent"), false).toBool();

    if (GpgJob::available()) {
        // Before the agent may start, as it reads it then.
        writeAgentConfig(m_useAgent);
        // Else an agent of the app's own, which one left by a Posthoorn that didn't close properly
        // would keep what it remembered; and with nothing to remember for it.
        if (!GpgJob::setUpSystemAgent()) {
            GpgJob::stopAgent();
            writeAgentConfig(false);
        }
    }

    connect(new DeviceLock(this), &DeviceLock::locked, this, &PassphraseCache::onDeviceLocked);
}

PassphraseCache::~PassphraseCache()
{
    clearMemory();
    if (theInstance == this)
        theInstance = nullptr;
}

bool PassphraseCache::outlivesApp() const
{
    return GpgJob::systemAgent();
}

bool PassphraseCache::agentKeeps() const
{
    return m_useAgent && GpgJob::systemAgent();
}

void PassphraseCache::close()
{
    clearMemory();
    if (agentKeeps() && m_minutes != -1) {
        qDebug() << "gpg-agent keeps the passphrase after Posthoorn closes";
        return;
    }
    forget();
}

int PassphraseCache::watchDeviceLock()
{
    DeviceLock lock;
    QObject::connect(&lock, &DeviceLock::locked, [] {
        // Read again each time: they're changed in the app.
        QSettings settings(AppState::fileName(), QSettings::IniFormat);
        if (!settings.value(QStringLiteral("pgp/forgetPassphraseOnLock"), false).toBool()
                || !settings.value(QStringLiteral("pgp/passphraseInAgent"), false).toBool())
            return;
        qInfo() << "The phone locked, stopping Posthoorn's gpg-agent";
        // Without waiting: this service stops with it.
        QProcess::execute(QStringLiteral("systemctl"),
                          QStringList() << QStringLiteral("--user") << QStringLiteral("--no-block")
                                        << QStringLiteral("stop") << QStringLiteral("posthoorn-gpg-agent.service"));
    });
    return QCoreApplication::exec();
}

void PassphraseCache::setMinutes(int minutes)
{
    minutes = qMax(-1, minutes);
    if (minutes == m_minutes)
        return;
    m_minutes = minutes;
    m_settings.setValue(QStringLiteral("pgp/passphraseMinutes"), minutes);
    m_settings.sync();
    restart();
    emit minutesChanged();
}

void PassphraseCache::setForgetOnLock(bool forget)
{
    if (forget == m_forgetOnLock)
        return;
    m_forgetOnLock = forget;
    m_settings.setValue(QStringLiteral("pgp/forgetPassphraseOnLock"), forget);
    m_settings.sync();
    emit forgetOnLockChanged();
}

void PassphraseCache::setUseAgent(bool use)
{
    if (use == m_useAgent)
        return;
    m_useAgent = use;
    m_settings.setValue(QStringLiteral("pgp/passphraseInAgent"), use);
    m_settings.sync();
    restart();
    emit useAgentChanged();
}

QString PassphraseCache::passphrase()
{
    if (m_expiry.isValid() && QDateTime::currentDateTimeUtc() > m_expiry)
        clearMemory();
    return m_passphrase;
}

void PassphraseCache::remember(const QString &passphrase)
{
    qDebug() << "Passphrase remembered for" << m_minutes << "minutes" << (agentKeeps() ? "by gpg-agent" : "in memory");
    // gpg-agent remembered it itself, for as long as writeAgentConfig() said.
    if (agentKeeps() || m_minutes == 0)
        return;
    clearMemory();
    m_passphrase = passphrase;
    m_expiry = m_minutes > 0 ? QDateTime::currentDateTimeUtc().addSecs(m_minutes * 60) : QDateTime();
}

void PassphraseCache::forget()
{
    qDebug() << "Forgetting the passphrase" << (agentKeeps() ? "in gpg-agent" : "in memory");
    clearMemory();
    // Stopped when idle, else only emptied, as a gpg job may still need it.
    if (GpgJob::runningJobs() == 0)
        GpgJob::stopAgent();
    else
        GpgJob::reloadAgent();
}

void PassphraseCache::writeAgentConfig(bool agentKeeps) const
{
    // Otherwise the agent remembers nothing: Posthoorn does.
    int ttl = 0;
    if (agentKeeps)
        ttl = m_minutes < 0 ? agentForever : m_minutes * 60;
    QSaveFile file(GpgJob::homeDirectory() + QStringLiteral("/gpg-agent.conf"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Can't write gpg-agent.conf:" << file.errorString();
        return;
    }
    // Remembered for ttl after its last use, without a hard limit, as Posthoorn does itself.
    file.write("# Written by Posthoorn from its settings: changes here are overwritten.\n");
    file.write("default-cache-ttl " + QByteArray::number(ttl) + '\n');
    file.write("max-cache-ttl " + QByteArray::number(ttl > 0 ? agentForever : 0) + '\n');
    if (!file.commit())
        qWarning() << "Can't write gpg-agent.conf:" << file.errorString();
}

void PassphraseCache::onDeviceLocked()
{
    if (!m_forgetOnLock)
        return;
    qDebug() << "The phone locked, forgetting the passphrase";
    forget();
}

void PassphraseCache::restart()
{
    writeAgentConfig(agentKeeps());
    // A running agent reads its settings again; reloading also empties its cache.
    forget();
}

void PassphraseCache::clearMemory()
{
    // Overwritten in place first, as far as this copy goes.
    m_passphrase.fill(QChar(0));
    m_passphrase.clear();
    m_expiry = QDateTime();
}
