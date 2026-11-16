QT += core gui qml quick quickcontrols2 dbus webenginequick webenginecore

CONFIG += c++17 release
TARGET = mdview
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/filepicker.h \
    src/localfiles.h \
    src/portalfilepicker.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/localfiles.cpp \
    src/portalfilepicker.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc

# Mermaid alone is several megabytes; compress the bundled web assets.
QMAKE_RESOURCE_FLAGS += -compress-algo zstd
