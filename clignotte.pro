lessThan(QT_MAJOR_VERSION, 6): error("clignotte requires Qt 6. Run qmake6 instead of qmake.")

QT += core sql
QT -= gui

CONFIG += c++17

TARGET = note
CONFIG += console sqlite
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp

HEADERS +=

DISTFILES += \
    pkg/archlinux/PKGBUILD
