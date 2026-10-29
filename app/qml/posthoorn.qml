// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1
import Posthoorn 1.0
import "pages"

ApplicationWindow {
    id: app

    // The plugin's C++ models use EmailAgent::instance(), which this one becomes. Keep it the
    // first child so it exists before any model asks for it.
    readonly property EmailAgent emailAgent: agent
    readonly property EmailAccountListModel accounts: accountModel

    // Bumped when accounts or their standard folders change: inboxFolderId() doesn't notify.
    property int accountsVersion

    // The folders the unified inbox shows, as { accountId, folderId }: those chosen, or else the
    // inbox of every account.
    readonly property var unifiedFolders: {
        accountsVersion
        var folders = []
        if (AppState.unifiedConfigured) {
            var entries = AppState.unifiedFolders
            for (var i = 0; i < entries.length; ++i) {
                var parts = entries[i].split("/")
                folders.push({ accountId: parseInt(parts[0]), folderId: parseInt(parts[1]) })
            }
        } else {
            for (var j = 0; j < accountModel.numberOfAccounts; ++j) {
                var accountId = accountModel.accountId(j)
                var inbox = agent.inboxFolderId(accountId)
                if (inbox > 0)
                    folders.push({ accountId: accountId, folderId: inbox })
            }
        }
        return folders
    }
    readonly property var unifiedFolderIds: unifiedFolders.map(function(folder) { return folder.folderId })

    EmailAgent {
        id: agent
        onError: Notices.show(qsTr("Sync failed for an account (error %1)").arg(syncError),
                              Notice.Long, Notice.Center)
        onSendCompleted: Notices.show(success ? qsTr("Mail sent")
                                              : qsTr("Sending failed, the mail is in the Outbox"),
                                      success ? Notice.Short : Notice.Long, Notice.Center)
        onStandardFoldersCreated: app.accountsVersion++
    }

    EmailAccountListModel {
        id: accountModel
        onNumberOfAccountsChanged: app.accountsVersion++
        onAccountsUpdated: app.accountsVersion++
    }

    // A new mail from accountId, or from the first account for 0.
    function compose(accountId) {
        var from = accountId > 0 ? accountModel.emailAddressFromAccountId(accountId)
                                 : accountModel.numberOfAccounts > 0 ? accountModel.emailAddress(0) : ""
        pageStack.push(Qt.resolvedUrl("pages/ComposePage.qml"), { from: from })
    }

    initialPage: Component { AccountsPage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
}
