// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1
import Posthoorn 1.0

Page {
    id: page

    property int messageId

    // The HTML part when there is one, unless the text part is asked for. Either way it's shown
    // reformatted, never as the sender laid it out.
    property bool preferTextPart: false
    readonly property bool useHtml: message.contentType === EmailMessage.HTML && !preferTextPart
    property bool showImages: false

    // Reading a body makes the plugin download it, so only read the one that's used, and only once
    // the message is set: the bindings can run before messageId is, which would request a download
    // for message 0.
    readonly property bool loaded: message.messageId > 0
    readonly property string source: !loaded ? "" : useHtml ? message.htmlBody : message.body

    EmailMessage {
        id: message
        messageId: page.messageId
    }

    MessageBody {
        id: formatted
        html: page.useHtml ? page.source : ""
        plainText: page.useHtml ? "" : page.source
        showImages: page.showImages
        textColor: Theme.primaryColor
        linkColor: Theme.highlightColor
        secondaryColor: Theme.secondaryColor
        quoteColors: [Theme.secondaryHighlightColor, Theme.secondaryColor]
        paragraphSpacing: Theme.paddingMedium
        indent: Theme.paddingLarge
        maxImageWidth: body.width - 2 * Theme.horizontalPageMargin
        //: Shown in a mail instead of an image until images are loaded. %1 describes the image.
        imagePlaceholder: qsTr("[Image: %1]")
        //: Description of an image in a mail that has none of its own.
        imageText: qsTr("no description")
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
                text: page.preferTextPart ? qsTr("Use HTML part") : qsTr("Use text part")
                onClicked: page.preferTextPart = !page.preferTextPart
            }
            MenuItem {
                visible: formatted.imageCount > 0 && !page.showImages
                text: qsTr("Load images (%1)").arg(formatted.imageCount)
                onClicked: page.showImages = true
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

            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin

                // Not a PageHeader: that aligns right, while the header lines below start left.
                Item {
                    width: 1
                    height: Theme.paddingLarge
                }
                Label {
                    width: parent.width
                    text: message.subject
                    wrapMode: Text.Wrap
                    font.family: Theme.fontFamilyHeading
                    font.pixelSize: Theme.fontSizeLarge
                    color: Theme.highlightColor
                }
                Item {
                    width: 1
                    height: Theme.paddingMedium
                }

                Label {
                    width: parent.width
                    text: message.fromDisplayName.length > 0 ? message.fromDisplayName : message.fromAddress
                    color: Theme.primaryColor
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    visible: message.fromDisplayName.length > 0
                    text: message.fromAddress
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryHighlightColor
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    wrapMode: Text.Wrap
                    text: qsTr("To: %1").arg(message.to.join(", "))
                }
                Label {
                    width: parent.width
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    wrapMode: Text.Wrap
                    visible: message.cc.length > 0
                    text: qsTr("Cc: %1").arg(message.cc.join(", "))
                }
                Label {
                    width: parent.width
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    wrapMode: Text.Wrap
                    text: Format.formatDate(message.date, Formatter.DateFull) + ", "
                          + Format.formatDate(message.date, Formatter.TimeValue)
                }
                Label {
                    width: parent.width
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    wrapMode: Text.Wrap
                    visible: message.numberOfAttachments > 0
                    text: qsTr("%n attachment(s)", "", message.numberOfAttachments)
                }
                Item {
                    width: 1
                    height: Theme.paddingMedium
                }
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
                visible: page.source.length === 0
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: page.showImages && formatted.remoteImageCount > 0
                text: qsTr("Images loaded from the sender's servers: they can see that you read this mail.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }

            TextArea {
                id: body
                width: parent.width
                readOnly: true
                labelVisible: false
                // Silica only selects on press-and-hold in a focused editor, and doesn't focus
                // read-only ones on click. Read-only, focus doesn't bring up the keyboard.
                focusOnClick: true
                text: formatted.text
                Component.onCompleted: _editor.textFormat = TextEdit.RichText

                onClicked: {
                    var position = body.mapToItem(body._editor, mouse.x, mouse.y)
                    var link = body._editor.linkAt(position.x, position.y)
                    if (link.length > 0)
                        pageStack.push(Qt.resolvedUrl("OpenLinkDialog.qml"), { url: link })
                }
            }
        }

        VerticalScrollDecorator { }
    }
}
