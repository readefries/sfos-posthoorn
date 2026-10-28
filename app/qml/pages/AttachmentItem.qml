// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0

// An attachment of a mail being written, with a menu to remove it.
ListItem {
    id: item

    property string name
    property real size
    // Shown instead of the size, e.g. while downloading.
    property string status

    signal removeRequested()

    contentHeight: Theme.itemSizeSmall

    menu: ContextMenu {
        MenuItem {
            text: qsTr("Remove")
            onClicked: item.removeRequested()
        }
    }

    Icon {
        id: icon
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        source: "image://theme/icon-m-attach"
    }

    Column {
        anchors {
            left: icon.right
            right: parent.right
            leftMargin: Theme.paddingMedium
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        Label {
            width: parent.width
            text: item.name
            truncationMode: TruncationMode.Fade
            color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
        }
        Label {
            text: item.status.length > 0 ? item.status : Format.formatFileSize(item.size)
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }
    }
}
