// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MUTEDACCOUNTSPLUGIN_H
#define MUTEDACCOUNTSPLUGIN_H

#include <qmailid.h>
#include <qmailmessageserverplugin.h>

#include <QObject>
#include <QtPlugin>

// New-mail notifications come from the messageserver, not from the mail app: its notifications
// plugin (qmf-notifications-plugin) announces every unread mail that arrives in a folder that is
// synced, unless the mail has QMF's NoNotification status. QMF itself sets that status on mail it
// copies or moves, and on old mail it finds.
//
// This plugin runs in the messageserver too and sets NoNotification on the mail that arrives in the
// accounts switched off in Posthoorn's Settings (state.ini, notifications/mutedAccounts). The
// messageserver starts its plugins in the order of their keys, so this one ("mutedaccounts")
// sees new mail before the notifications plugin ("notifications") does. Even the other way
// round, the notifications plugin takes back what it was about to announce once the mail's status
// changes, as it does for mail that is read meanwhile.
//
// Nothing else changes: the mail syncs, is unread and counts as unread as before.
class MutedAccountsService : public QMailMessageServerService
{
    Q_OBJECT

public:
    MutedAccountsService();

    // The accounts switched off, from Posthoorn's state.ini. Read again every time: the app
    // changes it while the messageserver runs.
    static QMailAccountIdList mutedAccounts();

private slots:
    void messagesAdded(const QMailMessageIdList &ids);
};

class MutedAccountsPlugin : public QMailMessageServerPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "nl.readefries.posthoorn.MutedAccountsPlugin")

public:
    explicit MutedAccountsPlugin(QObject *parent = nullptr);

    QString key() const override;
    QMailMessageServerService *createService() override;
};

#endif // MUTEDACCOUNTSPLUGIN_H
