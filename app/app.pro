include(../common.pri)

TARGET = posthoorn
CONFIG += sailfishapp

include(../core/core.pri)

HEADERS += src/messagebody.h
SOURCES += \
    src/main.cpp \
    src/messagebody.cpp

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

DISTFILES += \
    posthoorn.desktop \
    qml/posthoorn.qml \
    qml/cover/CoverPage.qml \
    qml/pages/AccountsPage.qml \
    qml/pages/FoldersPage.qml \
    qml/pages/MessagesPage.qml \
    qml/pages/MessagePage.qml \
    qml/pages/MoveToPage.qml \
    qml/pages/OpenLinkDialog.qml \
    qml/pages/SelectionPage.qml
