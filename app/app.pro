include(../common.pri)

TARGET = posthoorn
CONFIG += sailfishapp
# Answering mail notifications and links, see MailUiService.
QT += dbus
# Building outgoing mail; the rest goes through the Nemo.Email QML plugin.
PKGCONFIG += QmfClient QmfMessageServer

include(../core/core.pri)

HEADERS += \
    src/addresses.h \
    src/appstate.h \
    src/archivefolders.h \
    src/gpgjob.h \
    src/imappartfetcher.h \
    src/links.h \
    src/mailuiservice.h \
    src/messagebody.h \
    src/messageflags.h \
    src/pgpkeys.h \
    src/pgpmessage.h \
    src/outgoingmessage.h \
    src/snoozes.h \
    src/unifiedmessagelistmodel.h
SOURCES += \
    src/appstate.cpp \
    src/archivefolders.cpp \
    src/gpgjob.cpp \
    src/imappartfetcher.cpp \
    src/mailuiservice.cpp \
    src/main.cpp \
    src/messagebody.cpp \
    src/outgoingmessage.cpp \
    src/pgpkeys.cpp \
    src/pgpmessage.cpp \
    src/snoozes.cpp \
    src/unifiedmessagelistmodel.cpp

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

# For the posthoorn-default-mail package: in place of Jolla's Mail, mail notifications and mailto:
# links open Posthoorn (see MailUiService).
defaultmail_service.files = defaultmail/com.jolla.email.ui.service
defaultmail_service.path = /usr/share/dbus-1/services
defaultmail_profile.files = defaultmail/posthoorn.profile
defaultmail_profile.path = /etc/sailjail/permissions
defaultmail_mailto.files = defaultmail/posthoorn-mailto.desktop
defaultmail_mailto.path = /usr/share/applications
INSTALLS += defaultmail_service defaultmail_profile defaultmail_mailto

DISTFILES += \
    posthoorn.desktop \
    qml/posthoorn.qml \
    qml/cover/CoverPage.qml \
    qml/pages/AccountsPage.qml \
    qml/pages/AttachmentItem.qml \
    qml/pages/ComposePage.qml \
    qml/pages/FolderItem.qml \
    qml/pages/KeysPage.qml \
    qml/pages/FoldersPage.qml \
    qml/pages/MessagesPage.qml \
    qml/pages/MessagePage.qml \
    qml/pages/MoveToPage.qml \
    qml/pages/OpenLinkDialog.qml \
    qml/pages/PgpStatus.qml \
    qml/pages/ReceivedAttachmentItem.qml \
    qml/pages/SettingsPage.qml \
    qml/pages/SnoozePage.qml \
    qml/pages/VirtualFolderItem.qml \
    qml/pages/UnifiedFoldersPage.qml
