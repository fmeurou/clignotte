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

SOURCES += clignotte.cpp \
            encrypt/sha256.cpp \
    encrypt/sha384.cpp \
    encrypt/sha512.cpp

HEADERS += clignotte.h\
        clignotte_global.h \
        encrypt/sha256.h \
    encrypt/sha384.h \
    encrypt/sha512.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}

DISTFILES += \
    encrypt/licence.txt
