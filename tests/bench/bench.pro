# Times UnifiedMessageListModel on a large folder, in a store of its own (QMF_DATA); not part of
# make check or the RPM. See bench.cpp.
include(../../common.pri)

TEMPLATE = app
TARGET = bench-messagelist
CONFIG += console link_pkgconfig
CONFIG -= app_bundle
QT = core sql
PKGCONFIG += QmfClient

INCLUDEPATH += ../../app/src
HEADERS += ../../app/src/unifiedmessagelistmodel.h
SOURCES += bench.cpp ../../app/src/unifiedmessagelistmodel.cpp
