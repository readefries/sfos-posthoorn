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
    // The HTML part had nothing to show, so the text part is shown instead. Only once: "Use HTML
    // part" still shows the empty HTML.
    property bool htmlWasEmpty: false
    // Opened from a notification's reply action: 1 to reply, 2 to reply to all, once the text is
    // there to quote.
    property int replyWhenLoaded: 0
    // Read once and then kept here: EmailMessage doesn't tell.
    property bool starred: false
    onLoadedChanged: {
        if (loaded)
            starred = MessageFlags.isStarred(messageId)
    }

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

    Connections {
        target: formatted
        onTextChanged: {
            if (page.useHtml && page.source.length > 0 && formatted.empty && !page.htmlWasEmpty) {
                // In the log, to find mail the reader gets wrong.
                console.log("Message", page.messageId, "has nothing to show in its HTML, using its text part")
                page.htmlWasEmpty = true
                page.preferTextPart = true
                return
            }
            if (page.replyWhenLoaded > 0 && page.source.length > 0) {
                var all = page.replyWhenLoaded === 2
                page.replyWhenLoaded = 0
                app.whenIdle(function() { page.reply(all) })
            }
        }
    }

    Component.onCompleted: {
        if (!message.read)
            app.emailAgent.markMessageAsRead(messageId)
    }

    onStatusChanged: {
        if (status === PageStatus.Active)
            body.forceActiveFocus()
    }

    RemorsePopup {
        id: remorse
    }

    function prefixed(prefix, pattern) {
        return pattern.test(message.subject) ? message.subject : prefix + message.subject
    }

    function dateText() {
        return Format.formatDate(message.date, Formatter.DateMedium) + " "
                + Format.formatDate(message.date, Formatter.TimeValue)
    }

    // A real array: QStringList properties come as sequences, which concat takes as one element.
    function array(list) {
        var result = []
        for (var i = 0; i < list.length; ++i)
            result.push(list[i])
        return result
    }

    // Quotes only the selected text when there is a selection.
    function reply(all) {
        var selection = body.selectedText
        var sender = message.replyTo.length > 0 ? message.replyTo : message.from
        var to = [sender]
        var cc = []
        if (all) {
            var own = array(app.accounts.allEmailAddresses())
            to = array(Addresses.without(to.concat(array(message.to)), own))
            cc = array(Addresses.without(array(message.cc), own.concat(to)))
            // Replying to all on a mail of one's own.
            if (to.length === 0)
                to = [sender]
        }
        pageStack.push(Qt.resolvedUrl("ComposePage.qml"), {
                           from: message.recipientAddress || message.accountAddress,
                           to: to.join(", "),
                           cc: cc.join(", "),
                           //: Prefix for the subject of a reply
                           subject: prefixed(qsTr("Re: "), /^\s*(re|aw|antw|sv|vs):/i),
                           responseType: all ? EmailMessage.ReplyToAll : EmailMessage.Reply,
                           originalMessageId: page.messageId,
                           //: Introduces the quote in a reply. %1 is the date and time, %2 the sender.
                           attribution: qsTr("On %1, %2 wrote:").arg(dateText())
                                        .arg(message.fromDisplayName || message.fromAddress),
                           quote: selection.length > 0 ? formatted.quoteSelection(selection)
                                                       : formatted.quoted(false),
                           quoteWithEarlier: selection.length > 0 ? "" : formatted.quoted(true)
                       })
    }

    function forward() {
        var header = [qsTr("---------- Forwarded message ----------"),
                      qsTr("From: %1").arg(message.from),
                      qsTr("Date: %1").arg(dateText()),
                      qsTr("Subject: %1").arg(message.subject),
                      qsTr("To: %1").arg(message.to.join(", "))]
        if (message.cc.length > 0)
            header.push(qsTr("Cc: %1").arg(message.cc.join(", ")))
        pageStack.push(Qt.resolvedUrl("ComposePage.qml"), {
                           from: message.recipientAddress || message.accountAddress,
                           //: Prefix for the subject of a forwarded mail
                           subject: prefixed(qsTr("Fwd: "), /^\s*(fwd?|wg|doorst):/i),
                           responseType: EmailMessage.Forward,
                           originalMessageId: page.messageId,
                           forwarded: header.join("\n") + "\n\n" + formatted.plainContent()
                       })
    }

    SilicaFlickable {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: actions.top
        }
        clip: true
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
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
                text: qsTr("Move to…")
                onClicked: {
                    var picker = pageStack.push(Qt.resolvedUrl("MoveToPage.qml"), {
                                                    accountId: message.accountId,
                                                    currentFolderId: message.folderId
                                                })
                    picker.folderChosen.connect(function(folderId) {
                        app.emailAgent.moveMessage(page.messageId, folderId)
                        // Back to the list: the mail isn't in this folder any more.
                        app.popFrom(picker, pageStack.previousPage(page))
                    })
                }
            }
            MenuItem {
                text: qsTr("Mark as unread")
                onClicked: {
                    app.emailAgent.markMessageAsUnread(page.messageId)
                    app.popFrom(page)
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
                Item {
                    width: parent.width
                    height: Math.max(subject.height, starButton.height)

                    Label {
                        id: subject
                        width: parent.width - starButton.width - Theme.paddingSmall
                        text: message.subject
                        wrapMode: Text.Wrap
                        font.family: Theme.fontFamilyHeading
                        font.pixelSize: Theme.fontSizeLarge
                        color: Theme.highlightColor
                    }
                    IconButton {
                        id: starButton
                        anchors {
                            right: parent.right
                            // Lined up with the subject's first line, and with the page margin.
                            rightMargin: -Theme.paddingMedium
                            top: parent.top
                            topMargin: -Theme.paddingSmall
                        }
                        enabled: page.loaded
                        icon.source: page.starred ? "image://theme/icon-m-favorite-selected"
                                                  : "image://theme/icon-m-favorite"
                        onClicked: {
                            page.starred = !page.starred
                            app.setStarred(page.messageId, page.starred)
                        }
                    }
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
                Item {
                    width: 1
                    height: Theme.paddingMedium
                }
            }

            Repeater {
                model: message.attachmentModel
                ReceivedAttachmentItem {
                    width: column.width
                    mail: message
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
                visible: page.htmlWasEmpty && !page.useHtml
                text: qsTr("Nothing readable in this mail's HTML, so this is its text part.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
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

    Item {
        id: actions
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: Theme.itemSizeLarge

        PanelBackground {
            anchors.fill: parent
        }

        Row {
            anchors.centerIn: parent
            spacing: Theme.paddingLarge * 2

            IconButton {
                icon.source: "image://theme/icon-m-message-reply"
                enabled: page.loaded
                onClicked: page.reply(false)
            }
            IconButton {
                visible: message.multipleRecipients
                icon.source: "image://theme/icon-m-message-reply-all"
                enabled: page.loaded
                onClicked: page.reply(true)
            }
            IconButton {
                icon.source: "image://theme/icon-m-message-forward"
                enabled: page.loaded
                onClicked: page.forward()
            }
            IconButton {
                visible: app.canArchive(message.accountId, message.folderId)
                icon.source: "image://theme/icon-m-file-archive-folder"
                onClicked: {
                    app.archive(page.messageId)
                    app.popFrom(page)
                }
            }
            IconButton {
                icon.source: "image://theme/icon-m-delete"
                onClicked: remorse.execute(qsTr("Deleting"), function() {
                    app.emailAgent.deleteMessage(page.messageId)
                    app.popFrom(page)
                })
            }
        }
    }
}
