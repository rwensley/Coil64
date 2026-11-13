# KE9ESU fork of Coil64: console test target for the ke9esu cores (run: ke9esu_tests [dir-with-golden-json]).
QT = core
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = ke9esu_tests
include(../../ke9esu/ke9esu.pri)
SOURCES += tst_ke9esu.cpp
DEFINES += KE9ESU_TEST_DIR=\\\"$$PWD\\\"
