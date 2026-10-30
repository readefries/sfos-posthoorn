// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1
import Posthoorn 1.0

Page {
    id: page

    // The folder's own name when not set.
    property string title
    readonly property string shownTitle: title.length > 0 ? title : folder.displayName
    // The unified inbox: the folders in app.unifiedFolders, of any account.
    property bool unified: false
    // A folder: both 0 for the unified inbox.
    property int accountId
    property int folderId
    property var accessor

    // In selection mode a tap (un)selects a message, and the panel at the bottom acts on the
    // selected ones. The selection lives in the model, so it goes when the page does.
    property bool selecting: false

    // One of these is used, the other one stays empty. The unified inbox's model leaves acting on
    // messages to EmailAgent, which the functions below take care of.
    readonly property QtObject messages: unified ? unifiedMessages : folderMessages

    EmailMessageListModel {
        id: folderMessages
        folderAccessor: page.unified ? null : page.accessor
    }

    UnifiedMessageListModel {
        id: unifiedMessages
        folderIds: page.unified ? app.unifiedFolderIds : []
    }

    EmailFolder {
        id: folder
        folderAccessor: page.unified ? null : page.accessor
    }

    onStatusChanged: {
        if (status !== PageStatus.Active)
            return
        if (unified)
            AppState.save("unified")
        else
            AppState.save("messages", accountId, folderId)
    }

    RemorsePopup {
        id: remorse
    }

    function sync() {
        if (!unified) {
            app.emailAgent.retrieveMessageList(accountId, folderId)
            return
        }
        var folders = app.unifiedFolders
        for (var i = 0; i < folders.length; ++i)
            app.emailAgent.retrieveMessageList(folders[i].accountId, folders[i].folderId)
    }

    function loadOlder() {
        if (!unified) {
            app.emailAgent.getMoreMessages(folderId)
            return
        }
        var folders = app.unifiedFolders
        for (var i = 0; i < folders.length; ++i)
            app.emailAgent.getMoreMessages(folders[i].folderId)
    }

    function markSelected(read) {
        if (!unified) {
            if (read)
                folderMessages.markAsReadSelectedMessages()
            else
                folderMessages.markAsUnReadSelectedMessages()
        } else {
            var ids = unifiedMessages.selectedMessageIds()
            for (var i = 0; i < ids.length; ++i) {
                if (read)
                    app.emailAgent.markMessageAsRead(ids[i])
                else
                    app.emailAgent.markMessageAsUnread(ids[i])
            }
        }
        stopSelecting()
    }

    function moveSelected(folderId) {
        if (!unified) {
            folderMessages.moveSelectedMessages(folderId)
        } else {
            var ids = unifiedMessages.selectedMessageIds()
            for (var i = 0; i < ids.length; ++i)
                app.emailAgent.moveMessage(ids[i], folderId)
        }
        stopSelecting()
    }

    function archiveSelected() {
        if (!unified) {
            folderMessages.moveSelectedMessages(ArchiveFolders.folderId(accountId))
        } else {
            var ids = unifiedMessages.selectedMessageIds()
            for (var i = 0; i < ids.length; ++i)
                app.archive(ids[i])
        }
        stopSelecting()
    }

    function deleteSelected() {
        if (!unified)
            folderMessages.deleteSelectedMessages()
        else
            app.emailAgent.deleteMessagesFromVariantList(unifiedMessages.selectedMessageIds())
        stopSelecting()
    }

    // The account to move the selection within, 0 when it mixes accounts: QMF moves within one.
    readonly property int moveAccountId: {
        if (!unified)
            return accountId
        messages.selectedMessageCount
        var accounts = unifiedMessages.selectedAccountIds()
        return accounts.length === 1 ? accounts[0] : 0
    }

    // Whether any of the selection can be archived; the rest stays where it is.
    readonly property bool canArchiveSelected: {
        if (!unified)
            return app.canArchive(accountId, folderId)
        messages.selectedMessageCount
        var accounts = unifiedMessages.selectedAccountIds()
        for (var i = 0; i < accounts.length; ++i) {
            if (app.canArchive(accounts[i], 0))
                return true
        }
        return false
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
            title: page.selecting ? qsTr("%n selected", "", messages.selectedMessageCount) : page.shownTitle
            description: page.selecting ? page.shownTitle : qsTr("%n message(s)", "", messages.count)
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

            // Sync is the closest to the finger.
            MenuItem {
                visible: !page.selecting
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                visible: !page.selecting
                text: qsTr("New mail")
                onClicked: app.compose(page.accountId)
            }
            MenuItem {
                visible: !page.selecting
                text: qsTr("Sync")
                onClicked: page.sync()
            }
        }

        PushUpMenu {
            visible: !page.selecting && list.count > 0
            busy: app.emailAgent.synchronizing
            MenuItem {
                text: qsTr("Load older messages")
                onClicked: page.loadOlder()
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: page.unified && app.unifiedFolders.length === 0 ? qsTr("No folders chosen")
                                                                    : qsTr("No messages")
            hintText: page.unified && app.unifiedFolders.length === 0
                      ? qsTr("Choose the folders to show here in Settings, in the pulley")
                      : qsTr("Pull down to sync")
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
                if (!page.selecting) {
                    // A draft is continued rather than read.
                    if (model.folderId === app.emailAgent.draftsFolderId(model.accountId))
                        pageStack.push(Qt.resolvedUrl("ComposePage.qml"), { draftId: model.messageId })
                    else
                        pageStack.push(Qt.resolvedUrl("MessagePage.qml"), { messageId: model.messageId })
                } else if (model.selected)
                    messages.deselectMessage(index)
                else
                    messages.selectMessage(index)
            }

            menu: ContextMenu {
                MenuItem {
                    visible: app.canArchive(model.accountId, model.folderId)
                    text: qsTr("Archive")
                    onClicked: app.archive(model.messageId)
                }
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
                        // In the unified inbox, where a message is when that isn't an inbox.
                        text: (page.unified && model.folderId !== app.emailAgent.inboxFolderId(model.accountId)
                               ? model.folderName + " · " : "")
                              + Format.formatDate(model.qDateTime, Formatter.TimeValue)
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
                onClicked: page.markSelected(true)
            }
            IconButton {
                icon.source: "image://theme/icon-m-mark-unread"
                enabled: panel.anySelected
                onClicked: page.markSelected(false)
            }
            IconButton {
                icon.source: "image://theme/icon-m-folder"
                enabled: panel.anySelected && page.moveAccountId > 0
                onClicked: page.pickFolder(page.moveAccountId, page.folderId, messages.selectedMessageCount,
                                           page.moveSelected)
            }
            IconButton {
                icon.source: "image://theme/icon-m-file-archive-folder"
                enabled: panel.anySelected && page.canArchiveSelected
                onClicked: page.archiveSelected()
            }
            IconButton {
                icon.source: "image://theme/icon-m-delete"
                enabled: panel.anySelected
                onClicked: remorse.execute(qsTr("Deleting %n message(s)", "", messages.selectedMessageCount),
                                           page.deleteSelected)
            }
        }
    }
}
