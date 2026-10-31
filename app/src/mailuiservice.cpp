// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mailuiservice.h"

#include <QDBusConnection>
#include <QDebug>
#include <QUrl>
#include <QUrlQuery>

namespace {

const QString serviceName = QStringLiteral("com.jolla.email.ui");
const QString objectPath = QStringLiteral("/com/jolla/email/ui");

} // namespace

MailUiService::MailUiService(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(objectPath, this, QDBusConnection::ExportScriptableSlots
                            | QDBusConnection::ExportScriptableSignals))
        return;
    m_registered = bus.registerService(serviceName);
    if (m_registered) {
        qDebug() << "Answering mail notifications and links as" << serviceName;
    } else {
        // The usual case: without posthoorn-default-mail the sandbox doesn't allow the name.
        bus.unregisterObject(objectPath);
    }
}

void MailUiService::inboxShown(int accountId)
{
    if (!m_registered)
        return;
    if (accountId > 0)
        emit accountInboxDisplayed(accountId);
    else
        emit combinedInboxDisplayed();
}

void MailUiService::compose(const QString &emailSubject, const QString &emailTo, const QString &emailCc,
                            const QString &emailBcc, const QString &emailBody)
{
    emit composeRequested(emailSubject, emailTo, emailCc, emailBcc, emailBody);
}

void MailUiService::openCombinedInbox()
{
    emit inboxRequested(0);
}

void MailUiService::openInbox(int accountId)
{
    emit inboxRequested(accountId);
}

void MailUiService::mailto(const QStringList &content)
{
    // "mailto:a@example.org,b@example.org?cc=…&subject=…&body=…"; only the first link counts.
    if (content.isEmpty())
        return;
    const QUrl url(content.first());
    const QUrlQuery query(url);
    QStringList to;
    const QString path = url.path(QUrl::FullyDecoded);
    if (!path.isEmpty())
        to.append(path);
    const QString more = query.queryItemValue(QStringLiteral("to"), QUrl::FullyDecoded);
    if (!more.isEmpty())
        to.append(more);
    emit composeRequested(query.queryItemValue(QStringLiteral("subject"), QUrl::FullyDecoded),
                          to.join(QStringLiteral(", ")),
                          query.queryItemValue(QStringLiteral("cc"), QUrl::FullyDecoded),
                          query.queryItemValue(QStringLiteral("bcc"), QUrl::FullyDecoded),
                          query.queryItemValue(QStringLiteral("body"), QUrl::FullyDecoded));
}

void MailUiService::openMessage(int messageId)
{
    emit messageRequested(messageId, 0);
}

void MailUiService::replyToMessage(int messageId)
{
    emit messageRequested(messageId, 1);
}

void MailUiService::replyAllToMessage(int messageId)
{
    emit messageRequested(messageId, 2);
}

void MailUiService::activateWindow(const QStringList &)
{
    emit activateRequested();
}
