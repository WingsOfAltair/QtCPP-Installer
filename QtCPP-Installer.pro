QT       += core gui widgets concurrent

CONFIG   += qt gui c++17
CONFIG  -= console
CONFIG  -= app_bundle

TEMPLATE = app
TARGET = QtCPP-Installer

# ---- Source and Header Files ----
SOURCES += \
    downloadmanager.cpp \
    main.cpp \
    mainwindow.cpp \
    segmentdownloader.cpp

HEADERS += \
    downloadmanager.h \
    mainwindow.h \
    segmentdownloader.h \
    utils.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    resources/resources.qrc

# ---- Include Paths ----
INCLUDEPATH += D:/GitHub/bit7z/include

# ---- Libraries ----
LIBS += -LD:/GitHub/bit7z/lib/x64/Debug -lbit7z -loleaut32

# ---- Windows Target ----
DEFINES += _WIN32_WINNT=0x0601

# ---- MSVC-specific Compiler Flags ----
QMAKE_CXXFLAGS += /W4
QT += core network