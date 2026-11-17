// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gpgjob.h"

#include "passphrasecache.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <sys/stat.h>

namespace {

// posthoorn-gnupg's prefix, see gnupg/rpm/posthoorn-gnupg.spec.
const QString gnupgBin = QStringLiteral("/usr/libexec/posthoorn/gnupg/bin");

int jobs = 0;
bool outsideSandbox = false;

bool isSocket(const QString &path)
{
    struct stat info;
    return ::stat(QFile::encodeName(path).constData(), &info) == 0 && S_ISSOCK(info.st_mode);
}

// Runs one of GnuPG's tools and waits for it: its exit code, and what it wrote, trimmed.
int runTool(const QString &tool, const QStringList &arguments, QString *output = nullptr)
{
    QProcess process;
    process.start(gnupgBin + QLatin1Char('/') + tool,
                  QStringList() << QStringLiteral("--homedir") << GpgJob::homeDirectory() << arguments);
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        return -1;
    }
    if (output)
        *output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

void runGpgconf(const QString &action)
{
    QProcess::startDetached(gnupgBin + QStringLiteral("/gpgconf"),
                            QStringList() << QStringLiteral("--homedir") << GpgJob::homeDirectory()
                                          << action << QStringLiteral("gpg-agent"));
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
            --jobs;
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
    PassphraseCache *cache = PassphraseCache::instance();
    return cache ? cache->passphrase() : QString();
}

void GpgJob::rememberPassphrase(const QString &passphrase)
{
    if (PassphraseCache *cache = PassphraseCache::instance())
        cache->remember(passphrase);
}

int GpgJob::runningJobs()
{
    return jobs;
}

bool GpgJob::setUpSystemAgent()
{
    outsideSandbox = false;
    const QString socket = homeDirectory() + QStringLiteral("/S.gpg-agent");
    if (!isSocket(socket))
        return false;

    // gpg looks for the agent in its socket folder, which is the sandbox's own (/run/user/<uid>/
    // gnupg/d.<hash>): there, a redirect to the socket in the keyring's folder, which the sandbox
    // shares.
    runTool(QStringLiteral("gpgconf"), QStringList() << QStringLiteral("--create-socketdir"));
    QString expected;
    runTool(QStringLiteral("gpgconf"), QStringList() << QStringLiteral("--list-dirs") << QStringLiteral("agent-socket"),
            &expected);
    QString redirect;
    if (!expected.isEmpty() && expected != socket) {
        // An agent of the app's own, left from before, would be found first.
        if (isSocket(expected))
            runTool(QStringLiteral("gpgconf"), QStringList() << QStringLiteral("--kill") << QStringLiteral("gpg-agent"));
        QFile::remove(expected);
        QSaveFile file(expected);
        if (!file.open(QIODevice::WriteOnly)
                || file.write("%Assuan%\nsocket=" + QFile::encodeName(socket) + '\n') < 0
                || !file.commit()) {
            qWarning() << "Can't point gpg to the agent outside the sandbox:" << file.errorString();
            return false;
        }
        redirect = expected;
    }
    // Starts it, through systemd, when it isn't running yet.
    if (runTool(QStringLiteral("gpg-connect-agent"), QStringList() << QStringLiteral("--no-autostart")
                << QStringLiteral("/bye")) != 0) {
        qWarning() << "The gpg-agent outside the sandbox doesn't answer; using one of the app's own";
        if (!redirect.isEmpty())
            QFile::remove(redirect);
        return false;
    }
    qDebug() << "Using the gpg-agent outside the sandbox";
    outsideSandbox = true;
    return true;
}

bool GpgJob::systemAgent()
{
    return outsideSandbox;
}

void GpgJob::stopAgent()
{
    qDebug() << "Stopping gpg-agent";
    runGpgconf(QStringLiteral("--kill"));
}

void GpgJob::reloadAgent()
{
    runGpgconf(QStringLiteral("--reload"));
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
    ++jobs;
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
    // The app's own, in the sandbox; posthoorn-gpg-agent may keep the passphrase.
    if (--jobs == 0 && !outsideSandbox)
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
