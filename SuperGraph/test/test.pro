#-------------------------------------------------
#
# Project created by QtCreator 2018-04-20T17:39:13
#
#-------------------------------------------------
include($$[STARLAB])
include($$[SURFACEMESH])
StarlabTemplate(appbundle)

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = test
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


# Build flag for the static libraries
CONFIG(debug, debug|release) {
    CFG = debug
} else {
    CFG = release
}


# NURBS library
LIBS += -L$$PWD/../NURBS/$$CFG/lib -lNURBS
INCLUDEPATH += ../NURBS

# TopoBlender library
LIBS += -L$$PWD/../TopoBlenderLib/$$CFG/lib -lTopoBlenderLib
INCLUDEPATH += ../TopoBlenderLib


SOURCES += main.cpp\
        mainwindow.cpp \
    Shapeholder.cpp \
    Matcher.cpp \
    Supergraph.cpp \

HEADERS  += mainwindow.h \
    global.h \
    Shapeholder.h \
    Matcher.h \
    CustomDrawObjects.h \
    Supergraph.h \

FORMS    += mainwindow.ui

DISTFILES +=


