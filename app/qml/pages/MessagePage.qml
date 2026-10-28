// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1

Page {
    id: page

    property int messageId

    // Reading a body makes the plugin download it, so only read them once the message is set: the
    // bindings below can run before messageId is, which would request a download for message 0.
    readonly property bool loaded: message.messageId > 0
    readonly property string plainText: loaded ? message.body : ""
    readonly property string htmlText: loaded && showHtml ? message.htmlBody : ""
    // " " is what the plugin returns for a downloaded plain part with no text.
    readonly property bool hasPlainBody: plainText.trim().length > 0
    // Plain text when the mail has it, unless chosen otherwise from the pulley. The plugin can't
    // tell "no plain part" from "plain part still downloading", so this may switch once the body
    // arrives.
    property var htmlChoice: undefined
    readonly property bool showHtml: htmlChoice !== undefined
                                     ? htmlChoice
                                     : message.contentType === EmailMessage.HTML && !hasPlainBody

    EmailMessage {
        id: message
        messageId: page.messageId
    }

    Component.onCompleted: {
        if (!message.read)
            app.emailAgent.markMessageAsRead(messageId)
    }

    onStatusChanged: {
        if (status === PageStatus.Active)
            body.forceActiveFocus()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Quote selection")
                enabled: body.selectedText.length > 0
                onClicked: pageStack.push(Qt.resolvedUrl("SelectionPage.qml"), {
                                              sender: message.fromDisplayName || message.fromAddress,
                                              date: message.date,
                                              selection: body.selectedText
                                          })
            }
            MenuItem {
                visible: message.contentType === EmailMessage.HTML
                text: page.showHtml ? qsTr("Show plain text") : qsTr("Show HTML")
                onClicked: page.htmlChoice = !page.showHtml
            }
            MenuItem {
                text: qsTr("Mark as unread")
                onClicked: {
                    app.emailAgent.markMessageAsUnread(page.messageId)
                    pageStack.pop()
                }
            }
        }

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: message.subject
                wrapMode: Text.Wrap
            }

            DetailItem {
                label: qsTr("From")
                value: message.fromDisplayName.length > 0
                       ? message.fromDisplayName + " <" + message.fromAddress + ">"
                       : message.fromAddress
            }
            DetailItem {
                label: qsTr("To")
                value: message.to.join(", ")
            }
            DetailItem {
                visible: message.cc.length > 0
                label: qsTr("Cc")
                value: message.cc.join(", ")
            }
            DetailItem {
                label: qsTr("Date")
                value: Format.formatDate(message.date, Formatter.DateFull) + " "
                       + Format.formatDate(message.date, Formatter.TimeValue)
            }
            DetailItem {
                visible: message.numberOfAttachments > 0
                label: qsTr("Attachments")
                value: message.numberOfAttachments
            }

            Separator {
                width: parent.width
                color: Theme.primaryColor
                horizontalAlignment: Qt.AlignHCenter
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Medium
                running: visible
                // The plugin downloads the body when it's first asked for, and returns "" until then.
                visible: page.showHtml ? page.htmlText.length === 0 : page.plainText.length === 0
            }

            // Spike: shows the body as-is, to try out text selection. HTML goes through Qt's rich
            // text subset, which ignores scripts and doesn't fetch remote images; proper cleanup and
            // theming come with the reader milestone.
            TextArea {
                id: body
                width: parent.width
                readOnly: true
                // Silica only selects on press-and-hold in a focused editor, and doesn't focus
                // read-only ones on click. Read-only, focus doesn't bring up the keyboard.
                focusOnClick: true
                labelVisible: false
                text: page.showHtml ? page.htmlText : page.plainText
                color: Theme.primaryColor
                Component.onCompleted: _editor.textFormat = Qt.binding(function() {
                    return page.showHtml ? TextEdit.RichText : TextEdit.PlainText
                })
            }
        }

        VerticalScrollDecorator { }
    }
}
