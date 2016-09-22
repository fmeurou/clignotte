QT += core sql gui

CONFIG += c++11

TARGET = clignotte_cli
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

LIBS += -lclignotte

SOURCES += main.cpp

HEADERS +=

DISTFILES += \
    pkg/archlinux/PKGBUILD
