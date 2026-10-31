// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1
import Posthoorn 1.0

// Where the app starts: the unified inbox, the favourites and the accounts. With a single account
// its folders are here instead, without a page of their own.
Page {
    id: page

    property bool restored: false

    readonly property int singleAccountId: app.accounts.numberOfAccounts === 1 ? app.accounts.accountId(0) : 0
    readonly property bool single: singleAccountId > 0

    FolderListModel {
        id: singleFolders
        accountKey: page.singleAccountId
    }

    function openFolder(index, folderId, operation) {
        // The name comes from the folder itself, see MessagesPage.
        pageStack.push(Qt.resolvedUrl("MessagesPage.qml"), {
                           accountId: page.singleAccountId,
                           folderId: folderId,
                           accessor: singleFolders.folderAccessor(index)
                       }, operation)
    }

    function openUnified(operation) {
        pageStack.push(Qt.resolvedUrl("MessagesPage.qml"), {
                           unified: true,
                           title: qsTr("Unified inbox")
                       }, operation)
    }

    // Opens the folder or account the app was left in.
    function restore() {
        app.started = true
        // Started from a notification or a mail link: that instead.
        if (app.pendingRequest) {
            var request = app.pendingRequest
            app.pendingRequest = null
            request()
            return
        }
        var accountId = AppState.accountId
        // "messages" without an account was the plugin's combined inbox, before the unified one.
        if (AppState.page === "unified" || (AppState.page === "messages" && accountId === 0)) {
            if (app.accounts.numberOfAccounts > 0)
                openUnified(PageStackAction.Immediate)
        } else if (single && AppState.page === "messages" && accountId === singleAccountId) {
            var index = singleFolders.indexFromFolderId(AppState.folderId)
            if (index >= 0)
                openFolder(index, AppState.folderId, PageStackAction.Immediate)
        } else if (!single && (AppState.page === "folders" || AppState.page === "messages")
                   && app.accounts.indexFromAccountId(accountId) >= 0) {
            pageStack.push(Qt.resolvedUrl("FoldersPage.qml"), {
                               accountId: accountId,
                               title: app.accounts.displayNameFromAccountId(accountId),
                               restoreFolderId: AppState.page === "messages" ? AppState.folderId : 0
                           }, PageStackAction.Immediate)
        }
    }

    onStatusChanged: {
        if (status !== PageStatus.Active)
            return
        if (!restored) {
            restored = true
            restoreLater.start()
        } else {
            AppState.save("accounts")
        }
    }

    // Pushing while this page is still becoming active makes its status change again inside its
    // own change: a binding loop. So right after instead.
    Timer {
        id: restoreLater
        interval: 0
        onTriggered: page.restore()
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: page.single ? singleFolders : app.accounts
        delegate: page.single ? folderDelegate : accountDelegate

        header: Column {
            width: list.width

            PageHeader {
                // The header doesn't find its page from inside the list, and without it the
                // title doesn't keep clear of the camera cutout.
                page: list.parent
                title: page.single ? app.accounts.displayNameFromAccountId(page.singleAccountId) : qsTr("Accounts")
            }

            ListItem {
                id: unifiedInbox
                visible: app.accounts.numberOfAccounts > 0
                contentHeight: Theme.itemSizeMedium
                onClicked: page.openUnified(PageStackAction.Animated)
                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("Mark all as read")
                        onClicked: app.markFoldersRead(app.unifiedFolderIds)
                    }
                }
                Column {
                    anchors {
                        left: parent.left
                        right: parent.right
                        leftMargin: Theme.horizontalPageMargin
                        rightMargin: Theme.horizontalPageMargin
                        verticalCenter: parent.verticalCenter
                    }
                    Label {
                        width: parent.width
                        text: qsTr("Unified inbox")
                        color: unifiedInbox.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                    Label {
                        width: parent.width
                        text: AppState.unifiedConfigured
                              ? qsTr("%n folder(s)", "", app.unifiedFolders.length)
                              : qsTr("The inboxes of all accounts")
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryColor
                        truncationMode: TruncationMode.Fade
                    }
                }
            }

            SectionHeader {
                visible: favourites.count > 0
                text: qsTr("Favourites")
            }

            // Added from a folder's long-press menu.
            Repeater {
                id: favourites
                model: AppState.favouriteFolders

                ListItem {
                    id: favourite

                    readonly property int accountId: parseInt(modelData.split("/")[0])
                    readonly property int folderId: parseInt(modelData.split("/")[1])

                    // Not for a folder that's gone, or of an account that's gone.
                    visible: app.accounts.indexFromAccountId(accountId) >= 0 && folder.displayName.length > 0
                    width: parent.width
                    contentHeight: visible ? Theme.itemSizeMedium : 0

                    onClicked: pageStack.push(Qt.resolvedUrl("MessagesPage.qml"), {
                                                  accountId: accountId,
                                                  folderId: folderId,
                                                  accessor: folder.folderAccessor
                                              })

                    menu: ContextMenu {
                        MenuItem {
                            visible: folder.folderUnreadCount > 0
                            text: qsTr("Mark all as read")
                            onClicked: app.markFoldersRead([favourite.folderId])
                        }
                        MenuItem {
                            text: qsTr("Remove from favourites")
                            onClicked: AppState.setFavourite(favourite.accountId, favourite.folderId, false)
                        }
                    }

                    EmailFolder {
                        id: folder
                        folderAccessor: app.emailAgent.accessorFromFolderId(favourite.folderId)
                    }

                    Column {
                        anchors {
                            left: parent.left
                            right: favouriteUnread.left
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.paddingMedium
                            verticalCenter: parent.verticalCenter
                        }
                        Label {
                            width: parent.width
                            text: folder.displayName
                            color: favourite.highlighted ? Theme.highlightColor : Theme.primaryColor
                            truncationMode: TruncationMode.Fade
                        }
                        Label {
                            width: parent.width
                            text: app.accounts.displayNameFromAccountId(favourite.accountId)
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.secondaryColor
                            truncationMode: TruncationMode.Fade
                        }
                    }

                    Label {
                        id: favouriteUnread
                        anchors {
                            right: parent.right
                            rightMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        text: folder.folderUnreadCount > 0 ? folder.folderUnreadCount : ""
                        color: Theme.highlightColor
                    }
                }
            }

            SectionHeader {
                visible: page.single || favourites.count > 0
                text: page.single ? qsTr("Folders") : qsTr("Accounts")
            }
        }

        PullDownMenu {
            busy: app.emailAgent.synchronizing
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                text: qsTr("Search")
                onClicked: app.search()
            }
            MenuItem {
                visible: page.single
                text: qsTr("Refresh folder list")
                onClicked: app.emailAgent.retrieveFolderList(page.singleAccountId)
            }
            MenuItem {
                text: qsTr("Sync all folders")
                onClicked: app.emailAgent.accountsSyncAllFolders()
            }
            MenuItem {
                text: qsTr("Sync inboxes")
                onClicked: app.emailAgent.accountsSyncInbox()
            }
            MenuItem {
                visible: app.accounts.numberOfAccounts > 0
                text: qsTr("New mail")
                onClicked: app.compose(0)
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: page.single ? qsTr("No folders yet") : qsTr("No email accounts")
            hintText: page.single ? qsTr("Pull down to refresh the folder list")
                                  : qsTr("Add one in Settings → Accounts")
        }

        VerticalScrollDecorator { }
    }

    Component {
        id: folderDelegate
        FolderItem {
            accountId: page.singleAccountId
            folderId: model.folderId
            name: model.folderName
            nestingLevel: model.folderNestingLevel
            unreadCount: model.folderUnreadCount
            canHaveMessages: model.canHaveMessages
            onClicked: page.openFolder(index, model.folderId, PageStackAction.Animated)
        }
    }

    Component {
        id: accountDelegate
        ListItem {
            contentHeight: Theme.itemSizeMedium

            onClicked: pageStack.push(Qt.resolvedUrl("FoldersPage.qml"),
                                      { accountId: model.mailAccountId, title: model.displayName })

            Column {
                anchors {
                    left: parent.left
                    right: unread.left
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.paddingMedium
                    verticalCenter: parent.verticalCenter
                }
                Label {
                    width: parent.width
                    text: model.displayName
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    text: model.emailAddress
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                    truncationMode: TruncationMode.Fade
                }
            }

            Label {
                id: unread
                anchors {
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                text: model.unreadCount > 0 ? model.unreadCount : ""
                color: Theme.highlightColor
            }
        }
    }
}
