// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1
import Posthoorn 1.0

// Writing a new mail, a reply or a forward. Accepting sends it; going back keeps it as a draft
// unless it's discarded from the pulley.
//
// Quotes are sent as '>' lines, which every mail client understands, but shown here the way the
// reader shows them. The plugin sends plain text without format=flowed, so paragraphs go out as
// single lines for the receiving client to wrap.
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
    // Forward: the original has attachments, which aren't sent along yet.
    property bool originalHasAttachments: false

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

    canAccept: hasRecipients && recipientsValid

    function composedBody() {
        var own = body.text.replace(/\s+$/, "")
        if (signature.length > 0)
            own += (own.length > 0 ? "\n\n" : "") + "-- \n" + signature
        if (quoteBlock.length === 0)
            return own
        return writeBelowQuote ? quoteBlock + "\n\n" + own : own + "\n\n" + quoteBlock
    }

    function fill(message) {
        // First: it picks the account the rest belongs to.
        message.from = fromBox.value
        message.to = Addresses.split(toField.text)
        message.cc = Addresses.split(ccField.text)
        message.bcc = Addresses.split(bccField.text)
        message.subject = subjectField.text
        message.body = composedBody()
        message.responseType = responseType
        if (originalMessageId > 0)
            message.originalMessageId = originalMessageId
    }

    onAccepted: {
        fill(outgoing)
        outgoing.send()
    }

    onRejected: {
        if (discarded || body.text.trim().length === 0)
            return
        fill(outgoing)
        outgoing.saveDraft()
        Notices.show(qsTr("Saved to Drafts"), Notice.Short, Notice.Center)
    }

    Component.onCompleted: {
        var index = identities.indexFromAddress(from)
        if (index >= 0)
            fromBox.currentIndex = index
        if (toField.text.length === 0)
            toField.forceActiveFocus()
        else
            body.forceActiveFocus()
    }

    EmailMessage {
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

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: dialog.originalHasAttachments
                text: qsTr("The original's attachments aren't sent along yet.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }
        }

        VerticalScrollDecorator { }
    }
}
