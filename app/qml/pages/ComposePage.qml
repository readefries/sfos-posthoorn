// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import Sailfish.Contacts 1.0
import Nemo.Email 0.1
import org.nemomobile.contacts 1.0
import Posthoorn 1.0

// Writing a new mail, a reply or a forward, or continuing a draft. Accepting sends it; going
// back keeps it as a draft unless it's discarded (or for a continued draft, deleted) from the
// pulley.
//
// Quotes are sent as '>' lines, which every mail client understands, but shown here the way the
// reader shows them. OutgoingMessage sends the text as format=flowed, so clients that know it
// rewrap the lines to their width.
Dialog {
    id: dialog

    // The address to send from; picks the account.
    property string from
    // Recipients as "Name <address>, …".
    property string to
    property string cc
    property string bcc
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
    // A saved draft to continue: everything above comes from it.
    property int draftId
    // Text to start with, from a mailto: link.
    property string bodyText

    property bool includeQuote: true
    property bool keepEarlierQuotes: false
    property bool writeBelowQuote: false
    property bool showCcBcc: cc.length > 0 || bcc.length > 0
    property bool discarded: false
    // What the mail looked like when the composer opened: leaving it unchanged doesn't save a
    // draft, so neither does an untouched reply.
    property string openedState

    readonly property string currentQuote: keepEarlierQuotes && quoteWithEarlier.length > 0
                                           ? quoteWithEarlier : quote
    readonly property string quoteBlock: forwarded.length > 0 ? forwarded
                                       : includeQuote && currentQuote.length > 0
                                         ? attribution + "\n" + currentQuote : ""

    // Forwarding sends the original's attachments along, except those removed here. They're
    // downloaded first.
    // A draft of a forward has the original's attachments already.
    readonly property bool forwarding: responseType === EmailMessage.Forward && originalMessageId > 0
                                       && draftId === 0
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
        for (var k = 0; k < draftFiles.count; ++k)
            size += draftFiles.get(k).size
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

    // The recipient fields suggest contacts but only keep addresses, so names are looked up again
    // when sending: those of the mail replied to, or else the contact's.
    property var knownNames: ({})
    // Bumped when recipients change: recipientsToString() doesn't notify.
    property int recipientsVersion

    function rememberNames(text) {
        var entries = Addresses.split(text)
        for (var i = 0; i < entries.length; ++i) {
            var open = entries[i].lastIndexOf("<")
            if (open > 0) {
                var name = entries[i].substring(0, open).trim().replace(/^"(.*)"$/, "$1").replace(/\\(.)/g, "$1")
                knownNames[Addresses.address(entries[i])] = name
            }
        }
    }

    function bareAddresses(text) {
        return Addresses.split(text).map(function(entry) { return Addresses.address(entry) })
    }

    function addresses(field) {
        return field.recipientsToString().split(",").map(function(address) {
            return address.trim()
        }).filter(function(address) { return address.length > 0 })
    }

    function recipients(field) {
        return addresses(field).map(function(address) {
            var name = knownNames[address.toLowerCase()]
            if (!name) {
                var person = people.personByEmailAddress(address)
                name = person ? person.displayLabel : ""
            }
            return Addresses.format(name || "", address)
        })
    }

    // What is typed into a recipient field only becomes a recipient when the field is left, and
    // tapping Accept or going back doesn't leave it.
    function takeTypedRecipients() {
        toField.updateSummary()
        ccField.updateSummary()
        bccField.updateSummary()
        recipientsVersion++
    }

    function allValid(field) {
        return addresses(field).every(function(address) { return Addresses.isValid(address) })
    }

    readonly property bool hasRecipients: !toField.empty || !ccField.empty || !bccField.empty
    readonly property bool recipientsValid: {
        recipientsVersion
        return allValid(toField) && allValid(ccField) && allValid(bccField)
    }

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
        message.to = recipients(toField)
        message.cc = recipients(ccField)
        message.bcc = recipients(bccField)
        message.subject = subjectField.text
        message.body = composedBody()
        message.responseType = responseType
        message.originalMessageId = originalMessageId
        var paths = []
        for (var i = 0; i < files.count; ++i)
            paths.push(files.get(i).filePath)
        message.attachments = paths
        message.forwardedParts = forwardedLocations()
        var kept = []
        for (var k = 0; k < draftFiles.count; ++k)
            kept.push(draftFiles.get(k).location)
        message.keptParts = kept
        message.sign = signSwitch.visible && signSwitch.checked
        message.signingKey = signSwitch.key
        message.attachKey = AppState.attachPublicKey
    }

    // Everything that ends up in the mail, to tell whether anything was changed.
    function state() {
        var paths = []
        for (var i = 0; i < files.count; ++i)
            paths.push(files.get(i).filePath)
        var kept = []
        for (var k = 0; k < draftFiles.count; ++k)
            kept.push(draftFiles.get(k).location)
        return JSON.stringify([fromBox.value, recipients(toField), recipients(ccField), recipients(bccField),
                               subjectField.text,
                               composedBody(), paths, kept, forwardedLocations()])
    }

    // Fills the form from the draft, putting its quote back where the composer shows quotes.
    function loadDraft() {
        if (!outgoing.loadDraft(draftId)) {
            Notices.show(qsTr("The draft couldn't be opened"), Notice.Long, Notice.Center)
            return
        }
        from = outgoing.from
        to = outgoing.to.join(", ")
        cc = outgoing.cc.join(", ")
        bcc = outgoing.bcc.join(", ")
        subject = outgoing.subject
        responseType = outgoing.responseType
        originalMessageId = outgoing.originalMessageId
        var parts = outgoing.draftParts()
        body.text = parts.text
        attribution = parts.attribution
        quote = parts.quote
        writeBelowQuote = parts.writtenBelowQuote
        var attachments = outgoing.draftAttachments()
        for (var i = 0; i < attachments.length; ++i)
            draftFiles.append(attachments[i])
    }

    onAcceptBlocked: {
        takeTypedRecipients()
        if (canAccept)
            accept()
    }
    onAcceptPendingChanged: {
        if (acceptPending)
            takeTypedRecipients()
    }

    // Sent after a few seconds, which can be undone; the app does that, as this page is gone by then.
    onAccepted: {
        var message = app.createOutgoing()
        if (draftId > 0)
            message.loadDraft(draftId)
        fill(message)
        app.sendLater(message, pageStack.currentPage === dialog ? pageStack.previousPage(dialog)
                                                               : pageStack.currentPage)
    }

    onRejected: {
        takeTypedRecipients()
        if (discarded || state() === openedState)
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
        if (draftId > 0)
            loadDraft()
        var index = identities.indexFromAddress(from)
        if (index >= 0)
            fromBox.currentIndex = index
        rememberNames(to)
        rememberNames(cc)
        rememberNames(bcc)
        toField.setEmailRecipients(bareAddresses(to))
        ccField.setEmailRecipients(bareAddresses(cc))
        bccField.setEmailRecipients(bareAddresses(bcc))
        if (to.length === 0)
            toField.forceActiveFocus()
        else
            body.forceActiveFocus()
        // Into the mail store only, unlike downloading an attachment, which saves it in Downloads
        // too.
        if (forwarding && !forwardReady)
            original.downloadMessage()
        openedState = state()
    }

    // For the recipient fields' suggestions.
    PeopleModel {
        id: people
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

    // The continued draft's own attachments: location, name, size.
    ListModel {
        id: draftFiles
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

    RemorsePopup {
        id: remorse
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
                text: dialog.draftId > 0 ? qsTr("Delete draft") : qsTr("Discard")
                onClicked: {
                    if (dialog.draftId === 0) {
                        dialog.discarded = true
                        app.popFrom(dialog)
                        return
                    }
                    remorse.execute(qsTr("Deleting draft"), function() {
                        dialog.discarded = true
                        app.emailAgent.deleteMessage(dialog.draftId)
                        app.popFrom(dialog)
                    })
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

            RecipientField {
                id: toField
                placeholderText: qsTr("To")
                summaryPlaceholderText: qsTr("To")
                contactSearchModel: people
                showLabel: true
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhEmailCharactersOnly
                onSelectionChanged: dialog.recipientsVersion++
                onLastFieldExited: (dialog.showCcBcc ? ccField : subjectField).forceActiveFocus()
            }
            RecipientField {
                id: ccField
                visible: dialog.showCcBcc
                placeholderText: qsTr("Cc")
                summaryPlaceholderText: qsTr("Cc")
                contactSearchModel: people
                showLabel: true
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhEmailCharactersOnly
                onSelectionChanged: dialog.recipientsVersion++
                onLastFieldExited: bccField.forceActiveFocus()
            }
            RecipientField {
                id: bccField
                visible: dialog.showCcBcc
                placeholderText: qsTr("Bcc")
                summaryPlaceholderText: qsTr("Bcc")
                contactSearchModel: people
                showLabel: true
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhEmailCharactersOnly
                onSelectionChanged: dialog.recipientsVersion++
                onLastFieldExited: subjectField.forceActiveFocus()
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !dialog.recipientsValid
                text: qsTr("Not every recipient is a valid address.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.errorColor
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

            TextSwitch {
                id: signSwitch
                // The key for the From address.
                readonly property string key: {
                    app.pgpKeys.keys
                    return app.pgpKeys.signingKeyFor(fromBox.value)
                }
                visible: key.length > 0
                text: qsTr("Sign")
                description: qsTr("With your PGP key, so recipients can tell it's from you, unchanged")
                checked: AppState.signByDefault
            }

            SectionHeader {
                visible: files.count > 0 || draftFiles.count > 0
                         || (dialog.forwarding && original.attachmentModel.count > dialog.leftOut.length)
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
                model: draftFiles
                AttachmentItem {
                    name: model.name
                    size: model.size
                    onRemoveRequested: draftFiles.remove(index)
                }
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
                text: dialog.bodyText
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
