include(../../common.pri)
include(../../core/core.pri)

TEMPLATE = app
TARGET = tst_mailcontent
CONFIG += testcase no_testcase_installs console
CONFIG -= app_bundle
QT = core testlib

SOURCES += tst_mailcontent.cpp
