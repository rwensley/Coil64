/* eflw_core.h - KE9ESU end-fed long-wire (EFLW) analyzer core, C++ port of the EFLW section of core.js v1.3.2,
   plus KE9ESU-fork additions (recommended transformer, trap suggestions) used for value passing between windows.
   halfWaveFt(f) = 491.8 * k / f(MHz), k ~ 0.95 (end effect / velocity in wire). For each band the wire length is
   expressed in half-waves h; zones: near integer h -> very high Z (good for 49:1/64:1, bad for 9:1); near h = m+0.5
   (odd lambda/4) -> low Z; otherwise moderate/reactive (9:1 + tuner sweet spot); h < 0.4 -> electrically short.
   Approximate! Real impedance depends on height, counterpoise, ground and nearby objects.
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_EFLW_CORE_H
#define KE9ESU_EFLW_CORE_H

#include "balun_core.h"

namespace ke9esu {

struct EflwOpts { double k = 0.95, tol = NaN, minGapFt = 2; std::vector<Band> bands; };   // tol NaN = function default
struct EflwRow { QString band; double lo, hi; double halfWaves[2]; double lambdaLo[2]; QStringList kinds; QString verdict, unun9, unun49; };
struct AvoidZone { QString band; int n; double loFt, hiFt; };

double halfWaveFt(double fMHz, double k = 0.95);
QString classifyHalfWaves(double h, double tol);
std::vector<EflwRow> eflwAnalysis(double lengthFt, const EflwOpts &opts = EflwOpts());
std::vector<AvoidZone> avoidZones(double maxFt, const EflwOpts &opts = EflwOpts());
std::vector<std::pair<double, double>> goodLengths(double minFt, double maxFt, const EflwOpts &opts = EflwOpts());

/* ---- KE9ESU fork additions (not in the web app) ---- */
struct TransformerAdvice { QString topology, presetId, reason; QStringList highBands, lowBands, shortBands, moderateBands; };
/** Recommended transformer for the analysed wire (web-app guidance as a rule): 49:1 only if the wire is a half-wave
    multiple on every analysed band, otherwise the 9:1 (Ruthroff, or the 4:12 autotransformer) with a tuner. */
TransformerAdvice recommendTransformer(double lengthFt, const std::vector<EflwRow> &rows);
struct TrapSuggestion { QString band; double fMHz, distanceFt, halfWaves; QString text; };
/** For bands where the wire is near n*lambda/2 (high Z, poor for a 9:1), a parallel trap resonant in that band placed
    distanceFt from the feed point makes the wire electrically that long on that band; the distance is chosen so the
    band lands in the moderate zone (h = m +- 0.25 half-waves, checked at both band edges). Rule of thumb only. */
std::vector<TrapSuggestion> trapSuggestions(double lengthFt, const std::vector<EflwRow> &rows, double k = 0.95);

} // namespace ke9esu
#endif
