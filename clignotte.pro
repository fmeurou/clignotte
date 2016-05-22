QT += core sql
QT -= gui

CONFIG += c++11

TARGET = note
CONFIG += console sqlite
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    note.cpp

HEADERS += \
    note.h
