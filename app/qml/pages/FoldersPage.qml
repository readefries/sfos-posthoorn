// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1

Page {
    id: page

    property int accountId
    property string title

    FolderListModel {
        id: folders
        accountKey: page.accountId
    }

    Component.onCompleted: console.log("account", accountId, "has", folders.count, "folders")

    SilicaListView {
        id: list
        anchors.fill: parent
        model: folders

        header: PageHeader {
            title: page.title
        }

        PullDownMenu {
            busy: app.emailAgent.synchronizing
            MenuItem {
                text: qsTr("Refresh folder list")
                onClicked: app.emailAgent.retrieveFolderList(page.accountId)
            }
            MenuItem {
                text: qsTr("Sync account")
                onClicked: app.emailAgent.synchronize(page.accountId)
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: qsTr("No folders yet")
            hintText: qsTr("Pull down to refresh the folder list")
        }

        delegate: ListItem {
            enabled: model.canHaveMessages
            contentHeight: Theme.itemSizeSmall

            onClicked: pageStack.push(Qt.resolvedUrl("MessagesPage.qml"), {
                                          title: model.folderName,
                                          accountId: page.accountId,
                                          folderId: model.folderId,
                                          accessor: folders.folderAccessor(index)
                                      })

            Label {
                anchors {
                    left: parent.left
                    right: unread.left
                    leftMargin: Theme.horizontalPageMargin + model.folderNestingLevel * Theme.paddingLarge
                    rightMargin: Theme.paddingMedium
                    verticalCenter: parent.verticalCenter
                }
                text: model.folderName
                color: !model.canHaveMessages ? Theme.secondaryColor
                                              : highlighted ? Theme.highlightColor : Theme.primaryColor
                truncationMode: TruncationMode.Fade
            }

            Label {
                id: unread
                anchors {
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                text: model.folderUnreadCount > 0 ? model.folderUnreadCount : ""
                color: Theme.highlightColor
            }
        }

        VerticalScrollDecorator { }
    }
}
