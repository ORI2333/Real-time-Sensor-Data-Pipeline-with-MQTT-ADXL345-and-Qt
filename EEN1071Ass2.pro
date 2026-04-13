#-------------------------------------------------------------------------
# EEN1071 Assignment 2 - Project Configuration File
# This file defines the modules, source files, and libraries used by qmake.
#-------------------------------------------------------------------------

# Add essential Qt modules: core logic and GUI elements
QT       += core gui widgets printsupport network

# Ensure compatibility with newer Qt versions by explicitly adding the widgets module
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Set the C++ standard to C++17 for modern features and syntax
CONFIG += c++17
CONFIG += utf8_source

# Keep source and runtime narrow strings in UTF-8 on MinGW to avoid Chinese mojibake.
win32-g++ {
    QMAKE_CFLAGS   += -finput-charset=UTF-8 -fexec-charset=UTF-8
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}

# Uncommenting the line below helps maintain code quality by preventing
# the use of outdated (deprecated) Qt functions.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

# List of all C++ source files to be compiled into the application
SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/qcustomplot.cpp

# List of header files containing class definitions and declarations
HEADERS += \
    include/mainwindow.h \
    include/qcustomplot.h

# The XML-based UI file designed in Qt Designer
FORMS += \
    ui/mainwindow.ui

INCLUDEPATH += \
    include \
    src \
    ui

win32 {
    RC_ICONS = assets/app_icon.ico
}

# External Library Linking: Links the Paho MQTT C client library
# (Required for IoT/Messaging functionality in this assignment)
LIBS  += -lpaho-mqtt3c

# Deployment rules: Defines where the binary is installed on different OS targets
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    README.md \
    assets/app_icon.ico \
    assets/README.md
