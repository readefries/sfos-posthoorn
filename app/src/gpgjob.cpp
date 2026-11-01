// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gpgjob.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

// posthoorn-gnupg's prefix, see gnupg/rpm/posthoorn-gnupg.spec.
const QString gnupgBin = QStringLiteral("/usr/libexec/posthoorn/gnupg/bin");

// How long a passphrase is kept, as gpg-agent would.
const int passphraseMinutes = 10;

int runningJobs = 0;
QString rememberedPassphrase;
QDateTime passphraseExpiry;

// Ends gpg-agent, which gpg started as a daemon.
void stopAgent()
{
    QProcess::startDetached(gnupgBin + QStringLiteral("/gpgconf"),
                            QStringList() << QStringLiteral("--homedir") << GpgJob::homeDirectory()
                                          << QStringLiteral("--kill") << QStringLiteral("gpg-agent"));
}

} // namespace

GpgJob::GpgJob(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &GpgJob::onFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qWarning() << "gpg didn't start:" << m_process.errorString();
            --runningJobs;
            m_exitCode = -1;
            emit finished();
        }
    });
}

QString GpgJob::program()
{
    return gnupgBin + QStringLiteral("/gpg");
}

bool GpgJob::available()
{
    return QFileInfo(program()).isExecutable();
}

QString GpgJob::homeDirectory()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/gnupg");
    if (!QDir(directory).exists()) {
        QDir().mkpath(directory);
        // gpg refuses a home folder others can read.
        QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
    return directory;
}

QString GpgJob::passphrase()
{
    if (passphraseExpiry.isValid() && QDateTime::currentDateTimeUtc() > passphraseExpiry) {
        rememberedPassphrase.clear();
        passphraseExpiry = QDateTime();
    }
    return rememberedPassphrase;
}

void GpgJob::rememberPassphrase(const QString &passphrase)
{
    rememberedPassphrase = passphrase;
    passphraseExpiry = QDateTime::currentDateTimeUtc().addSecs(passphraseMinutes * 60);
}

QByteArray GpgJob::canonical(const QByteArray &data)
{
    QByteArray result;
    result.reserve(data.size() + data.count('\n'));
    for (int i = 0; i < data.size(); ++i) {
        const char c = data.at(i);
        if (c == '\n' && (i == 0 || data.at(i - 1) != '\r'))
            result += '\r';
        result += c;
    }
    return result;
}

void GpgJob::start(const QStringList &arguments, const QByteArray &input)
{
    ++runningJobs;
    m_output.clear();
    m_errors.clear();
    m_status.clear();
    m_exitCode = -1;

    // Status lines on stderr, mixed with gpg's messages; split apart when done. No terminal,
    // and passphrases from us rather than a pinentry.
    QStringList all;
    all << QStringLiteral("--homedir") << homeDirectory()
        << QStringLiteral("--batch") << QStringLiteral("--no-tty")
        << QStringLiteral("--status-fd") << QStringLiteral("2")
        << QStringLiteral("--pinentry-mode") << QStringLiteral("loopback")
        << QStringLiteral("--agent-program") << gnupgBin + QStringLiteral("/gpg-agent")
        << arguments;
    m_process.start(program(), all);
    if (!input.isEmpty())
        m_process.write(input);
    m_process.closeWriteChannel();
}

bool GpgJob::hasStatus(const QString &keyword) const
{
    const QString prefix = keyword + QLatin1Char(' ');
    for (const QString &line : m_status) {
        if (line == keyword || line.startsWith(prefix))
            return true;
    }
    return false;
}

QStringList GpgJob::statusArguments(const QString &keyword) const
{
    const QString prefix = keyword + QLatin1Char(' ');
    for (const QString &line : m_status) {
        if (line.startsWith(prefix))
            return line.mid(prefix.size()).split(QLatin1Char(' '));
        if (line == keyword)
            return QStringList();
    }
    return QStringList();
}

void GpgJob::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (--runningJobs == 0)
        stopAgent();
    m_exitCode = status == QProcess::NormalExit ? exitCode : -1;
    m_output = m_process.readAllStandardOutput();
    static const QByteArray marker("[GNUPG:] ");
    for (const QByteArray &line : m_process.readAllStandardError().split('\n')) {
        if (line.startsWith(marker))
            m_status.append(QString::fromUtf8(line.mid(marker.size())));
        else if (!line.isEmpty())
            m_errors += line + '\n';
    }
    emit finished();
}
