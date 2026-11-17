// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1
import Posthoorn 1.0

// One's own signatures, and which one each sending address uses.
Page {
    id: page

    // What an address can use: the account's signature, none, or one of these.
    readonly property var choices: {
        var list = [{ id: "account", name: qsTr("The account's signature") },
                    { id: "none", name: qsTr("No signature") }]
        var own = Signatures.list
        for (var i = 0; i < own.length; ++i)
            list.push({ id: own[i].id, name: own[i].name })
        return list
    }

    function indexOfChoice(choice) {
        for (var i = 0; i < choices.length; ++i) {
            if (choices[i].id === choice)
                return i
        }
        return 0
    }

    function edit(signature) {
        var dialog = pageStack.push(Qt.resolvedUrl("SignatureDialog.qml"), signature ? {
                                        signatureName: signature.name,
                                        signatureText: signature.text
                                    } : {})
        dialog.accepted.connect(function() {
            if (signature)
                Signatures.update(signature.id, dialog.signatureName, dialog.signatureText)
            else
                Signatures.add(dialog.signatureName, dialog.signatureText)
        })
    }

    EmailTransmitAddressListModel {
        id: identities
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("New signature")
                onClicked: page.edit(null)
            }
        }

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: qsTr("Signatures")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Signatures of your own, for any of your addresses. The account's signature is "
                           + "the one set in Settings → Accounts. Each mail can still use another.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }

            SectionHeader {
                text: qsTr("Your signatures")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: Signatures.list.length === 0
                text: qsTr("None yet. Pull down to add one.")
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
            }

            Repeater {
                model: Signatures.list

                ListItem {
                    id: item

                    width: column.width
                    contentHeight: texts.height + 2 * Theme.paddingMedium
                    onClicked: page.edit(modelData)
                    menu: Component {
                        ContextMenu {
                            MenuItem {
                                text: qsTr("Edit")
                                onClicked: page.edit(modelData)
                            }
                            MenuItem {
                                text: qsTr("Delete")
                                onClicked: {
                                    var id = modelData.id
                                    item.remorseDelete(function() { Signatures.remove(id) })
                                }
                            }
                        }
                    }

                    Column {
                        id: texts
                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        Label {
                            width: parent.width
                            text: modelData.name
                            color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                            truncationMode: TruncationMode.Fade
                        }
                        Label {
                            width: parent.width
                            text: modelData.text
                            textFormat: Text.PlainText
                            maximumLineCount: 3
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                            font.pixelSize: Theme.fontSizeSmall
                            color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        }
                    }
                }
            }

            SectionHeader {
                text: qsTr("Per address")
            }

            Repeater {
                model: identities

                ComboBox {
                    id: box

                    readonly property string address: model.emailAddress

                    width: column.width
                    label: address
                    // Set rather than bound: choosing from the menu sets it too.
                    function update() {
                        currentIndex = page.indexOfChoice(Signatures.choiceFor(address))
                    }
                    Component.onCompleted: update()

                    Connections {
                        target: Signatures
                        onChanged: box.update()
                    }

                    menu: ContextMenu {
                        Repeater {
                            model: page.choices
                            MenuItem {
                                text: modelData.name
                                onClicked: Signatures.setChoiceFor(box.address, modelData.id)
                            }
                        }
                    }
                }
            }
        }

        VerticalScrollDecorator { }
    }
}
