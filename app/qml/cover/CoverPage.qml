// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.4
import Sailfish.Silica 1.0
import Posthoorn 1.0

// What's waiting in the unified inbox: how many unread mails, and the newest of them, as many as
// fit above the cover's buttons.
CoverBackground {
    id: cover

    UnifiedMessageListModel {
        id: unread
        folderIds: app.unifiedFolderIds
        unreadOnly: true
    }

    CoverPlaceholder {
        visible: unread.count === 0
        //: Cover, when the unified inbox has no unread mail
        text: qsTr("No new mail")
        icon.source: "/usr/share/icons/hicolor/86x86/apps/posthoorn.png"
    }

    Item {
        id: count
        visible: unread.count > 0
        x: Theme.paddingLarge
        y: Theme.paddingMedium
        width: parent.width - 2 * Theme.paddingLarge
        height: countLabel.height

        Label {
            id: countLabel
            text: unread.count
            font.pixelSize: Theme.fontSizeHuge
            color: Theme.highlightColor
        }
        Label {
            anchors {
                left: countLabel.right
                leftMargin: Theme.paddingSmall
                right: parent.right
                baseline: countLabel.baseline
            }
            //: Cover, after the number of unread mails
            text: qsTr("unread")
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.highlightColor
            truncationMode: TruncationMode.Fade
        }
    }

    ListView {
        id: list
        visible: unread.count > 0
        anchors {
            top: count.bottom
            topMargin: Theme.paddingSmall
            left: parent.left
            right: parent.right
            leftMargin: Theme.paddingLarge
            rightMargin: Theme.paddingLarge
        }
        // Down to the buttons, without cutting a mail in half.
        readonly property real space: (cover.coverActionArea ? cover.coverActionArea.y : cover.height) - y
        readonly property real itemHeight: senderMetrics.height + subjectMetrics.height + Theme.paddingSmall
        height: Math.max(0, Math.floor(space / itemHeight)) * itemHeight
        interactive: false
        clip: true
        model: unread

        delegate: Column {
            width: list.width
            height: list.itemHeight

            Label {
                width: parent.width
                text: model.senderDisplayName
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.primaryColor
                truncationMode: TruncationMode.Fade
            }
            Label {
                width: parent.width
                text: model.subject
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                truncationMode: TruncationMode.Fade
            }
        }
    }

    FontMetrics {
        id: senderMetrics
        font.pixelSize: Theme.fontSizeExtraSmall
    }
    FontMetrics {
        id: subjectMetrics
        font.pixelSize: Theme.fontSizeExtraSmall
    }

    CoverActionList {
        CoverAction {
            iconSource: "image://theme/icon-cover-new"
            onTriggered: {
                app.activate()
                app.request(function() { app.compose(0) })
            }
        }
        CoverAction {
            iconSource: "image://theme/icon-cover-sync"
            onTriggered: app.emailAgent.accountsSyncInbox()
        }
    }
}
