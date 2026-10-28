include(../common.pri)

TEMPLATE = lib
TARGET = posthoorncore
CONFIG += staticlib
QT = core

HEADERS += \
    flowedtext.h \
    mailaddresses.h \
    mailcontent.h
SOURCES += \
    flowedtext.cpp \
    mailaddresses.cpp \
    mailcontent.cpp
