// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Posthoorn 1.0

// Spike: shows how a reply quoting only the selected text would start. Compose comes later.
Page {
    id: page

    property string sender
    property var date
    property string selection

    // What would be sent: '>' quoting, which every mail client understands. Shown here the way the
    // reader shows quotes.
    readonly property string quoted: {
        // Text selected in rich text has U+2029 between paragraphs and U+2028 for line breaks.
        var lines = selection.replace(/\r\n|[\r  ]/g, "\n").split("\n")
        return qsTr("On %1, %2 wrote:").arg(Format.formatDate(page.date, Formatter.DateMedium))
                                       .arg(page.sender)
                + "\n" + lines.map(function(line) {
                    return line.length === 0 ? ">" : line[0] === ">" ? ">" + line : "> " + line
                }).join("\n")
    }

    MessageBody {
        id: formatted
        plainText: page.quoted
        textColor: Theme.primaryColor
        linkColor: Theme.highlightColor
        secondaryColor: Theme.secondaryColor
        quoteColors: [Theme.secondaryHighlightColor, Theme.secondaryColor]
        paragraphSpacing: Theme.paddingMedium
        indent: Theme.paddingLarge
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
                textFormat: Text.RichText
                text: formatted.text
            }
        }

        VerticalScrollDecorator { }
    }
}
