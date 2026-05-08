lessThan(QT_MAJOR_VERSION, 6): error("clignotte-gui requires Qt 6. Run qmake6 instead of qmake.")

QT += core sql qml quick quickcontrols2

CONFIG += c++17

TARGET = note-gui
TEMPLATE = app

SOURCES += \
    gui/main.cpp \
    gui/dbcontroller.cpp \
    gui/notebookmodel.cpp \
    gui/notemodel.cpp

HEADERS += \
    gui/dbcontroller.h \
    gui/notebookmodel.h \
    gui/notemodel.h

RESOURCES += \
    gui/qml.qrc

OBJECTS_DIR = build-gui
MOC_DIR     = build-gui
RCC_DIR     = build-gui
UI_DIR      = build-gui
DESTDIR     = .

isEmpty(PREFIX): PREFIX = /usr/local
target.path = $$PREFIX/bin
INSTALLS += target
