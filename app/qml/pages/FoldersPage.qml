// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1
import Posthoorn 1.0

Page {
    id: page

    property int accountId
    property string title
    // Set when the app starts where it was left: the folder to open right away.
    property int restoreFolderId

    function openFolder(index, folderId, name, operation) {
        pageStack.push(Qt.resolvedUrl("MessagesPage.qml"), {
                           title: name,
                           accountId: page.accountId,
                           folderId: folderId,
                           accessor: folders.folderAccessor(index)
                       }, operation)
    }

    onStatusChanged: {
        if (status !== PageStatus.Active)
            return
        if (restoreFolderId > 0) {
            restoreLater.start()
            return
        }
        AppState.save("folders", accountId)
    }

    // Opens the folder the app was left in. Right after this page becomes active, not while it
    // does: see AccountsPage.
    Timer {
        id: restoreLater
        interval: 0
        onTriggered: {
            var folderId = page.restoreFolderId
            page.restoreFolderId = 0
            var index = folders.indexFromFolderId(folderId)
            if (index >= 0)
                // The name comes from the folder itself, see MessagesPage.
                page.openFolder(index, folderId, "", PageStackAction.Immediate)
            else
                AppState.save("folders", page.accountId)
        }
    }

    FolderListModel {
        id: folders
        accountKey: page.accountId
    }

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
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                text: qsTr("Search")
                onClicked: app.search()
            }
            MenuItem {
                text: qsTr("Refresh folder list")
                onClicked: app.emailAgent.retrieveFolderList(page.accountId)
            }
            MenuItem {
                text: qsTr("Sync account")
                onClicked: app.emailAgent.synchronize(page.accountId)
            }
            MenuItem {
                text: qsTr("New mail")
                onClicked: app.compose(page.accountId)
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
            menu: model.folderUnreadCount > 0 ? readMenu : null

            onClicked: page.openFolder(index, model.folderId, model.folderName, PageStackAction.Animated)

            Component {
                id: readMenu
                ContextMenu {
                    MenuItem {
                        text: qsTr("Mark all as read")
                        onClicked: app.markFoldersRead([model.folderId])
                    }
                }
            }

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
