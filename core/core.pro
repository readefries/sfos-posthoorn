include(../common.pri)

TEMPLATE = lib
TARGET = posthoorncore
CONFIG += staticlib
QT = core

HEADERS += \
    mailaddresses.h \
    mailcontent.h
SOURCES += \
    mailaddresses.cpp \
    mailcontent.cpp
