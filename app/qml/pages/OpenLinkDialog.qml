// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Posthoorn 1.0

// Shows where a link in a mail goes before it's opened: the text of a link can say anything, and
// many links in newsletters go through a tracker first. Opens the cleaned address (redirectors
// unwrapped, tracking parameters out) unless the original is asked for.
Dialog {
    id: dialog

    // As it is in the mail.
    property string url
    property bool useOriginal: false

    readonly property var cleaned: Links.clean(url)
    readonly property string target: useOriginal ? url : cleaned.url

    readonly property string scheme: {
        var match = /^([a-z][a-z0-9+.-]*):/i.exec(target)
        return match ? match[1].toLowerCase() : ""
    }
    readonly property bool allowed: ["https", "http", "mailto", "tel"].indexOf(scheme) >= 0

    canAccept: allowed
    onAccepted: Qt.openUrlExternally(target)

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
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
                text: dialog.target
                wrapMode: Text.WrapAnywhere
                textFormat: Text.PlainText
                font.family: "monospace"
                font.pixelSize: Theme.fontSizeSmall
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: dialog.cleaned.changed
                text: dialog.useOriginal
                      ? qsTr("The link as it is in the mail.")
                      : dialog.cleaned.unwrapped && dialog.cleaned.stripped
                        ? qsTr("Taken out of a redirect, which would see you open it, and without tracking parameters.")
                        : dialog.cleaned.unwrapped
                          ? qsTr("Taken out of a redirect, which would see you open it.")
                          : qsTr("Without tracking parameters.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: dialog.scheme === "http"
                text: qsTr("Unencrypted: others on the network can see and change it.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.errorColor
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !dialog.allowed
                text: qsTr("Links of this type can't be opened.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.errorColor
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: dialog.cleaned.changed
                text: dialog.useOriginal ? qsTr("Use the cleaned link") : qsTr("Use the original link")
                onClicked: dialog.useOriginal = !dialog.useOriginal
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Copy link")
                onClicked: {
                    Clipboard.text = dialog.target
                    Notices.show(qsTr("Link copied"), Notice.Short, Notice.Center)
                }
            }
        }
    }
}
