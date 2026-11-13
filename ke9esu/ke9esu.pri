# KE9ESU fork of Coil64: RF transformers & traps core (balun/unun designer, long-wire analyser, coax and LC traps,
# core database, standard-part combiner). GPL-3.0-or-later.
INCLUDEPATH += $$PWD
HEADERS += $$PWD/ke9esu_util.h $$PWD/balun_core.h $$PWD/trap_core.h $$PWD/eflw_core.h $$PWD/combiner_core.h $$PWD/serialize.h
SOURCES += $$PWD/ke9esu_util.cpp $$PWD/balun_core.cpp $$PWD/trap_core.cpp $$PWD/eflw_core.cpp $$PWD/combiner_core.cpp $$PWD/serialize.cpp
RESOURCES += $$PWD/ke9esu.qrc
