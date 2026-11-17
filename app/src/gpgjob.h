// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef GPGJOB_H
#define GPGJOB_H

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QStringList>

// One run of Posthoorn's own gpg (posthoorn-gnupg), in the background, with its own keyring in
// the app's data folder. What gpg reports for programs (--status-fd) comes back as status lines,
// without their "[GNUPG:] " prefix.
//
// A passphrase goes in on stdin (--passphrase-fd 0), so any other input must be a file.
//
// The agent: preferably posthoorn-gpg-agent, a systemd user service outside the sandbox
// (app/systemd/), which can outlive the app. Without it, gpg starts gpg-agent as a daemon in the
// sandbox, which would outlive the app there and keep the sandbox, and so the next start of the
// app, from ending (and the launcher refuses a second instance). So that one is stopped again
// once no job runs any more, and Posthoorn keeps the passphrase itself (PassphraseCache).
class GpgJob : public QObject
{
    Q_OBJECT

public:
    explicit GpgJob(QObject *parent = nullptr);

    // Where gpg is, and whether it's there (posthoorn-gnupg installed).
    static QString program();
    static bool available();
    // The keyring's folder, created when missing.
    static QString homeDirectory();

    // The passphrase that last worked, for a while (PassphraseCache): empty when there's none,
    // it expired or gpg-agent keeps it.
    static QString passphrase();
    static void rememberPassphrase(const QString &passphrase);

    // How many gpg jobs run now.
    static int runningJobs();
    // Points gpg to posthoorn-gpg-agent when its socket is there and it answers; once, before
    // any job. Whether it does.
    static bool setUpSystemAgent();
    static bool systemAgent();
    // Ends gpg-agent, and with it what it remembered. posthoorn-gpg-agent starts again when used.
    static void stopAgent();
    // Has a running gpg-agent read its settings again, which also empties its cache.
    static void reloadAgent();

    // Signed data in its canonical form, with CRLF line ends (RFC 3156).
    static QByteArray canonical(const QByteArray &data);

    // Runs gpg with arguments, after the ones every run gets. input goes to its stdin.
    void start(const QStringList &arguments, const QByteArray &input = QByteArray());

    QByteArray output() const { return m_output; }
    QStringList status() const { return m_status; }
    int exitCode() const { return m_exitCode; }
    // Whether a status line starts with keyword.
    bool hasStatus(const QString &keyword) const;
    // The arguments of the first status line starting with keyword.
    QStringList statusArguments(const QString &keyword) const;
    // gpg's own messages, for the log.
    QString errors() const { return QString::fromUtf8(m_errors); }

signals:
    void finished();

private:
    void onFinished(int exitCode, QProcess::ExitStatus status);

    QProcess m_process;
    QByteArray m_output;
    QByteArray m_errors;
    QStringList m_status;
    int m_exitCode = -1;
};

#endif // GPGJOB_H
