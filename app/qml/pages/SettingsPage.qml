// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Posthoorn 1.0

// The unified inbox's folders and each account's archive folder.
Page {
    id: page

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: qsTr("Settings")
            }

            SectionHeader {
                text: qsTr("Unified inbox")
            }

            ValueButton {
                label: qsTr("Folders")
                value: AppState.unifiedConfigured ? qsTr("%n folder(s)", "", app.unifiedFolders.length)
                                                  : qsTr("The inboxes of all accounts")
                onClicked: pageStack.push(Qt.resolvedUrl("UnifiedFoldersPage.qml"))
            }

            SectionHeader {
                text: qsTr("Archive")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Archiving moves mail to this folder of its account. Unless one is chosen, it's "
                           + "the folder called Archive, or Gmail's All Mail.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }

            Repeater {
                model: app.accounts

                ListItem {
                    id: item

                    readonly property int accountId: model.mailAccountId
                    readonly property int archiveId: {
                        ArchiveFolders.revision
                        return ArchiveFolders.folderId(accountId)
                    }
                    readonly property bool chosen: {
                        ArchiveFolders.revision
                        return ArchiveFolders.chosen(accountId)
                    }

                    width: column.width
                    contentHeight: Theme.itemSizeMedium

                    onClicked: {
                        var picker = pageStack.push(Qt.resolvedUrl("MoveToPage.qml"), {
                                                        accountId: item.accountId,
                                                        currentFolderId: item.archiveId,
                                                        title: qsTr("Archive folder"),
                                                        description: model.displayName
                                                    })
                        picker.folderChosen.connect(function(folderId) {
                            ArchiveFolders.choose(item.accountId, folderId)
                            app.popFrom(picker, page)
                        })
                    }

                    menu: item.archiveId > 0 || item.chosen ? archiveMenu : null

                    Component {
                        id: archiveMenu
                        ContextMenu {
                            MenuItem {
                                visible: item.archiveId > 0
                                text: qsTr("No archive folder")
                                onClicked: ArchiveFolders.choose(item.accountId, 0)
                            }
                            MenuItem {
                                visible: item.chosen
                                text: qsTr("Find it by name")
                                onClicked: ArchiveFolders.forget(item.accountId)
                            }
                        }
                    }

                    Column {
                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        Label {
                            width: parent.width
                            text: model.displayName
                            color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                            truncationMode: TruncationMode.Fade
                        }
                        Label {
                            width: parent.width
                            text: {
                                ArchiveFolders.revision
                                if (item.archiveId <= 0)
                                    return item.chosen ? qsTr("No archive folder") : qsTr("None found, tap to choose one")
                                var name = ArchiveFolders.name(item.accountId)
                                return item.chosen ? name : qsTr("%1 (found by its name)").arg(name)
                            }
                            font.pixelSize: Theme.fontSizeSmall
                            color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                            truncationMode: TruncationMode.Fade
                        }
                    }
                }
            }
        }

        VerticalScrollDecorator { }
    }
}
