# KE9ESU fork of Coil64: RF transformers & traps windows (Qt Widgets). Included by Coil64.pro; the core is in ke9esu.pri.
# GPL-3.0-or-later.
include($$PWD/ke9esu.pri)
QT += widgets printsupport
INCLUDEPATH += $$PWD/ui
UI_H = theme project widgets diagram drawings netdiagram coreui balundialog eflwdialog trapdialogs coredbdialog combinerdialog bridge
for(f, UI_H) {
    HEADERS += $$PWD/ui/$${f}.h
    SOURCES += $$PWD/ui/$${f}.cpp
}
