// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import Nemo.Email 0.1
import Posthoorn 1.0

// Writing a new mail, a reply or a forward. Accepting sends it; going back keeps it as a draft
// unless it's discarded from the pulley.
//
// Quotes are sent as '>' lines, which every mail client understands, but shown here the way the
// reader shows them. OutgoingMessage sends the text as format=flowed, so clients that know it
// rewrap the lines to their width.
Dialog {
    id: dialog

    // The address to send from; picks the account.
    property string from
    property string to
    property string cc
    property string subject
    property int responseType: EmailMessage.NoResponse
    property int originalMessageId
    // Reply: the line introducing the quote, the quoted original, and the same including what
    // the original quoted itself (empty when that's no different).
    property string attribution
    property string quote
    property string quoteWithEarlier
    // Forward: the original with a short header, not quoted.
    property string forwarded

    property bool includeQuote: true
    property bool keepEarlierQuotes: false
    property bool writeBelowQuote: false
    property bool showCcBcc: cc.length > 0
    property bool discarded: false

    readonly property string currentQuote: keepEarlierQuotes && quoteWithEarlier.length > 0
                                           ? quoteWithEarlier : quote
    readonly property string quoteBlock: forwarded.length > 0 ? forwarded
                                       : includeQuote && currentQuote.length > 0
                                         ? attribution + "\n" + currentQuote : ""

    // Forwarding sends the original's attachments along, except those removed here. They're
    // downloaded first.
    readonly property bool forwarding: responseType === EmailMessage.Forward && originalMessageId > 0
    property var leftOut: []
    // Bumped when the original's attachments may have changed: the model's methods don't notify.
    property int attachmentsVersion

    function forwardedLocations() {
        var locations = []
        var model = original.attachmentModel
        for (var i = 0; forwarding && i < model.count; ++i) {
            var location = model.location(i)
            if (leftOut.indexOf(location) < 0)
                locations.push(location)
        }
        return locations
    }

    readonly property bool forwardReady: {
        attachmentsVersion
        var model = original.attachmentModel
        for (var i = 0; forwarding && i < model.count; ++i) {
            if (leftOut.indexOf(model.location(i)) < 0 && !model.isDownloaded(i))
                return false
        }
        return true
    }

    readonly property real totalSize: {
        attachmentsVersion
        var size = 0
        for (var i = 0; i < files.count; ++i)
            size += files.get(i).fileSize
        var model = original.attachmentModel
        for (var j = 0; forwarding && j < model.count; ++j) {
            if (leftOut.indexOf(model.location(j)) < 0)
                size += model.size(j)
        }
        return size
    }

    function hasFile(path) {
        for (var i = 0; i < files.count; ++i) {
            if (files.get(i).filePath === path)
                return true
        }
        return false
    }

    readonly property int accountId: identities.accountId(fromBox.currentIndex)
    readonly property string signature: accountId > 0 && app.accounts.appendSignature(accountId)
                                        ? app.accounts.signature(accountId) : ""

    function allValid(text) {
        var entries = Addresses.split(text)
        for (var i = 0; i < entries.length; ++i) {
            if (!Addresses.isValid(entries[i]))
                return false
        }
        return true
    }

    readonly property bool hasRecipients: Addresses.split(toField.text).length > 0
                                          || Addresses.split(ccField.text).length > 0
                                          || Addresses.split(bccField.text).length > 0
    readonly property bool recipientsValid: allValid(toField.text) && allValid(ccField.text)
                                            && allValid(bccField.text)

    canAccept: hasRecipients && recipientsValid && forwardReady

    function composedBody() {
        var own = body.text.replace(/\s+$/, "")
        if (signature.length > 0)
            own += (own.length > 0 ? "\n\n" : "") + "-- \n" + signature
        if (quoteBlock.length === 0)
            return own
        return writeBelowQuote ? quoteBlock + "\n\n" + own : own + "\n\n" + quoteBlock
    }

    function fill(message) {
        message.from = fromBox.value
        message.to = Addresses.split(toField.text)
        message.cc = Addresses.split(ccField.text)
        message.bcc = Addresses.split(bccField.text)
        message.subject = subjectField.text
        message.body = composedBody()
        message.responseType = responseType
        message.originalMessageId = originalMessageId
        var paths = []
        for (var i = 0; i < files.count; ++i)
            paths.push(files.get(i).filePath)
        message.attachments = paths
        message.forwardedParts = forwardedLocations()
    }

    onAccepted: {
        fill(outgoing)
        var accountId = outgoing.send()
        if (accountId > 0)
            app.emailAgent.processSendingQueue(accountId)
        else
            Notices.show(qsTr("The mail couldn't be put in the Outbox: %1").arg(outgoing.error),
                         Notice.Long, Notice.Center)
    }

    onRejected: {
        if (discarded || (body.text.trim().length === 0 && files.count === 0))
            return
        fill(outgoing)
        var accountId = outgoing.saveDraft()
        if (accountId > 0) {
            app.emailAgent.exportUpdates(accountId)
            Notices.show(qsTr("Saved to Drafts"), Notice.Short, Notice.Center)
        } else {
            Notices.show(qsTr("The draft couldn't be saved: %1").arg(outgoing.error),
                         Notice.Long, Notice.Center)
        }
    }

    Component.onCompleted: {
        var index = identities.indexFromAddress(from)
        if (index >= 0)
            fromBox.currentIndex = index
        if (toField.text.length === 0)
            toField.forceActiveFocus()
        else
            body.forceActiveFocus()
        // Into the mail store only, unlike downloading an attachment, which saves it in Downloads
        // too.
        if (forwarding && !forwardReady)
            original.downloadMessage()
    }

    EmailMessage {
        id: original
        messageId: dialog.forwarding ? dialog.originalMessageId : 0
        onMessageDownloaded: dialog.attachmentsVersion++
        onAttachmentsChanged: dialog.attachmentsVersion++
        onStoredMessageChanged: dialog.attachmentsVersion++
        onMessageDownloadFailed: Notices.show(qsTr("The original's attachments couldn't be downloaded"),
                                              Notice.Long, Notice.Center)
    }

    // Files to attach: fileName, filePath, fileSize.
    ListModel {
        id: files
    }

    Component {
        id: filePicker
        MultiContentPickerDialog {
            title: qsTr("Attach")
            // Set once the picker is accepted.
            onSelectedContentChanged: {
                for (var i = 0; i < selectedContent.count; ++i) {
                    var item = selectedContent.get(i)
                    if (!dialog.hasFile(item.filePath)) {
                        files.append({ fileName: item.fileName, filePath: item.filePath,
                                       fileSize: item.fileSize })
                    }
                }
            }
        }
    }

    OutgoingMessage {
        id: outgoing
    }

    EmailTransmitAddressListModel {
        id: identities
    }

    MessageBody {
        id: formattedQuote
        plainText: dialog.quoteBlock
        textColor: Theme.secondaryColor
        linkColor: Theme.secondaryHighlightColor
        secondaryColor: Theme.secondaryColor
        quoteColors: [Theme.secondaryHighlightColor, Theme.secondaryColor]
        paragraphSpacing: Theme.paddingSmall
        indent: Theme.paddingLarge
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Discard")
                onClicked: {
                    dialog.discarded = true
                    pageStack.pop()
                }
            }
            MenuItem {
                visible: dialog.quote.length > 0
                text: dialog.includeQuote ? qsTr("Leave out the quote") : qsTr("Include the quote")
                onClicked: dialog.includeQuote = !dialog.includeQuote
            }
            MenuItem {
                visible: dialog.includeQuote && dialog.quoteWithEarlier.length > 0
                         && dialog.quoteWithEarlier !== dialog.quote
                text: dialog.keepEarlierQuotes ? qsTr("Leave out earlier quotes")
                                               : qsTr("Include earlier quotes")
                onClicked: dialog.keepEarlierQuotes = !dialog.keepEarlierQuotes
            }
            MenuItem {
                visible: dialog.quoteBlock.length > 0
                text: dialog.writeBelowQuote ? qsTr("Write above the quote") : qsTr("Write below the quote")
                onClicked: dialog.writeBelowQuote = !dialog.writeBelowQuote
            }
            MenuItem {
                text: qsTr("Add attachment")
                onClicked: pageStack.push(filePicker)
            }
            MenuItem {
                visible: !dialog.showCcBcc
                text: qsTr("Add Cc and Bcc")
                onClicked: {
                    dialog.showCcBcc = true
                    ccField.forceActiveFocus()
                }
            }
        }

        Column {
            id: column
            width: parent.width

            DialogHeader {
                acceptText: qsTr("Send")
                cancelText: qsTr("Save")
            }

            ComboBox {
                id: fromBox
                label: qsTr("From")
                enabled: identities.numberOfAddresses > 1
                menu: ContextMenu {
                    Repeater {
                        model: identities
                        MenuItem {
                            text: model.emailAddress
                        }
                    }
                }
            }

            TextField {
                id: toField
                width: parent.width
                label: qsTr("To")
                placeholderText: label
                text: dialog.to
                inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                errorHighlight: !dialog.allValid(text)
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: (dialog.showCcBcc ? ccField : subjectField).forceActiveFocus()
            }
            TextField {
                id: ccField
                visible: dialog.showCcBcc
                width: parent.width
                label: qsTr("Cc")
                placeholderText: label
                text: dialog.cc
                inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                errorHighlight: !dialog.allValid(text)
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: bccField.forceActiveFocus()
            }
            TextField {
                id: bccField
                visible: dialog.showCcBcc
                width: parent.width
                label: qsTr("Bcc")
                placeholderText: label
                inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                errorHighlight: !dialog.allValid(text)
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: subjectField.forceActiveFocus()
            }
            TextField {
                id: subjectField
                width: parent.width
                label: qsTr("Subject")
                placeholderText: label
                text: dialog.subject
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: body.forceActiveFocus()
            }

            SectionHeader {
                visible: files.count > 0 || (dialog.forwarding && original.attachmentModel.count > dialog.leftOut.length)
                text: qsTr("Attachments, %1").arg(Format.formatFileSize(dialog.totalSize))
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: dialog.totalSize > 20 * 1024 * 1024
                text: qsTr("Mail servers often refuse mail this large.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }
            Repeater {
                model: files
                AttachmentItem {
                    name: model.fileName
                    size: model.fileSize
                    onRemoveRequested: files.remove(index)
                }
            }
            Repeater {
                model: dialog.forwarding ? original.attachmentModel : null
                AttachmentItem {
                    visible: dialog.leftOut.indexOf(model.contentLocation) < 0
                    name: model.displayName
                    size: model.size
                    status: model.downloaded ? "" : qsTr("Downloading…")
                    onRemoveRequested: {
                        var leftOut = dialog.leftOut.slice()
                        leftOut.push(model.contentLocation)
                        dialog.leftOut = leftOut
                    }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: dialog.writeBelowQuote && dialog.quoteBlock.length > 0
                wrapMode: Text.Wrap
                textFormat: Text.RichText
                text: formattedQuote.text
            }

            TextArea {
                id: body
                width: parent.width
                placeholderText: qsTr("Message")
                labelVisible: false
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: dialog.signature.length > 0
                text: "-- \n" + dialog.signature
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
            }

            Item {
                width: 1
                height: Theme.paddingLarge
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !dialog.writeBelowQuote && dialog.quoteBlock.length > 0
                wrapMode: Text.Wrap
                textFormat: Text.RichText
                text: formattedQuote.text
            }
        }

        VerticalScrollDecorator { }
    }
}
