include(../common.pri)

TEMPLATE = lib
TARGET = posthoorncore
CONFIG += staticlib
QT = core

HEADERS += \
    flowedtext.h \
    linkcleaner.h \
    mailaddresses.h \
    mailcontent.h \
    maildraft.h
SOURCES += \
    flowedtext.cpp \
    linkcleaner.cpp \
    mailaddresses.cpp \
    mailcontent.cpp \
    maildraft.cpp
