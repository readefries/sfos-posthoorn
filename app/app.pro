include(../common.pri)

TARGET = posthoorn
CONFIG += sailfishapp
# Building outgoing mail; the rest goes through the Nemo.Email QML plugin.
PKGCONFIG += QmfClient

include(../core/core.pri)

HEADERS += \
    src/addresses.h \
    src/appstate.h \
    src/messagebody.h \
    src/outgoingmessage.h
SOURCES += \
    src/appstate.cpp \
    src/main.cpp \
    src/messagebody.cpp \
    src/outgoingmessage.cpp

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

DISTFILES += \
    posthoorn.desktop \
    qml/posthoorn.qml \
    qml/cover/CoverPage.qml \
    qml/pages/AccountsPage.qml \
    qml/pages/AttachmentItem.qml \
    qml/pages/ComposePage.qml \
    qml/pages/FoldersPage.qml \
    qml/pages/MessagesPage.qml \
    qml/pages/MessagePage.qml \
    qml/pages/MoveToPage.qml \
    qml/pages/OpenLinkDialog.qml
