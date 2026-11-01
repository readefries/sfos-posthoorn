// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0

// Asks for the passphrase of one's key, for decrypting or signing.
Dialog {
    id: dialog

    property alias passphrase: field.text
    property string acceptText
    // Why it's asked again, such as a passphrase that didn't work.
    property string error

    canAccept: field.text.length > 0

    Column {
        width: parent.width

        DialogHeader {
            acceptText: dialog.acceptText
        }

        PasswordField {
            id: field
            width: parent.width
            label: qsTr("Passphrase of your key")
            placeholderText: label
            focus: true
            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.onClicked: if (text.length > 0) accept()
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            visible: text.length > 0
            text: dialog.error
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.errorColor
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            text: qsTr("Posthoorn keeps it for 10 minutes, in memory only, so it doesn't ask again for a while.")
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
        }
    }
}
