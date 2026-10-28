# Include from projects that link against the static posthoorncore library.
INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
POSTHOORNCORE_BUILD_DIR = $$shadowed($$PWD)
LIBS += -L$$POSTHOORNCORE_BUILD_DIR -lposthoorncore
PRE_TARGETDEPS += $$POSTHOORNCORE_BUILD_DIR/libposthoorncore.a
