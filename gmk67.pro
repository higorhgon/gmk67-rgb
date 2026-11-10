QT += core gui dbus qml quick quickcontrols2 concurrent

CONFIG += c++20 release
TARGET = gmk67
TEMPLATE = app
VERSION = 2.0.0
DEFINES += GMK67_VERSION=\\\"$$VERSION\\\"

HEADERS += \
    src/animations.h \
    src/backend.h \
    src/cli.h \
    src/keyboard.h \
    src/protocol.h \
    src/streamer.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/animations.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/keyboard.cpp \
    src/protocol.cpp \
    src/streamer.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
