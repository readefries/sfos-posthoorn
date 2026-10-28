// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1
import "pages"

ApplicationWindow {
    id: app

    // The plugin's C++ models use EmailAgent::instance(), which this one becomes. Keep it the
    // first child so it exists before any model asks for it.
    readonly property EmailAgent emailAgent: agent
    readonly property EmailAccountListModel accounts: accountModel

    EmailAgent {
        id: agent
        onError: Notices.show(qsTr("Sync failed for an account (error %1)").arg(syncError),
                              Notice.Long, Notice.Center)
        onSendCompleted: Notices.show(success ? qsTr("Mail sent")
                                              : qsTr("Sending failed, the mail is in the Outbox"),
                                      success ? Notice.Short : Notice.Long, Notice.Center)
    }

    EmailAccountListModel {
        id: accountModel
        onNumberOfAccountsChanged: console.log("accounts:", numberOfAccounts)
        Component.onCompleted: console.log("accounts at start:", numberOfAccounts)
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
