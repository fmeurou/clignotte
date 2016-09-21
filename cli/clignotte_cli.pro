QT += core sql
QT -= gui

CONFIG += c++11

TARGET = clignotte_cli
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

LIB += -lclignotte

SOURCES += main.cpp

HEADERS +=

DISTFILES += \
    pkg/archlinux/PKGBUILD
