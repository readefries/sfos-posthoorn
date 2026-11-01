// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1

// An attachment of a received mail, a row of EmailMessage.attachmentModel. Tapping opens it,
// downloading it first if needed. The email plugin saves attachments in Downloads, where the
// apps that open them can read them.
ListItem {
    id: item

    // The mail the attachment belongs to. Not called message: "message: message" where it's
    // used would bind the property to itself.
    property EmailMessage mail
    // Open once the download is done.
    property bool openWhenSaved: false
    // Import into the PGP keyring once the download is done.
    property bool importWhenSaved: false

    // A PGP key someone sent along.
    readonly property bool isKey: model.mimeType.toLowerCase() === "application/pgp-keys"
                                  || (/\.(asc|key|pgp)$/i.test(model.displayName)
                                      && model.mimeType.toLowerCase() !== "application/pgp-signature")
    // PGP/MIME's own parts: the signature and the encrypted mail, which are no files for the user.
    readonly property bool pgpPart: /^application\/pgp-(signature|encrypted)$/.test(model.mimeType.toLowerCase())
                                    || (model.mimeType.toLowerCase() === "application/octet-stream"
                                        && model.displayName === "encrypted.asc")

    function importKey() {
        importWhenSaved = false
        app.pgpKeys.importFile(decodeURIComponent(model.url.toString().replace(/^file:\/\//, "")))
    }

    readonly property bool saved: model.url.length > 0
    readonly property bool downloading: model.statusInfo === EmailAgent.Queued
                                        || model.statusInfo === EmailAgent.Downloading

    function download(open) {
        openWhenSaved = open
        // True when it's in Downloads already; otherwise the url follows once it is.
        if (mail.downloadAttachment(model.contentLocation) && open)
            openFile()
    }

    function openFile() {
        openWhenSaved = false
        if (!Qt.openUrlExternally(model.url))
            Notices.show(qsTr("No app opens %1").arg(model.displayName), Notice.Long, Notice.Center)
    }

    function iconName(mimeType) {
        var variant = Theme.colorScheme === Theme.LightOnDark ? "-light" : "-dark"
        var type = mimeType.toLowerCase()
        if (type.indexOf("image/") === 0)
            return "icon-m-file-image"
        if (type.indexOf("audio/") === 0)
            return "icon-m-file-audio"
        if (type.indexOf("video/") === 0)
            return "icon-m-file-video"
        if (type === "message/rfc822")
            return "icon-m-mail"
        if (type === "application/pdf")
            return "icon-m-file-pdf" + variant
        if (/vcard/.test(type))
            return "icon-m-file-vcard"
        if (/zip|compressed|x-tar|x-7z|x-rar|gzip/.test(type))
            return "icon-m-file-compressed"
        if (/spreadsheet|excel|csv/.test(type))
            return "icon-m-file-spreadsheet" + variant
        if (/presentation|powerpoint/.test(type))
            return "icon-m-file-presentation" + variant
        if (/msword|wordprocessing|opendocument\.text|rtf/.test(type))
            return "icon-m-file-formatted" + variant
        if (type.indexOf("text/") === 0)
            return "icon-m-file-document" + variant
        return "icon-m-file-other" + variant
    }

    onSavedChanged: {
        if (saved && openWhenSaved)
            openFile()
        if (saved && importWhenSaved)
            importKey()
    }

    visible: !pgpPart

    contentHeight: pgpPart ? 0 : Theme.itemSizeMedium
    onClicked: {
        if (saved)
            openFile()
        else if (!downloading)
            download(true)
    }

    menu: ContextMenu {
        MenuItem {
            text: qsTr("Open")
            onClicked: item.saved ? item.openFile() : item.download(true)
        }
        MenuItem {
            visible: item.isKey
            text: qsTr("Import key")
            onClicked: {
                if (item.saved) {
                    item.importKey()
                } else {
                    item.importWhenSaved = true
                    item.download(false)
                }
            }
        }
        MenuItem {
            visible: !item.saved
            text: qsTr("Save to Downloads")
            onClicked: item.download(false)
        }
        MenuItem {
            visible: item.downloading
            text: qsTr("Stop downloading")
            onClicked: {
                item.openWhenSaved = false
                item.mail.cancelAttachmentDownload(model.contentLocation)
            }
        }
    }

    Icon {
        id: icon
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        source: "image://theme/" + item.iconName(model.mimeType)
    }

    Column {
        anchors {
            left: icon.right
            right: parent.right
            leftMargin: Theme.paddingMedium
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        Label {
            width: parent.width
            text: model.displayName
            truncationMode: TruncationMode.Fade
            color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
        }
        Label {
            width: parent.width
            text: {
                var size = Format.formatFileSize(model.size)
                if (item.downloading)
                    return qsTr("%1, downloading %2%").arg(size).arg(Math.round(model.progressInfo * 100))
                if (model.statusInfo === EmailAgent.Failed || model.statusInfo === EmailAgent.FailedToSave)
                    return qsTr("%1, download failed").arg(size)
                if (item.saved)
                    return qsTr("%1, in Downloads").arg(size)
                return size
            }
            font.pixelSize: Theme.fontSizeExtraSmall
            color: model.statusInfo === EmailAgent.Failed || model.statusInfo === EmailAgent.FailedToSave
                   ? Theme.errorColor : Theme.secondaryColor
            truncationMode: TruncationMode.Fade
        }
    }
}
