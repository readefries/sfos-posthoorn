// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MAILUISERVICE_H
#define MAILUISERVICE_H

#include <qdbusmacros.h>
#include <QObject>
#include <QStringList>

// The mail app's D-Bus service, com.jolla.email.ui, as the MailUi singleton: what the system's
// new-mail notifications and mailto: links call. Only when the posthoorn-default-mail package
// is installed, in place of Jolla's Mail app: its sailjail profile lets the app own the name.
// Otherwise registering fails and nothing changes.
//
// The calls are passed on to QML as signals; the signals of the interface tell the
// notifications which inbox is in view.
class MailUiService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.jolla.email.ui")
    Q_PROPERTY(bool registered READ registered CONSTANT)

public:
    explicit MailUiService(QObject *parent = nullptr);

    bool registered() const { return m_registered; }

    // An inbox is in view: an account's, or the combined one for 0.
    Q_INVOKABLE void inboxShown(int accountId);

public slots:
    Q_SCRIPTABLE void compose(const QString &emailSubject, const QString &emailTo, const QString &emailCc,
                              const QString &emailBcc, const QString &emailBody);
    Q_SCRIPTABLE void openCombinedInbox();
    Q_SCRIPTABLE void openInbox(int accountId);
    Q_SCRIPTABLE void mailto(const QStringList &content);
    Q_SCRIPTABLE void openMessage(int messageId);
    Q_SCRIPTABLE void replyToMessage(int messageId);
    Q_SCRIPTABLE void replyAllToMessage(int messageId);
    Q_SCRIPTABLE Q_NOREPLY void activateWindow(const QStringList &dummy);

signals:
    // For QML.
    void composeRequested(const QString &subject, const QString &to, const QString &cc, const QString &bcc,
                          const QString &body);
    void inboxRequested(int accountId);
    // reply: 0 to read it, 1 to reply, 2 to reply to all.
    void messageRequested(int messageId, int reply);
    void activateRequested();

    // On D-Bus.
    Q_SCRIPTABLE void displayEntered();
    Q_SCRIPTABLE void displayExit();
    Q_SCRIPTABLE void combinedInboxDisplayed();
    Q_SCRIPTABLE void accountInboxDisplayed(int accountId);

private:
    bool m_registered = false;
};

#endif // MAILUISERVICE_H
