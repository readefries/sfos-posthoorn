# posthoorn-notify-filter: a plugin for QMF's messageserver that keeps new-mail notifications
# quiet for the accounts switched off in Posthoorn's Settings. See mutedaccountsplugin.h.
include(../common.pri)

TEMPLATE = lib
TARGET = posthoornmutedaccounts
CONFIG += plugin hide_symbols link_pkgconfig
QT -= gui
PKGCONFIG += QmfClient QmfMessageServer

HEADERS += mutedaccountsplugin.h
SOURCES += mutedaccountsplugin.cpp

target.path = $$[QT_INSTALL_PLUGINS]/messagingframework/messageserverplugins
INSTALLS += target
