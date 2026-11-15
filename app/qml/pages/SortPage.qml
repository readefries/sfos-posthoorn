// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Posthoorn 1.0

// The order of message lists, chosen from the top left of a list (MessagesPage). A page rather
// than a menu under the list's header, which would open behind the camera cutout.
Page {
    id: page

    readonly property var choices: [
        { order: UnifiedMessageListModel.ByDate, text: qsTr("By date, newest first") },
        { order: UnifiedMessageListModel.OldestFirst, text: qsTr("By date, oldest first") },
        { order: UnifiedMessageListModel.BySender, text: qsTr("By sender, A to Z") },
        { order: UnifiedMessageListModel.BySenderDescending, text: qsTr("By sender, Z to A") },
        { order: UnifiedMessageListModel.BySize, text: qsTr("By size, largest first") }
    ]

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: qsTr("Order")
            }

            Repeater {
                model: page.choices

                BackgroundItem {
                    readonly property bool current: AppState.sortOrder === modelData.order

                    width: column.width
                    onClicked: {
                        AppState.sortOrder = modelData.order
                        app.popFrom(page)
                    }

                    Label {
                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        text: modelData.text
                        color: parent.highlighted || parent.current ? Theme.highlightColor : Theme.primaryColor
                        truncationMode: TruncationMode.Fade
                    }
                }
            }

            SectionHeader {
                text: qsTr("First")
            }

            TextSwitch {
                text: qsTr("Starred mail first")
                description: qsTr("Above the rest of the list, in the order above")
                checked: AppState.starredFirst
                onCheckedChanged: AppState.starredFirst = checked
            }

            TextSwitch {
                text: qsTr("Unread mail first")
                description: qsTr("Above the read mail, in the order above, below the starred mail when that's first")
                checked: AppState.unreadFirst
                onCheckedChanged: AppState.unreadFirst = checked
            }

            Item {
                width: 1
                height: Theme.paddingLarge
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("For all lists. By size, it's the size of the whole mail, mostly its attachments.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }
        }
    }
}
