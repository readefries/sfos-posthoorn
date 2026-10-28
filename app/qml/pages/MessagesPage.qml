// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1

Page {
    id: page

    property string title
    // Both 0 for the combined inbox.
    property int accountId
    property int folderId
    property var accessor

    EmailMessageListModel {
        id: messages
        folderAccessor: page.accessor
        onCountChanged: console.log("folder", page.folderId, "now has", count, "messages")
    }

    Component.onCompleted: console.log("opened folder", folderId, "of account", accountId,
                                       "accessor", accessor, "messages", messages.count)

    function sync() {
        if (folderId > 0)
            app.emailAgent.retrieveMessageList(accountId, folderId)
        else
            app.emailAgent.accountsSyncInbox()
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: messages

        header: PageHeader {
            title: page.title
            description: qsTr("%n message(s)", "", messages.count)
        }

        PullDownMenu {
            busy: app.emailAgent.synchronizing
            MenuItem {
                text: qsTr("Sync")
                onClicked: page.sync()
            }
        }

        PushUpMenu {
            visible: page.folderId > 0
            busy: app.emailAgent.synchronizing
            MenuItem {
                text: qsTr("Load older messages")
                onClicked: app.emailAgent.getMoreMessages(page.folderId)
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: qsTr("No messages")
            hintText: qsTr("Pull down to sync")
        }

        section {
            property: "timeSection"
            delegate: SectionHeader {
                text: Format.formatDate(new Date(section), Formatter.TimepointSectionRelative)
            }
        }

        delegate: ListItem {
            id: item
            contentHeight: column.height + 2 * Theme.paddingMedium

            onClicked: pageStack.push(Qt.resolvedUrl("MessagePage.qml"), { messageId: model.messageId })

            menu: ContextMenu {
                MenuItem {
                    text: model.readStatus ? qsTr("Mark as unread") : qsTr("Mark as read")
                    onClicked: {
                        if (model.readStatus)
                            app.emailAgent.markMessageAsUnread(model.messageId)
                        else
                            app.emailAgent.markMessageAsRead(model.messageId)
                    }
                }
                MenuItem {
                    text: qsTr("Delete")
                    onClicked: {
                        var id = model.messageId
                        item.remorseDelete(function() { app.emailAgent.deleteMessage(id) })
                    }
                }
            }

            // Unread marker.
            Rectangle {
                visible: !model.readStatus
                width: Theme.paddingSmall
                height: column.height
                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                }
                color: Theme.highlightColor
            }

            Column {
                id: column
                anchors {
                    left: parent.left
                    right: parent.right
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }

                Item {
                    width: parent.width
                    height: sender.height

                    Label {
                        id: sender
                        anchors {
                            left: parent.left
                            right: time.left
                            rightMargin: Theme.paddingSmall
                        }
                        text: model.senderDisplayName
                        font.bold: !model.readStatus
                        color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        id: time
                        anchors.right: parent.right
                        text: Format.formatDate(model.qDateTime, Formatter.TimeValue)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                    }
                }
                Label {
                    width: parent.width
                    text: model.subject
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: !model.readStatus
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    text: model.preview
                    visible: text.length > 0
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }
            }
        }

        VerticalScrollDecorator { }
    }
}
