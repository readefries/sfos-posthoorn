// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Posthoorn 1.0

// A folder in a folder list: its name, indented by its level, and its unread count, with a
// long-press menu for the favourites and marking it read.
ListItem {
    id: item

    property int accountId
    property int folderId
    property string name
    property int nestingLevel
    property int unreadCount
    property bool canHaveMessages: true

    enabled: canHaveMessages
    contentHeight: Theme.itemSizeSmall

    menu: ContextMenu {
        MenuItem {
            readonly property bool favourite: {
                AppState.favouriteFolders
                return AppState.isFavourite(item.accountId, item.folderId)
            }
            text: favourite ? qsTr("Remove from favourites") : qsTr("Add to favourites")
            onClicked: AppState.setFavourite(item.accountId, item.folderId, !favourite)
        }
        MenuItem {
            // Snoozed mail is read already; its count is how much is snoozed.
            visible: item.unreadCount > 0 && item.folderId !== app.snoozedFolderId
            text: qsTr("Mark all as read")
            onClicked: {
                if (app.isVirtualFolder(item.folderId))
                    app.markVirtualFolderRead(item.accountId, item.folderId)
                else
                    app.markFoldersRead([item.folderId])
            }
        }
    }

    Label {
        anchors {
            left: parent.left
            right: unread.left
            leftMargin: Theme.horizontalPageMargin + item.nestingLevel * Theme.paddingLarge
            rightMargin: Theme.paddingMedium
            verticalCenter: parent.verticalCenter
        }
        text: item.name
        color: !item.canHaveMessages ? Theme.secondaryColor
                                     : item.highlighted ? Theme.highlightColor : Theme.primaryColor
        truncationMode: TruncationMode.Fade
    }

    Label {
        id: unread
        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        text: item.unreadCount > 0 ? item.unreadCount : ""
        color: Theme.highlightColor
    }
}
