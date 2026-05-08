lessThan(QT_MAJOR_VERSION, 6): error("clignotte-gui requires Qt 6. Run qmake6 instead of qmake.")

QT += core sql qml quick quickcontrols2

CONFIG += c++17

TARGET = note-gui
TEMPLATE = app

SOURCES += \
    src/gui/main.cpp \
    src/gui/dbcontroller.cpp \
    src/gui/notebookmodel.cpp \
    src/gui/notemodel.cpp

HEADERS += \
    src/gui/dbcontroller.h \
    src/gui/notebookmodel.h \
    src/gui/notemodel.h

RESOURCES += \
    src/gui/qml.qrc

OBJECTS_DIR = build-gui
MOC_DIR     = build-gui
RCC_DIR     = build-gui
UI_DIR      = build-gui
DESTDIR     = .

isEmpty(PREFIX): PREFIX = /usr/local
target.path = $$PREFIX/bin
INSTALLS += target
