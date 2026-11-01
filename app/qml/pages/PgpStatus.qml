// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.6
import Sailfish.Silica 1.0
import Posthoorn 1.0

// What OpenPGP says about a mail, under its header in the reader: who signed it and whether that
// holds, or that it's encrypted, with a button to decrypt it. Nothing for other mail.
Column {
    id: status

    property PgpMessage pgp

    readonly property bool encrypted: pgp.kind === PgpMessage.Encrypted
    readonly property bool hasSignature: pgp.signature !== PgpMessage.NoSignature
    // Whether what it says is a warning.
    readonly property bool warning: pgp.state === PgpMessage.Failed
                                    || pgp.signature === PgpMessage.BadSignature
                                    || pgp.signature === PgpMessage.RevokedKey
                                    || pgp.signature === PgpMessage.SignatureError

    function signatureText() {
        var who = pgp.signer
        switch (pgp.signature) {
        case PgpMessage.GoodSignature:
            return pgp.trusted ? qsTr("Signed by %1").arg(who)
                               : qsTr("Signed by %1, whose key isn't certified").arg(who)
        case PgpMessage.UnknownKey:
            return qsTr("Signed with a key that isn't in your keyring (%1)").arg(who)
        case PgpMessage.ExpiredSignature:
            return qsTr("Signed by %1, with a key that has expired").arg(who)
        case PgpMessage.RevokedKey:
            return qsTr("Signed by %1, with a key that was revoked").arg(who)
        case PgpMessage.BadSignature:
            return qsTr("The signature by %1 doesn't match: the mail was changed after signing").arg(who)
        case PgpMessage.SignatureError:
            return qsTr("The signature couldn't be checked")
        }
        return ""
    }

    function stateText() {
        switch (pgp.state) {
        case PgpMessage.NeedsContent:
            return qsTr("Downloading the mail to check it…")
        case PgpMessage.Working:
            return status.encrypted ? qsTr("Decrypting…") : qsTr("Checking the signature…")
        case PgpMessage.NeedsPassphrase:
            return pgp.error.length > 0 ? pgp.error : qsTr("Encrypted")
        case PgpMessage.Failed:
            return pgp.error
        case PgpMessage.Done:
            if (pgp.error.length > 0)
                return pgp.error
            return status.encrypted ? qsTr("Encrypted; decrypted to read it here") : ""
        }
        return ""
    }

    function askPassphrase() {
        var dialog = pageStack.push(passphraseDialog)
        dialog.accepted.connect(function() { pgp.decrypt(dialog.passphrase) })
    }

    visible: pgp.kind !== PgpMessage.NotPgp
    bottomPadding: Theme.paddingMedium

    Item {
        width: parent.width
        height: Math.max(icon.height, labels.height)

        Icon {
            id: icon
            x: Theme.horizontalPageMargin
            anchors.verticalCenter: parent.verticalCenter
            source: status.encrypted ? "image://theme/icon-s-secure" : "image://theme/icon-s-certificates"
            color: status.warning ? Theme.errorColor : Theme.highlightColor
        }

        Column {
            id: labels
            anchors {
                left: icon.right
                leftMargin: Theme.paddingMedium
                right: parent.right
                rightMargin: Theme.horizontalPageMargin
            }
            Label {
                width: parent.width
                visible: text.length > 0
                text: status.stateText()
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: pgp.state === PgpMessage.Failed || pgp.error.length > 0 ? Theme.errorColor
                                                                               : Theme.highlightColor
            }
            Label {
                width: parent.width
                // Not when the state says why it couldn't be checked.
                visible: status.hasSignature && !(pgp.signature === PgpMessage.SignatureError && pgp.error.length > 0)
                text: status.signatureText()
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: status.warning ? Theme.errorColor : Theme.highlightColor
            }
            Label {
                width: parent.width
                visible: status.hasSignature && pgp.fingerprint.length > 0
                text: pgp.fingerprint.replace(/(.{4})/g, "$1 ").trim()
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeTiny
                font.family: "monospace"
                color: Theme.secondaryColor
            }
        }
    }

    Button {
        visible: pgp.state === PgpMessage.NeedsPassphrase
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("Decrypt")
        onClicked: status.askPassphrase()
    }

    Component {
        id: passphraseDialog

        Dialog {
            property alias passphrase: field.text

            canAccept: field.text.length > 0

            Column {
                width: parent.width

                DialogHeader {
                    acceptText: qsTr("Decrypt")
                }

                PasswordField {
                    id: field
                    width: parent.width
                    label: qsTr("Passphrase of your key")
                    placeholderText: label
                    focus: true
                    EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                    EnterKey.onClicked: if (text.length > 0) accept()
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: qsTr("Posthoorn keeps it for 10 minutes, in memory only, so other encrypted mail opens without asking.")
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryHighlightColor
                }
            }
        }
    }
}
