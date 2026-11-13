/* serialize.h - JSON (de)serialisation of the KE9ESU designer inputs, used by project save/open and the tests.
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_SERIALIZE_H
#define KE9ESU_SERIALIZE_H
#include <QJsonObject>
#include "balun_core.h"
#include "trap_core.h"
namespace ke9esu {
QJsonObject toJson(const BalunInput &i);
BalunInput balunInputFromJson(const QJsonObject &o, BalunInput base = BalunInput());   // missing keys keep base; null turns = auto
QJsonObject toJson(const CoaxTrapInput &i);
CoaxTrapInput coaxInputFromJson(const QJsonObject &o, CoaxTrapInput base = CoaxTrapInput());
QJsonObject toJson(const LcTrapInput &i);
LcTrapInput lcInputFromJson(const QJsonObject &o, LcTrapInput base = LcTrapInput());
}
#endif
