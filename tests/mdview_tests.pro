QT += core gui quick quickcontrols2 testlib dbus webenginecore
CONFIG += c++17 testcase
TARGET = mdview_tests
TEMPLATE = app

INCLUDEPATH += ../src
DEFINES += SRCDIR=\\\"$$PWD\\\"

HEADERS += ../src/backend.h ../src/filepicker.h ../src/localfiles.h ../src/theme.h ../src/translator.h
SOURCES += mdview_tests.cpp ../src/backend.cpp ../src/localfiles.cpp ../src/theme.cpp ../src/translator.cpp
