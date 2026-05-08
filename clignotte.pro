lessThan(QT_MAJOR_VERSION, 6): error("clignotte requires Qt 6. Run qmake6 instead of qmake.")

QT += core sql
QT -= gui

CONFIG += c++17

TARGET = note
CONFIG += console sqlite
CONFIG -= app_bundle

TEMPLATE = app

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/common.cpp \
    src/db.cpp \
    src/render.cpp \
    src/notebook.cpp \
    src/notes.cpp \
    src/tags.cpp \
    src/attachments.cpp \
    src/import.cpp \
    src/sync.cpp \
    src/completions.cpp

HEADERS += \
    src/common.h \
    src/sql.h \
    src/db.h \
    src/render.h \
    src/notebook.h \
    src/notes.h \
    src/tags.h \
    src/attachments.h \
    src/import.h \
    src/sync.h \
    src/completions.h

DISTFILES += \
    pkg/archlinux/PKGBUILD

isEmpty(PREFIX): PREFIX = /usr/local

target.path = $$PREFIX/bin
INSTALLS += target

unix {
    # Generate completion scripts from the freshly-linked binary so the
    # installed scripts always match what the binary emits.
    QMAKE_POST_LINK = mkdir -p completions && \
        ./$(TARGET) completions bash > completions/note && \
        ./$(TARGET) completions zsh  > completions/_note && \
        ./$(TARGET) completions fish > completions/note.fish

    bash_completions.path  = $$PREFIX/share/bash-completion/completions
    bash_completions.files = completions/note
    bash_completions.CONFIG += no_check_exist
    INSTALLS += bash_completions

    zsh_completions.path  = $$PREFIX/share/zsh/site-functions
    zsh_completions.files = completions/_note
    zsh_completions.CONFIG += no_check_exist
    INSTALLS += zsh_completions

    fish_completions.path  = $$PREFIX/share/fish/vendor_completions.d
    fish_completions.files = completions/note.fish
    fish_completions.CONFIG += no_check_exist
    INSTALLS += fish_completions

    license.path  = $$PREFIX/share/licenses/clignotte
    license.files = LICENSE
    INSTALLS += license
}
