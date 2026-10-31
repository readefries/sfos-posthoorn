// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1
import Posthoorn 1.0

// A folder's mail, the unified inbox, or a search through all mail.
Page {
    id: page

    // The folder's own name when not set.
    property string title
    readonly property string shownTitle: searching ? qsTr("Search")
                                                   : virtualFolder ? app.virtualFolderName(folderId)
                                                   : title.length > 0 ? title : folder.displayName
    // The unified inbox: the folders in app.unifiedFolders, of any account.
    property bool unified: false
    // Searching all folders of all accounts, or of searchAccountId.
    property bool searching: false
    // A folder: both 0 for the unified inbox and searching. folderId can be one of the account's
    // virtual folders (app.starredFolderId, app.snoozedFolderId).
    property int accountId
    property int folderId
    readonly property bool virtualFolder: !unified && !searching && app.isVirtualFolder(folderId)
    readonly property bool snoozedFolder: virtualFolder && folderId === app.snoozedFolderId
    property var accessor

    // What is searched for; the header's fields set these.
    property string searchText
    property bool searchBody: false
    property bool searchUnread: false
    property bool searchStarred: false
    property bool searchAttachments: false
    property int searchAccountId

    // In selection mode a tap (un)selects a message, and the panel at the bottom acts on the
    // selected ones. The selection lives in the model, so it goes when the page does.
    property bool selecting: false

    // Our model rather than the email plugin's, also for a single folder: it tells whether mail
    // is starred, and works the same for all three. Acting on messages goes through EmailAgent.
    readonly property QtObject messages: messageModel

    UnifiedMessageListModel {
        id: messageModel
        folderIds: page.searching || page.virtualFolder ? [] : page.unified ? app.unifiedFolderIds : [page.folderId]
        // Snoozed mail is out of sight until it's time.
        hiddenIds: Snoozes.messageIds
        search: page.virtualFolder ? app.virtualFolderSearch(page.folderId, page.accountId)
              : !page.searching ? ({}) : {
            "text": page.searchText,
            "body": page.searchBody,
            "unread": page.searchUnread,
            "starred": page.searchStarred,
            "attachments": page.searchAttachments,
            "accountId": page.searchAccountId
        }
    }

    EmailFolder {
        id: folder
        folderAccessor: page.unified || page.searching || page.virtualFolder ? null : page.accessor
    }

    onStatusChanged: {
        if (status !== PageStatus.Active || searching)
            return
        if (unified)
            AppState.save("unified")
        else
            AppState.save("messages", accountId, folderId)
        // So the new-mail notifications for it can go.
        if (unified)
            MailUi.inboxShown(0)
        else if (folderId === app.emailAgent.inboxFolderId(accountId))
            MailUi.inboxShown(accountId)
    }

    RemorsePopup {
        id: remorse
    }

    // The folders shown, as { accountId, folderId }.
    function shownFolders() {
        return unified ? app.unifiedFolders : [{ accountId: accountId, folderId: folderId }]
    }

    function sync() {
        // Its mail can be in any folder.
        if (virtualFolder) {
            app.emailAgent.synchronize(accountId)
            return
        }
        var folders = shownFolders()
        for (var i = 0; i < folders.length; ++i)
            app.emailAgent.retrieveMessageList(folders[i].accountId, folders[i].folderId)
    }

    function loadOlder() {
        var folders = shownFolders()
        for (var i = 0; i < folders.length; ++i)
            app.emailAgent.getMoreMessages(folders[i].folderId)
    }

    function markSelected(read) {
        var ids = messages.selectedMessageIds()
        for (var i = 0; i < ids.length; ++i) {
            if (read)
                app.emailAgent.markMessageAsRead(ids[i])
            else
                app.emailAgent.markMessageAsUnread(ids[i])
        }
        stopSelecting()
    }

    function moveSelected(folderId) {
        var ids = messages.selectedMessageIds()
        for (var i = 0; i < ids.length; ++i)
            app.emailAgent.moveMessage(ids[i], folderId)
        stopSelecting()
    }

    function archiveSelected() {
        var ids = messages.selectedMessageIds()
        for (var i = 0; i < ids.length; ++i)
            app.archive(ids[i])
        stopSelecting()
    }

    function deleteSelected() {
        app.emailAgent.deleteMessagesFromVariantList(messages.selectedMessageIds())
        stopSelecting()
    }

    // The account to move the selection within, 0 when it mixes accounts: QMF moves within one.
    readonly property int moveAccountId: {
        messages.selectedMessageCount
        var accounts = messages.selectedAccountIds()
        return accounts.length === 1 ? accounts[0] : 0
    }

    // Whether any of the selection can be archived; the rest stays where it is.
    readonly property bool canArchiveSelected: {
        if (!unified && !searching && !virtualFolder)
            return app.canArchive(accountId, folderId)
        messages.selectedMessageCount
        var ids = messages.selectedMessageIds()
        for (var i = 0; i < ids.length; ++i) {
            if (app.canArchive(app.emailAgent.accountIdForMessage(ids[i]), app.emailAgent.folderIdForMessage(ids[i])))
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

    readonly property bool canSwitchFolder: !unified && !searching && !selecting && accountId > 0

    // Picks another folder of the account and shows that instead of this one.
    function switchFolder() {
        var picker = pageStack.push(Qt.resolvedUrl("MoveToPage.qml"), {
                                        accountId: accountId,
                                        currentFolderId: folderId,
                                        title: qsTr("Folders"),
                                        description: app.accounts.displayNameFromAccountId(accountId),
                                        offerFavourites: true
                                    })
        picker.folderChosen.connect(function(chosen) {
            pageStack.replaceAbove(pageStack.previousPage(page), Qt.resolvedUrl("MessagesPage.qml"), {
                                       accountId: page.accountId,
                                       folderId: chosen,
                                       accessor: app.emailAgent.accessorFromFolderId(chosen)
                                   })
        })
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
            app.popFrom(picker, page)
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

        header: Column {
            width: list.width

            // A folder's title opens the account's other folders.
            BackgroundItem {
                width: parent.width
                height: pageHeader.height
                enabled: page.canSwitchFolder
                onClicked: page.switchFolder()

                PageHeader {
                    id: pageHeader
                    // See AccountsPage.
                    page: list.parent
                    title: page.selecting ? qsTr("%n selected", "", messages.selectedMessageCount)
                                          : page.shownTitle + (page.canSwitchFolder ? " ▾" : "")
                    description: page.selecting ? page.shownTitle
                                                : page.searching && messages.searchingServer
                                                  ? qsTr("Searching on the server…")
                                                  : page.searching && messages.searchingBodies
                                                    ? qsTr("Looking in the text of mails…")
                                                    : qsTr("%n message(s)", "", messages.count)
                }
            }

            SearchField {
                id: searchField
                visible: page.searching
                width: parent.width
                placeholderText: qsTr("Subject, sender or recipient")
                inputMethodHints: Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: {
                    searchDelay.stop()
                    page.searchText = text
                    focus = false
                }
                onTextChanged: searchDelay.restart()
                Component.onCompleted: {
                    if (page.searching)
                        forceActiveFocus()
                }

                // Not a new search for every letter typed.
                Timer {
                    id: searchDelay
                    interval: 400
                    onTriggered: page.searchText = searchField.text
                }
            }

            TextSwitch {
                visible: page.searching
                text: qsTr("Also in the text of mails")
                description: qsTr("Slower, and only mail whose text is downloaded")
                checked: page.searchBody
                onCheckedChanged: page.searchBody = checked
            }
            TextSwitch {
                visible: page.searching
                text: qsTr("Unread")
                checked: page.searchUnread
                onCheckedChanged: page.searchUnread = checked
            }
            TextSwitch {
                visible: page.searching
                text: qsTr("Starred")
                checked: page.searchStarred
                onCheckedChanged: page.searchStarred = checked
            }
            TextSwitch {
                visible: page.searching
                text: qsTr("With attachments")
                checked: page.searchAttachments
                onCheckedChanged: page.searchAttachments = checked
            }

            ComboBox {
                visible: page.searching && app.accounts.numberOfAccounts > 1
                label: qsTr("Accounts")
                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("All")
                        onClicked: page.searchAccountId = 0
                    }
                    Repeater {
                        model: app.accounts
                        MenuItem {
                            text: model.displayName
                            onClicked: page.searchAccountId = model.mailAccountId
                        }
                    }
                }
            }

            // Servers can search on text, unread and starred, not on attachments.
            Button {
                visible: page.searching && !messages.serverSearched
                         && (page.searchText.trim().length > 0 || page.searchUnread || page.searchStarred)
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Search on the server")
                onClicked: messages.searchServer()
            }
            Item {
                visible: page.searching
                width: 1
                height: Theme.paddingLarge
            }
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
                readonly property bool favourite: {
                    AppState.favouriteFolders
                    return AppState.isFavourite(page.accountId, page.folderId)
                }
                visible: page.canSwitchFolder
                text: favourite ? qsTr("Remove from favourites") : qsTr("Add to favourites")
                onClicked: AppState.setFavourite(page.accountId, page.folderId, !favourite)
            }
            MenuItem {
                visible: !page.selecting
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                visible: !page.selecting && !page.searching
                text: qsTr("Search")
                onClicked: app.search()
            }
            MenuItem {
                visible: !page.selecting
                text: qsTr("New mail")
                onClicked: app.compose(page.accountId)
            }
            MenuItem {
                visible: !page.selecting && !page.searching
                text: qsTr("Sync")
                onClicked: page.sync()
            }
        }

        PushUpMenu {
            visible: !page.selecting && !page.searching && !page.virtualFolder && list.count > 0
            busy: app.emailAgent.synchronizing
            MenuItem {
                text: qsTr("Load older messages")
                onClicked: page.loadOlder()
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0 && !(page.searching && (messages.searchingBodies || messages.searchingServer))
            // Below the search fields.
            verticalOffset: page.searching && list.headerItem ? list.headerItem.height / 2 : 0
            text: page.snoozedFolder ? qsTr("Nothing snoozed")
                  : page.virtualFolder ? qsTr("No starred mail")
                  : page.searching ? (page.searchText.trim().length > 0 || page.searchUnread || page.searchStarred
                                    || page.searchAttachments ? qsTr("Nothing found") : "")
                                 : page.unified && app.unifiedFolders.length === 0 ? qsTr("No folders chosen")
                                                                                   : qsTr("No messages")
            hintText: page.snoozedFolder ? qsTr("Snooze mail from its long-press menu")
                      : page.virtualFolder ? qsTr("Star mail from its long-press menu")
                      : page.searching ? (text.length > 0 ? (messages.serverSearched ? qsTr("Not on the server either")
                                                                                   : qsTr("Only mail on the phone was searched"))
                                                        : qsTr("Type something or pick a filter"))
                                     : page.unified && app.unifiedFolders.length === 0
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

            // When a snoozed mail comes back.
            function snoozedUntil() {
                var until = Snoozes.until(model.messageId)
                //: In the Snoozed folder: when a mail comes back. %1 is a date and time.
                return qsTr("until %1").arg(Format.formatDate(until, Formatter.DateMedium) + " "
                                            + Format.formatDate(until, Formatter.TimeValue))
            }

            menu: ContextMenu {
                MenuItem {
                    visible: app.canArchive(model.accountId, model.folderId)
                    text: qsTr("Archive")
                    onClicked: app.archive(model.messageId)
                }
                MenuItem {
                    text: page.snoozedFolder ? qsTr("Snooze until another time…") : qsTr("Snooze…")
                    onClicked: app.pickSnooze(page, [model.messageId])
                }
                MenuItem {
                    visible: page.snoozedFolder
                    text: qsTr("Unsnooze")
                    onClicked: {
                        var id = model.messageId
                        Snoozes.unsnooze(id)
                        app.emailAgent.markMessageAsUnread(id)
                    }
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
                    text: model.starred ? qsTr("Remove star") : qsTr("Star")
                    onClicked: app.setStarred(model.messageId, !model.starred)
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
                            right: model.starred ? star.left : time.left
                            rightMargin: Theme.paddingSmall
                        }
                        text: model.senderDisplayName
                        font.bold: !model.readStatus
                        color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                        truncationMode: TruncationMode.Fade
                    }
                    Icon {
                        id: star
                        visible: model.starred
                        anchors {
                            right: time.left
                            rightMargin: Theme.paddingSmall
                            verticalCenter: time.verticalCenter
                        }
                        source: "image://theme/icon-s-favorite"
                        highlighted: item.highlighted
                    }
                    Label {
                        id: time
                        anchors.right: parent.right
                        // In the unified inbox and search results, where a message is when that
                        // isn't an inbox.
                        text: ((page.unified || page.searching || page.virtualFolder) && model.folderId !== app.emailAgent.inboxFolderId(model.accountId)
                               ? model.folderName + " · " : "")
                              + (page.snoozedFolder ? item.snoozedUntil()
                                                    : Format.formatDate(model.qDateTime, Formatter.TimeValue))
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
