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

    // Whether mail in folderId of accountId can be archived: the account has an archive folder,
    // and it isn't that one.
    function canArchive(accountId, folderId) {
        ArchiveFolders.revision
        var archive = ArchiveFolders.folderId(accountId)
        return archive > 0 && archive !== folderId
    }

    // Moves a mail to its account's archive folder. Each mail goes to its own account's, so this
    // works for mail of several accounts too.
    function archive(messageId) {
        var accountId = agent.accountIdForMessage(messageId)
        if (canArchive(accountId, agent.folderIdForMessage(messageId)))
            agent.moveMessage(messageId, ArchiveFolders.folderId(accountId))
    }

    function setStarred(messageId, starred) {
        agent.exportUpdates(MessageFlags.setStarred(messageId, starred))
    }

    Component {
        id: folderMessages
        UnifiedMessageListModel { }
    }

    // The folder id of an account's virtual Starred folder: its starred mail, wherever it is. Used
    // like a real folder's id in the favourites and the saved state.
    readonly property int starredFolderId: -1

    // An account's starred mail.
    function openStarred(accountId, operation) {
        pageStack.push(Qt.resolvedUrl("pages/MessagesPage.qml"), {
                           accountId: accountId,
                           folderId: starredFolderId
                       }, operation)
    }

    function markStarredRead(accountId) {
        var model = folderMessages.createObject(app, { search: { starred: true, accountId: accountId } })
        var ids = model.unreadMessageIds()
        for (var i = 0; i < ids.length; ++i)
            agent.markMessageAsRead(ids[i])
        model.destroy()
    }

    // Marks all mail in the folders as read, not just what a list has loaded.
    function markFoldersRead(folderIds) {
        var model = folderMessages.createObject(app, { folderIds: folderIds })
        var ids = model.unreadMessageIds()
        for (var i = 0; i < ids.length; ++i)
            agent.markMessageAsRead(ids[i])
        model.destroy()
    }

    // Runs fn now, or right after a page transition: pushing or popping during one fails ("Cannot
    // pop while operation is in progress").
    function whenIdle(fn) {
        if (pageStack.busy)
            idleQueue.push(fn)
        else
            fn()
    }

    property var idleQueue: []

    Connections {
        target: pageStack
        onBusyChanged: {
            while (!pageStack.busy && app.idleQueue.length > 0)
                app.idleQueue.shift()()
        }
    }

    // Goes back from page to target, or to the page below it. Only while page is on top: it may
    // have been left already, e.g. during a remorse timer.
    function popFrom(page, target) {
        whenIdle(function() {
            if (pageStack.currentPage !== page)
                return
            if (target)
                pageStack.pop(target)
            else
                pageStack.pop()
        })
    }

    // What a notification or mail link asks for (MailUi), once the app has opened where it was
    // left; before that, the first page runs it instead of restoring.
    property bool started: false
    property var pendingRequest: null

    function request(fn) {
        if (started)
            whenIdle(fn)
        else
            pendingRequest = fn
    }

    Connections {
        target: MailUi
        onActivateRequested: app.activate()
        onInboxRequested: {
            app.activate()
            app.request(function() {
                if (accountId > 0) {
                    var inbox = agent.inboxFolderId(accountId)
                    if (inbox > 0)
                        pageStack.push(Qt.resolvedUrl("pages/MessagesPage.qml"), {
                                           accountId: accountId,
                                           folderId: inbox,
                                           accessor: agent.accessorFromFolderId(inbox)
                                       })
                } else {
                    pageStack.push(Qt.resolvedUrl("pages/MessagesPage.qml"), {
                                       unified: true,
                                       title: qsTr("Unified inbox")
                                   })
                }
            })
        }
        onMessageRequested: {
            app.activate()
            app.request(function() {
                pageStack.push(Qt.resolvedUrl("pages/MessagePage.qml"), {
                                   messageId: messageId,
                                   replyWhenLoaded: reply
                               })
            })
        }
        onComposeRequested: {
            app.activate()
            app.request(function() {
                pageStack.push(Qt.resolvedUrl("pages/ComposePage.qml"), {
                                   from: accountModel.numberOfAccounts > 0 ? accountModel.emailAddress(0) : "",
                                   subject: subject,
                                   to: to,
                                   cc: cc,
                                   bcc: bcc,
                                   bodyText: body
                               })
            })
        }
    }

    // Searching all mail of all accounts.
    function search() {
        pageStack.push(Qt.resolvedUrl("pages/MessagesPage.qml"), { searching: true })
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
