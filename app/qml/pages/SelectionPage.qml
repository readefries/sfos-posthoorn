// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0

// Spike: shows how a reply quoting only the selected text would start. Compose comes later.
Page {
    id: page

    property string sender
    property var date
    property string selection

    function quote(text) {
        return text.split("\n").map(function(line) {
            return line.length > 0 && line[0] === ">" ? ">" + line : "> " + line
        }).join("\n")
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: qsTr("Quoted reply")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.family: "monospace"
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("On %1, %2 wrote:").arg(Format.formatDate(page.date, Formatter.DateMedium))
                                             .arg(page.sender)
                      + "\n" + page.quote(page.selection)
            }
        }

        VerticalScrollDecorator { }
    }
}
