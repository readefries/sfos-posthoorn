// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

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
                id: allInboxes
                visible: list.count > 1
                onClicked: pageStack.push(Qt.resolvedUrl("MessagesPage.qml"), {
                                              title: qsTr("All inboxes"),
                                              accessor: app.emailAgent.combinedInboxAccessor()
                                          })
                Label {
                    anchors {
                        left: parent.left
                        right: parent.right
                        leftMargin: Theme.horizontalPageMargin
                        rightMargin: Theme.horizontalPageMargin
                        verticalCenter: parent.verticalCenter
                    }
                    text: qsTr("All inboxes")
                    color: allInboxes.highlighted ? Theme.highlightColor : Theme.primaryColor
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
