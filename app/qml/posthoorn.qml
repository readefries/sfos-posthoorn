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

    // The PGP keyring, shared by the Keys page and importing keys attached to mail.
    readonly property PgpKeys pgpKeys: keyring

    PgpKeys {
        id: keyring
        onImported: Notices.show(error.length > 0 ? error : qsTr("Imported %n key(s)", "", count),
                                 error.length > 0 ? Notice.Long : Notice.Short, Notice.Center)
        onRemoved: {
            if (error.length > 0)
                Notices.show(error, Notice.Long, Notice.Center)
        }
    }

    // Folder ids of an account's virtual folders, used like a real folder's id in the favourites
    // and the saved state. Starred: its starred mail, wherever it is. Snoozed: what's hidden until
    // later, see Snoozes.
    readonly property int starredFolderId: -1
    readonly property int snoozedFolderId: -2

    function isVirtualFolder(folderId) {
        return folderId === starredFolderId || folderId === snoozedFolderId
    }

    function virtualFolderName(folderId) {
        return folderId === snoozedFolderId ? qsTr("Snoozed") : qsTr("Starred")
    }

    // What a UnifiedMessageListModel searches for to show a virtual folder.
    function virtualFolderSearch(folderId, accountId) {
        if (folderId === snoozedFolderId)
            return { "ids": Snoozes.messageIds, "accountId": accountId }
        return { "starred": true, "accountId": accountId }
    }

    function openVirtualFolder(accountId, folderId, operation) {
        pageStack.push(Qt.resolvedUrl("pages/MessagesPage.qml"), {
                           accountId: accountId,
                           folderId: folderId
                       }, operation)
    }

    function markVirtualFolderRead(accountId, folderId) {
        var model = folderMessages.createObject(app, { search: virtualFolderSearch(folderId, accountId) })
        var ids = model.unreadMessageIds()
        for (var i = 0; i < ids.length; ++i)
            agent.markMessageAsRead(ids[i])
        model.destroy()
    }

    // Hides the messages until the chosen time, read meanwhile. pickSnooze() lets the user choose
    // it, from the page origin, which it comes back to; done is called once they're snoozed.
    function pickSnooze(origin, messageIds, done) {
        var picker = pageStack.push(Qt.resolvedUrl("pages/SnoozePage.qml"), { count: messageIds.length })
        picker.chosen.connect(function(until) {
            for (var i = 0; i < messageIds.length; ++i) {
                agent.markMessageAsRead(messageIds[i])
                Snoozes.snooze(messageIds[i], until)
            }
            if (done)
                done()
            app.whenIdle(function() { pageStack.pop(origin) })
        })
    }

    // Brings back what was snoozed until now, unread.
    function wakeSnoozed() {
        var ids = Snoozes.takeDue()
        for (var i = 0; i < ids.length; ++i)
            agent.markMessageAsUnread(ids[i])
    }

    Connections {
        target: Snoozes
        onDue: app.wakeSnoozed()
    }

    Component.onCompleted: wakeSnoozed()

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

    Component {
        id: outgoingComponent
        OutgoingMessage { }
    }

    // A message to fill and give to sendLater(), owned by the app rather than the composer.
    function createOutgoing() {
        return outgoingComponent.createObject(app)
    }

    // Sending can be undone for a few seconds. The mail is a draft meanwhile, so it isn't lost when
    // the app goes; this timer rather than the remorse popup sends it, as the popup goes with its page.
    property QtObject pendingSend: null

    Timer {
        id: sendTimer
        // A little after the popup's own countdown.
        interval: 5500
        onTriggered: app.sendNow()
    }

    function sendLater(message, page) {
        if (pendingSend)
            sendNow()
        if (message.saveDraft() === 0) {
            Notices.show(qsTr("The mail couldn't be stored: %1").arg(message.error), Notice.Long, Notice.Center)
            message.destroy()
            return
        }
        pendingSend = message
        sendTimer.restart()
        if (page) {
            var popup = Remorse.popupAction(page, qsTr("Sending"), function() { app.sendNow() })
            if (popup)
                popup.canceled.connect(app.undoSend)
        }
    }

    function sendNow() {
        sendTimer.stop()
        var message = pendingSend
        if (!message)
            return
        pendingSend = null
        var accountId = message.send()
        if (accountId > 0) {
            agent.processSendingQueue(accountId)
            // So the server's copy of a draft goes too.
            agent.exportUpdates(accountId)
        } else {
            Notices.show(qsTr("The mail couldn't be put in the Outbox: %1").arg(message.error),
                         Notice.Long, Notice.Center)
        }
        message.destroy()
    }

    // Back to the composer, with the mail as it was.
    function undoSend() {
        sendTimer.stop()
        var message = pendingSend
        if (!message)
            return
        pendingSend = null
        var draftId = message.draftId()
        message.destroy()
        whenIdle(function() {
            pageStack.push(Qt.resolvedUrl("pages/ComposePage.qml"), { draftId: draftId })
        })
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
