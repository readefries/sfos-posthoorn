// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1
import Posthoorn 1.0

// Choosing the folders the unified inbox shows, of all accounts. Until something is changed here
// it shows every account's inbox.
Page {
    id: page

    function entry(accountId, folderId) {
        return accountId + "/" + folderId
    }

    // What the unified inbox shows now, as AppState stores it.
    function currentEntries() {
        var folders = app.unifiedFolders
        var entries = []
        for (var i = 0; i < folders.length; ++i)
            entries.push(entry(folders[i].accountId, folders[i].folderId))
        return entries
    }

    function isShown(accountId, folderId) {
        var folders = app.unifiedFolders
        for (var i = 0; i < folders.length; ++i) {
            if (folders[i].accountId === accountId && folders[i].folderId === folderId)
                return true
        }
        return false
    }

    function setShown(accountId, folderId, shown) {
        var entries = currentEntries()
        var key = entry(accountId, folderId)
        var index = entries.indexOf(key)
        if (shown && index < 0)
            entries.push(key)
        else if (!shown && index >= 0)
            entries.splice(index, 1)
        AppState.setUnifiedFolders(entries)
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            enabled: AppState.unifiedConfigured
            MenuItem {
                text: qsTr("Show the inboxes of all accounts")
                onClicked: AppState.resetUnifiedFolders()
            }
        }

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: qsTr("Unified inbox")
                description: AppState.unifiedConfigured ? qsTr("%n folder(s)", "", app.unifiedFolders.length)
                                                        : qsTr("The inboxes of all accounts")
            }

            Repeater {
                model: app.accounts

                Column {
                    id: account
                    width: column.width

                    readonly property int accountId: model.mailAccountId

                    SectionHeader {
                        text: model.displayName
                    }

                    Repeater {
                        model: FolderListModel {
                            accountKey: account.accountId
                        }

                        TextSwitch {
                            visible: model.canHaveMessages
                            leftMargin: Theme.horizontalPageMargin + model.folderNestingLevel * Theme.paddingLarge
                            text: model.folderName
                            // Set from the stored choice, not by the switch itself.
                            automaticCheck: false
                            checked: page.isShown(account.accountId, model.folderId)
                            onClicked: page.setShown(account.accountId, model.folderId, !checked)
                        }
                    }
                }
            }
        }

        VerticalScrollDecorator { }
    }
}
