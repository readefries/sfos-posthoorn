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
// gpg starts gpg-agent as a daemon, which would outlive the app in its sandbox and keep the
// sandbox, and so the next start of the app, from ending. So the agent is stopped again once no
// job runs any more, and the passphrase is kept here instead (rememberPassphrase()).
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

    // The passphrase that last worked, for a while: empty when there's none or it expired.
    static QString passphrase();
    static void rememberPassphrase(const QString &passphrase);

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
