#-------------------------------------------------
#
# Project created by QtCreator 2012-07-22T23:10:11
#
#-------------------------------------------------

QT       += core gui opengl

TARGET = gds
TEMPLATE = app

SOURCES += main.cpp\
        startupmodewin.cpp \
    qtsingleapplication/singleapplication.cpp \
    mainwindoweditmode.cpp \
    diagramwidget/qgldiagramwidget.cpp \
    cpphighlighter.cpp \
    texteditorwin.cpp \
    codeeditorwid.cpp \
    mainwindowviewmode.cpp \
    creditswin.cpp

HEADERS  += startupmodewin.h \
    qtsingleapplication/singleapplication.h \
    mainwindoweditmode.h \
    diagramwidget/roundedRectangle.h \
    diagramwidget/qgldiagramwidget.h \
    gdsdbreader.h \
    texteditorwin.h \
    cpphighlighter.h \
    codeeditorwid.h \
    mainwindowviewmode.h \
    creditswin.h

FORMS    += startupmodewin.ui \
    mainwindoweditmode.ui \
    mainwindowviewmode.ui \
    creditswin.ui

RESOURCES += \
    GDSResources.qrc

win32: LIBS += -L$$PWD/lib/ -lglew32

INCLUDEPATH += $$PWD/
DEPENDPATH += $$PWD/

win32: PRE_TARGETDEPS += $$PWD/lib/glew32.lib

win32:RC_FILE = gds_icon/gds.rc
