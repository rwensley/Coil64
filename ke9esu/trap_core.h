/* trap_core.h - KE9ESU coax-trap and LC-trap designer core, C++ port of the trap section of core.js v1.3.2.
   Coax traps: VE6YP / W4ULD / K7MEM lumped model with the W8JI open-stub correction.
   LC traps: air-core coil maths ported from Coil64 (resolves.cpp HeliCoilS - R. Weaver helical-filament inductance;
   bessel.cpp + resolve_srf_cs.cpp - D. W. Knight self-capacitance and SRF; resolve_q.cpp solve_Qr - Arnold tables and
   Knight skin effect), by Valery Kustarev, GPL-3.0-or-later. The JS port (and so this one) uses 4.0/19 where
   Coil64 2.4.39 had the integer division 4/19 (fixed in this fork as well).
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later (see balun_core.h for the full notice).
   Lengths: inches for coax traps (as published), mm for LC coils. L in uH, C in pF, f in MHz. */
#ifndef KE9ESU_TRAP_CORE_H
#define KE9ESU_TRAP_CORE_H

#include "balun_core.h"

namespace ke9esu {

static const double C_LIGHT = 299.792458;   // m/us (=> MHz*m)

struct Coax { QString id, label; double z0, pfFt, vf, odIn, centerMM, shieldMM, vMaxRms; QString diel; bool foam, ccs; QString src; };
struct PvcForm { QString id, label; double odIn; };
struct CapType { QString id, label; double q; QString note; };
struct TrapBand { QString band; double fMHz, x; QString kind; double eqUH, eqPF; bool nearRes; };
struct TrapVoltage { double rms, pk; };
struct CoilQ { double q, esr, rac, rdc; };
struct Helical { double uH, lwM; };

const std::vector<Coax> &coaxList();
const Coax *coaxById(const QString &id);           // falls back to COAX[2] like the JS
const std::vector<PvcForm> &pvcForms();
const std::vector<Wire> &coilWires();
const Wire *coilWireById(const QString &id);       // falls back to COIL_WIRES[3]
const std::vector<CapType> &capTypes();
const CapType *capType(const QString &id);
const std::vector<double> *eSeries(const QString &name);   // E6/E12/E24, nullptr otherwise
extern const std::vector<double> CAP_RATINGS;

double wheelerUH(double rIn, double lenIn, double n);
double nagaokaK(double x);
double sheetUH(double Dmm, double lenMM, double n);
double rosaKs(double x);
double rosaKm(double n);
double nagaokaRosaUH(double Dk, double p, double dw, double n);
double heliCoilS(double Lw, double psi, double r, double dw, double accuracy);
Helical helicalUH(double N, double p, double Dk, double dw, double accuracy = 4);
double vfNom(double lod, double ei, double ex);
double coilSRF_MHz(double lk, double Dk, double lwM);
double coilCs_pF(double p, double Dk, double lk);
double lookupPsi(double Df, double dw, double pt, double N, double fkHz);
double skinFactor(double fkHz, double dw);
CoilQ coilQ(double I, double Df, double pm, double dw, double fa, double fsr, double N, double Cs);
double wireRac(double lenM, double dwMM, double fMHz);
double snapE(double v, const QString &series);
double capRating(double vPk, double margin = 2);
TrapVoltage trapVoltage(double powerW, double zEnd);
std::vector<TrapBand> trapReactanceAtBands(double L_uH, double C_pF, double f0MHz, bool skipSame);
double stubCapPF(const Coax &coax, double lenIn, double fMHz);
/** Band preset frequency like the web app: place "mid" = band centre, "below" = 1% below the band; NaN if unknown. */
double bandFrequency(const QString &bandId, const QString &place = "mid");

struct CoaxTrapInput {
    double fMHz = 7.05; QString coaxId = "rg58", formId = "2"; double formOdIn = 2.375, spacingIn = 0, leadIn = 1, trimIn = 1;
    QString model = "lumped"; double powerW = 100, zEnd = 3000; QString duty = "ssb"; double dutyPercent = 30;
    double customPfFt = 30.8, customVf = 0.66, customOdIn = 0.195, customZ0 = 50;
};
CoaxTrapInput normalizeCoaxTrap(const CoaxTrapInput &r);
struct CoaxTrapResult {
    CoaxTrapInput input; Coax coax;
    double n, pitchIn, meanDiaIn, windingLenIn, coaxLenIn, cutLenIn, L_uH, C_pF, X, fCheckMHz, fStubMHz, fLumpedMHz, lNagaokaUH, ldRatio, q, rpOhm;
    TrapVoltage vTrap; double pdKeyW, pdAvgW, fQuarterMHz;
    std::vector<TrapBand> bands; std::vector<Check> checks; QStringList steps; std::vector<Part> parts;
};
CoaxTrapResult coaxTrap(const CoaxTrapInput &raw);

struct LcTrapInput {
    double fMHz = 7.05; QString mode = "X"; double xOhm = 200, lUH = 5, cPF = 100; QString snap = "E12", coreType = "air", formId = "1.5";
    double formDiaMM = 48.26; QString wireId = "awg12"; double pitchMM = 0; QString coreId = "A:T-200-2"; double stack = 1, powerW = 100, zEnd = 3000;
    QString duty = "ssb"; double dutyPercent = 30; QString capType = "np0"; double leadMM = 50, ambientC = 30;
};
LcTrapInput normalizeLcTrap(const LcTrapInput &r);
struct LcCoil { double N, pitchMM, DkMM, lenMM, wireLenMM, uH, csPF, srfMHz, q, esr, wheelerUH, nagaokaRosaUH; const Wire *wire; };
struct LcToroid { const Core *core; int N; double uH, qCore, qWire, wireLenMM, bGauss, limGauss, pCoreW, tempRiseC, coreTempC; const Wire *wire; };
struct LcTrapResult {
    LcTrapInput input; double L_uH, C_pF, cIdealPF, X;
    bool hasCoil = false; LcCoil coil{}; bool hasToroid = false; LcToroid toroid{};
    double q, qCoil, rpOhm; TrapVoltage vTrap; double capRatingV, capCurrentA, pdKeyW, pdAvgW, fWithCsMHz;
    std::vector<TrapBand> bands; std::vector<Check> checks; QStringList steps; std::vector<Part> parts;
};
LcTrapResult lcTrap(const LcTrapInput &raw);

} // namespace ke9esu
#endif
