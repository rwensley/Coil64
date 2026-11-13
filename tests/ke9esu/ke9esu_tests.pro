# KE9ESU fork of Coil64: console test target for the ke9esu cores (run: ke9esu_tests [dir-with-golden-json]).
# gui/widgets only for the theme tests and the combiner wiring-diagram rendering tests (KE9ESU theme colours, style sheet, palette); runs on the offscreen platform
QT = core gui widgets
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = ke9esu_tests
include(../../ke9esu/ke9esu.pri)
INCLUDEPATH += ../../ke9esu/ui ../..
HEADERS += ../../ke9esu/ui/project.h ../../ke9esu/ui/theme.h ../../ke9esu/ui/netdiagram.h
SOURCES += tst_ke9esu.cpp ../../ke9esu/ui/project.cpp ../../ke9esu/ui/theme.cpp ../../ke9esu/ui/netdiagram.cpp
DEFINES += KE9ESU_TEST_DIR=\\\"$$PWD\\\"
