TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    20171634.c  \
    dir.c       \
    dump.c      \
    history.c   \
    opcode.c    \
    type.c \
    assemble.c \
    symtab.c

HEADERS += \
    type.h \
    assemble.h \
    symtab.h
