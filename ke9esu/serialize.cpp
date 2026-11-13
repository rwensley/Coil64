/* serialize.cpp - see serialize.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "serialize.h"
#include <QJsonValue>
#include <cmath>

namespace ke9esu {
static QJsonValue num(double v) { return std::isnan(v) ? QJsonValue() : QJsonValue(v); }
static void rd(const QJsonObject &o, const char *k, double &v) { if (o.contains(k)) { QJsonValue j = o.value(k); v = j.isDouble() ? j.toDouble() : j.isString() ? j.toString().toDouble() : NaN; } }
static void rd(const QJsonObject &o, const char *k, QString &v) { if (o.contains(k) && !o.value(k).isNull()) { QJsonValue j = o.value(k); v = j.isString() ? j.toString() : QString::number(j.toDouble()); } }

#define F_BALUN(X) X(topology) X(coreId) X(conductorId) X(stack) X(turns) X(zLow) X(powerW) X(vswr) X(fMinMHz) X(fMaxMHz) X(duty) X(dutyPercent) \
    X(xlFactor) X(chokeTarget) X(cmPercent) X(leadMM) X(ambientC) X(csMode) X(csPF) \
    X(capMode) X(capPF) X(capRatingV) X(sweepPoints) X(sweepScale) X(sweepLoadMode) X(sweepLoad) X(sweepLoadMin) X(sweepLoadMax) X(sweepLeadMM) X(sweepK) X(sweepCsPF) X(sweepZ0)
#define F_COAX(X) X(fMHz) X(coaxId) X(formId) X(formOdIn) X(spacingIn) X(leadIn) X(trimIn) X(model) X(powerW) X(zEnd) X(duty) X(dutyPercent) \
    X(customPfFt) X(customVf) X(customOdIn) X(customZ0)
#define F_LC(X) X(fMHz) X(mode) X(xOhm) X(lUH) X(cPF) X(snap) X(coreType) X(formId) X(formDiaMM) X(wireId) X(pitchMM) X(coreId) X(stack) X(powerW) \
    X(zEnd) X(duty) X(dutyPercent) X(capType) X(leadMM) X(ambientC)

template <class T> static QJsonValue jv(const T &v);
template <> QJsonValue jv(const double &v) { return num(v); }
template <> QJsonValue jv(const QString &v) { return v; }

#define W(f) o.insert(#f, jv(i.f));
#define R(f) rd(o, #f, b.f);
QJsonObject toJson(const BalunInput &i) { QJsonObject o; F_BALUN(W) return o; }
BalunInput balunInputFromJson(const QJsonObject &o, BalunInput b) { F_BALUN(R) return b; }
QJsonObject toJson(const CoaxTrapInput &i) { QJsonObject o; F_COAX(W) return o; }
CoaxTrapInput coaxInputFromJson(const QJsonObject &o, CoaxTrapInput b) { F_COAX(R) return b; }
QJsonObject toJson(const LcTrapInput &i) { QJsonObject o; F_LC(W) return o; }
LcTrapInput lcInputFromJson(const QJsonObject &o, LcTrapInput b) { F_LC(R) return b; }
}
