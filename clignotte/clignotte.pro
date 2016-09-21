#-------------------------------------------------
#
# Project created by QtCreator 2016-09-21T18:44:42
#
#-------------------------------------------------

QT       += sql

QT       -= gui

TARGET = clignotte
TEMPLATE = lib

DEFINES += CLIGNOTTE_LIBRARY

SOURCES += clignotte.cpp

HEADERS += clignotte.h\
        clignotte_global.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}
