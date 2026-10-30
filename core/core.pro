include(../common.pri)

TEMPLATE = lib
TARGET = posthoorncore
CONFIG += staticlib
QT = core

HEADERS += \
    archivefolder.h \
    flowedtext.h \
    linkcleaner.h \
    mailaddresses.h \
    mailcontent.h \
    maildraft.h
SOURCES += \
    archivefolder.cpp \
    flowedtext.cpp \
    linkcleaner.cpp \
    mailaddresses.cpp \
    mailcontent.cpp \
    maildraft.cpp
