QT += core gui dbus quick quickcontrols2 testlib concurrent
CONFIG += c++20 testcase
TARGET = gmk67_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/animations.h ../src/backend.h ../src/keyboard.h ../src/protocol.h \
    ../src/streamer.h ../src/theme.h ../src/translator.h
SOURCES += gmk67_tests.cpp ../src/animations.cpp ../src/backend.cpp ../src/keyboard.cpp \
    ../src/protocol.cpp ../src/streamer.cpp ../src/theme.cpp ../src/translator.cpp
