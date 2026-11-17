// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Posthoorn 1.0

// An account's patterns of addresses to reply from (SendingAddresses): added one at a time,
// removed from their long-press menu.
Page {
    id: page

    property int accountId
    property string accountName

    readonly property var patterns: {
        SendingAddresses.revision
        return SendingAddresses.patterns(accountId)
    }
    // Why the last pattern typed wasn't added.
    property string error

    function add() {
        var pattern = patternField.text.trim()
        if (pattern.length === 0)
            return
        if (patterns.indexOf(pattern.toLowerCase()) >= 0) {
            error = qsTr("%1 is already there").arg(pattern)
        } else if (!SendingAddresses.addPattern(accountId, pattern)) {
            error = qsTr("Not a pattern: use one address, with * for any part before or after the @, "
                         + "but not for the whole domain")
        } else {
            error = ""
            patternField.text = ""
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: qsTr("Reply addresses")
                description: page.accountName
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("A reply goes out from the address a mail was delivered to when it matches one of "
                           + "these. * stands for anything but @: *@example.com is every address of "
                           + "example.com, me+*@example.com every one starting with me+. The account's mail "
                           + "server has to accept sending from them.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }

            TextField {
                id: patternField
                width: parent.width
                label: page.error.length > 0 ? page.error : qsTr("Add a pattern")
                placeholderText: qsTr("Add a pattern, such as *@example.com")
                errorHighlight: page.error.length > 0
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onTextChanged: page.error = ""
                EnterKey.enabled: text.trim().length > 0
                EnterKey.iconSource: "image://theme/icon-m-add"
                EnterKey.onClicked: page.add()
            }

            SectionHeader {
                text: qsTr("Patterns")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: page.patterns.length === 0
                text: qsTr("None yet: replies go out from the account's address, or the alias the mail was sent to.")
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
            }

            Repeater {
                model: page.patterns

                ListItem {
                    id: item

                    width: column.width
                    menu: Component {
                        ContextMenu {
                            MenuItem {
                                text: qsTr("Remove")
                                onClicked: {
                                    var pattern = modelData
                                    item.remorseDelete(function() {
                                        SendingAddresses.removePattern(page.accountId, pattern)
                                    })
                                }
                            }
                        }
                    }

                    Label {
                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        text: modelData
                        color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                        truncationMode: TruncationMode.Fade
                    }
                }
            }
        }

        VerticalScrollDecorator { }
    }
}
