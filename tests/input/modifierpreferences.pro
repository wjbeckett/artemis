QT += core qml testlib
QT -= gui
CONFIG += console testcase c++11
CONFIG -= app_bundle
TARGET = modifierpreferences-test
INCLUDEPATH += ../../app
SOURCES += modifierpreferences.cpp ../../app/settings/streamingpreferences.cpp
HEADERS += ../../app/settings/streamingpreferences.h ../../app/settings/modifiermapping.h
