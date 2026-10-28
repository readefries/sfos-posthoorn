// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0

// Shows where a link in a mail goes before it's opened: the text of a link can say anything, and
// many links in newsletters go through a tracker first.
Dialog {
    id: dialog

    property string url

    readonly property string scheme: {
        var match = /^([a-z][a-z0-9+.-]*):/i.exec(url)
        return match ? match[1].toLowerCase() : ""
    }
    readonly property bool allowed: ["https", "http", "mailto", "tel"].indexOf(scheme) >= 0

    canAccept: allowed
    onAccepted: Qt.openUrlExternally(url)

    Column {
        width: parent.width
        spacing: Theme.paddingLarge

        DialogHeader {
            acceptText: qsTr("Open")
            title: qsTr("Open link")
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            text: qsTr("This opens:")
            wrapMode: Text.Wrap
            color: Theme.highlightColor
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            text: url
            wrapMode: Text.WrapAnywhere
            textFormat: Text.PlainText
            font.family: "monospace"
            font.pixelSize: Theme.fontSizeSmall
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            visible: scheme === "http"
            text: qsTr("Unencrypted: others on the network can see and change it.")
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.errorColor
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            visible: !allowed
            text: qsTr("Links of this type can't be opened.")
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.errorColor
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Copy link")
            onClicked: {
                Clipboard.text = dialog.url
                Notices.show(qsTr("Link copied"), Notice.Short, Notice.Center)
            }
        }
    }
}
