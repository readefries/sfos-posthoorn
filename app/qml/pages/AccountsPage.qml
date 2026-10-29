// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Posthoorn 1.0

Page {
    id: page

    property bool restored: false

    function openUnified(operation) {
        pageStack.push(Qt.resolvedUrl("MessagesPage.qml"), {
                           unified: true,
                           title: qsTr("Unified inbox")
                       }, operation)
    }

    // Opens the folder or account the app was left in.
    function restore() {
        var accountId = AppState.accountId
        // "messages" without an account was the plugin's combined inbox, before the unified one.
        if (AppState.page === "unified" || (AppState.page === "messages" && accountId === 0)) {
            if (app.accounts.numberOfAccounts > 0)
                openUnified(PageStackAction.Immediate)
        } else if ((AppState.page === "folders" || AppState.page === "messages")
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
            restore()
        } else {
            AppState.save("accounts")
        }
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: app.accounts

        header: Column {
            width: list.width

            PageHeader {
                title: qsTr("Accounts")
            }

            BackgroundItem {
                id: unifiedInbox
                visible: list.count > 0
                height: Theme.itemSizeMedium
                onClicked: page.openUnified(PageStackAction.Animated)
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
        }

        PullDownMenu {
            busy: app.emailAgent.synchronizing
            MenuItem {
                text: qsTr("Sync all folders")
                onClicked: app.emailAgent.accountsSyncAllFolders()
            }
            MenuItem {
                text: qsTr("Sync inboxes")
                onClicked: app.emailAgent.accountsSyncInbox()
            }
            MenuItem {
                visible: list.count > 0
                text: qsTr("New mail")
                onClicked: app.compose(0)
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: qsTr("No email accounts")
            hintText: qsTr("Add one in Settings → Accounts")
        }

        delegate: ListItem {
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

        VerticalScrollDecorator { }
    }
}
