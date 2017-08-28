#-------------------------------------------------
#
# Project created by QtCreator 2017-08-28T18:41:42
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = OgreQT5
TEMPLATE = app

INCLUDEPATH += /usr/include/OGRE

SOURCES += main.cpp\
        MainWindow.cpp \
    QTOgreWindow.cpp

HEADERS  += MainWindow.hpp \
    QTOgreWindow.hpp

FORMS    += MainWindow.ui

unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += OGRE

unix:!macx: LIBS += -lboost_system
