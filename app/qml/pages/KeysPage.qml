// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import Posthoorn 1.0

// The keys in Posthoorn's keyring: one's own (with a secret key) first, then other people's.
// Keys are imported from a file, such as one's own exported with gpg --export-secret-keys
// --armor, or from a key attached to a mail.
Page {
    id: page

    SilicaListView {
        id: list
        anchors.fill: parent
        model: app.pgpKeys.keys

        header: PageHeader {
            // The header doesn't find its page from inside the list; see AccountsPage.
            page: list.parent
            title: qsTr("PGP keys")
            description: app.pgpKeys.busy ? qsTr("Reading the keyring…") : ""
        }

        PullDownMenu {
            busy: app.pgpKeys.busy
            MenuItem {
                text: qsTr("Import from a file")
                onClicked: pageStack.push(filePicker)
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0 && !app.pgpKeys.busy
            text: app.pgpKeys.available ? qsTr("No keys yet") : qsTr("posthoorn-gnupg isn't installed")
            hintText: app.pgpKeys.available
                      ? qsTr("Import your own key, exported with gpg --export-secret-keys --armor, from the pulley")
                      : ""
        }

        delegate: ListItem {
            id: item
            contentHeight: column.height + 2 * Theme.paddingMedium

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Remove")
                    onClicked: {
                        var fingerprint = modelData.fingerprint
                        item.remorseDelete(function() { app.pgpKeys.remove(fingerprint) })
                    }
                }
            }

            Column {
                id: column
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    width: parent.width
                    text: modelData.name || modelData.keyId
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    text: [modelData.secret ? qsTr("Your key") : "", modelData.algorithm,
                           modelData.revoked ? qsTr("revoked") : modelData.expired ? qsTr("expired") : ""]
                          .filter(function(part) { return part.length > 0 }).join(" · ")
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: modelData.revoked || modelData.expired ? Theme.errorColor
                                                                   : item.highlighted ? Theme.secondaryHighlightColor
                                                                                      : Theme.secondaryColor
                }
                Label {
                    width: parent.width
                    text: modelData.fingerprint.replace(/(.{4})/g, "$1 ").trim()
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeTiny
                    font.family: "monospace"
                    color: Theme.secondaryColor
                }
            }
        }

        VerticalScrollDecorator { }
    }

    Component {
        id: filePicker
        FilePickerPage {
            title: qsTr("Key to import")
            nameFilters: ["*.asc", "*.gpg", "*.pgp", "*.key", "*.txt"]
            onSelectedContentPropertiesChanged: app.pgpKeys.importFile(selectedContentProperties.filePath)
        }
    }
}
