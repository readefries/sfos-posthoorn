// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0

// A signature's name and text, new or to change.
Dialog {
    id: dialog

    property string signatureName
    property string signatureText

    canAccept: nameField.text.trim().length > 0 && textArea.text.trim().length > 0

    onAccepted: {
        signatureName = nameField.text.trim()
        // Without empty lines at the end; the composer puts the "-- " line above it.
        signatureText = textArea.text.replace(/\s+$/, "")
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            DialogHeader {
                acceptText: qsTr("Save")
            }

            TextField {
                id: nameField
                width: parent.width
                label: qsTr("Name")
                placeholderText: qsTr("Name, e.g. Work or Personal")
                text: dialog.signatureName
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: textArea.forceActiveFocus()
            }

            TextArea {
                id: textArea
                width: parent.width
                label: qsTr("Signature")
                placeholderText: qsTr("Signature")
                text: dialog.signatureText
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Mail goes out as plain text, so the signature is text too. It comes after your "
                           + "text, below a \"-- \" line, so mail apps can tell it apart.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }
        }

        VerticalScrollDecorator { }
    }

    Component.onCompleted: {
        if (signatureName.length === 0)
            nameField.forceActiveFocus()
    }
}
