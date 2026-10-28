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

    // In selection mode a tap (un)selects a message, and the panel at the bottom acts on the
    // selected ones. The selection lives in the model, so it goes when the page does.
    property bool selecting: false

    EmailMessageListModel {
        id: messages
        folderAccessor: page.accessor
    }

    RemorsePopup {
        id: remorse
    }

    function sync() {
        if (folderId > 0)
            app.emailAgent.retrieveMessageList(accountId, folderId)
        else
            app.emailAgent.accountsSyncInbox()
    }

    function startSelecting(index) {
        selecting = true
        if (index !== undefined)
            messages.selectMessage(index)
    }

    function stopSelecting() {
        messages.deselectAllMessages()
        selecting = false
    }

    // Lets the user pick a folder of accountId, then calls moved with it and comes back here.
    function pickFolder(accountId, currentFolderId, count, moved) {
        var picker = pageStack.push(Qt.resolvedUrl("MoveToPage.qml"), {
                                        accountId: accountId,
                                        currentFolderId: currentFolderId,
                                        count: count
                                    })
        picker.folderChosen.connect(function(folderId) {
            moved(folderId)
            pageStack.pop(page)
        })
    }

    SilicaListView {
        id: list
        anchors {
            fill: parent
            bottomMargin: panel.visibleSize
        }
        clip: panel.expanded
        model: messages

        header: PageHeader {
            title: page.selecting ? qsTr("%n selected", "", messages.selectedMessageCount) : page.title
            description: page.selecting ? page.title : qsTr("%n message(s)", "", messages.count)
        }

        PullDownMenu {
            busy: app.emailAgent.synchronizing

            MenuItem {
                visible: page.selecting
                text: qsTr("Select all")
                onClicked: messages.selectAllMessages()
            }
            MenuItem {
                visible: page.selecting
                text: qsTr("Select none")
                onClicked: messages.deselectAllMessages()
            }
            MenuItem {
                visible: page.selecting
                text: qsTr("Done")
                onClicked: page.stopSelecting()
            }

            MenuItem {
                visible: !page.selecting && list.count > 0
                text: qsTr("Select messages")
                onClicked: page.startSelecting()
            }
            MenuItem {
                visible: !page.selecting && list.count > 0
                text: qsTr("Mark all as read")
                onClicked: messages.markAllMessagesAsRead()
            }
            MenuItem {
                visible: !page.selecting
                text: qsTr("Sync")
                onClicked: page.sync()
            }
            MenuItem {
                visible: !page.selecting
                text: qsTr("New mail")
                onClicked: app.compose(page.accountId)
            }
        }

        PushUpMenu {
            visible: page.folderId > 0 && !page.selecting
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
            highlighted: down || menuOpen || (page.selecting && model.selected)
            openMenuOnPressAndHold: !page.selecting

            onClicked: {
                if (!page.selecting)
                    pageStack.push(Qt.resolvedUrl("MessagePage.qml"), { messageId: model.messageId })
                else if (model.selected)
                    messages.deselectMessage(index)
                else
                    messages.selectMessage(index)
            }

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
                    text: qsTr("Move to…")
                    onClicked: {
                        // The delegate may be gone by the time a folder is picked.
                        var id = model.messageId
                        page.pickFolder(model.accountId, model.folderId, 1, function(folderId) {
                            app.emailAgent.moveMessage(id, folderId)
                        })
                    }
                }
                MenuItem {
                    text: qsTr("Select")
                    onClicked: page.startSelecting(index)
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

    DockedPanel {
        id: panel
        width: parent.width
        height: Theme.itemSizeLarge
        dock: Dock.Bottom
        open: page.selecting

        readonly property bool anySelected: messages.selectedMessageCount > 0

        Row {
            anchors.centerIn: parent
            spacing: Theme.paddingLarge * 2

            IconButton {
                icon.source: "image://theme/icon-m-mail-open"
                enabled: panel.anySelected
                onClicked: {
                    messages.markAsReadSelectedMessages()
                    page.stopSelecting()
                }
            }
            IconButton {
                icon.source: "image://theme/icon-m-mark-unread"
                enabled: panel.anySelected
                onClicked: {
                    messages.markAsUnReadSelectedMessages()
                    page.stopSelecting()
                }
            }
            IconButton {
                icon.source: "image://theme/icon-m-folder"
                // QMF moves within an account only, and the combined inbox mixes them.
                enabled: panel.anySelected && page.accountId > 0
                onClicked: page.pickFolder(page.accountId, page.folderId, messages.selectedMessageCount,
                                           function(folderId) {
                                               messages.moveSelectedMessages(folderId)
                                               page.selecting = false
                                           })
            }
            IconButton {
                icon.source: "image://theme/icon-m-delete"
                enabled: panel.anySelected
                onClicked: remorse.execute(qsTr("Deleting %n message(s)", "", messages.selectedMessageCount),
                                           function() {
                                               messages.deleteSelectedMessages()
                                               page.selecting = false
                                           })
            }
        }
    }
}
