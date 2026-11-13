/* balun_core.h - KE9ESU Balun & Unun Designer calculation core, C++ port of core.js (BalunCore v1.3.1).
   Pure calculations, no widgets. Every function is a 1:1 translation of the JavaScript original so that the
   C++ port and the web app give identical numbers and text (see tests/ke9esu_tests).
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later.

   Credits:
    - Ferrite complex-permeability tables (mu', mu'' vs frequency, data/<mix>-Material-Fair-Rite.csv) and the toroid
      self-capacitance formula come from Coil64 by Valery Kustarev (https://github.com/radioacoustick/Coil64,
      GPL-3.0-or-later), which in turn uses Fair-Rite published data and G3RBJ's self-resonance paper.
      Amidon core sizes/AL come from Coil64's amidon.cpp tables (cross-checked against amidoncorp.com).
    - Topology set, Z = w N^2 AL (mu'' + j mu')/mui winding model, VSWR mismatch factor and the connection steps are
      adapted from "Balun & Unun Designer" by Volodymyr Kopytsia (https://github.com/hermes98761234/balun-unun-designer),
      MIT License, Copyright (c) 2026 Volodymyr Kopytsia. The MIT notice is kept in docs/THIRD_PARTY-balun-unun-designer-MIT.txt.
    - Core dimensions/AL/Ae/le/Ve: Fair-Rite and Micrometals datasheets (see Core::src).

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public
License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later
version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
details. You should have received a copy of the GNU General Public License along with this program.
If not, see <https://www.gnu.org/licenses/>.

   UNITS: frequency fHz [Hz] or fMHz [MHz]; lengths mm unless named *Ft / *In / *Cm; AL in nH/turn^2;
   Ae cm^2, le cm, Ve cm^3; flux density mT (and gauss where named); power W; loss density mW/cm^3. */
#ifndef KE9ESU_BALUN_CORE_H
#define KE9ESU_BALUN_CORE_H

#include "ke9esu_util.h"
#include <QString>
#include <QStringList>
#include <QMap>
#include <vector>
#include <array>

namespace ke9esu {

static const char *const CORE_VERSION = "1.3.1";
static const double MU0 = 4e-7 * PI;

struct Material {
    QString id, kind, name;              // kind "ferrite" | "iron"
    double bsat_mT = 0, curieC = 0, mui = 0, q = 0, br = 0, hc = 0;
    bool hasLoss = false; double la = 0, lb = 0, lc = 0, ld = 0;    // Micrometals loss formula a,b,c,d
    std::vector<std::array<double, 3>> mu; // [Hz, mu', mu'']
    QString freq, color, type;            // Amidon iron-powder info
    double maxTempC = 0;
    bool hasAmidon = false; double amMui = 0; QString fResonant, fWideband, fSuppression;
};

struct Equiv { bool has = false; QString id, db; double al = 0; };

struct Core {
    QString id, amidon, pn, mat, src, db;
    double od = 0, idia = 0, h = 0, al = 0, ae = 0, le = 0, ve = 0;
    double alTol[2] = {20, 20};
    QStringList computed;
    // Amidon extras
    bool hasAlAmidon = false; double alAmidonValue = 0; QString alAmidonUnit;
    double alCoil64 = NaN, alWeb = NaN, alGeom = NaN; QString alPick;
    bool alSuspect = false, alDisagree = false;
    QString webUrl, freq, color;
    Equiv equiv;
};

struct Wire {                 // Designer conductors and LC-trap coil wires
    QString id, label, kind;  // kind "wire" | "coax" ("" for coil wires)
    double od = 0, cu = NaN, dia = NaN, ratedW = 0;
    QString jacket;
};

struct Band { QString id; double lo, hi; };

struct Geometry { double ae, le, ve, l0; };
struct Complex2 { double re, im; };
struct Impedance { double r, x, z; };

struct Topology {
    QString id, name, kind;   // kind "choke" | "transformer"
    double ratio; int wires, coreSets, turnsRatio; double currentFactor;
};

struct Check { QString id, status, text; };   // status fail | warn | info | ok
struct Part { double qty; QString item; };

struct BalunInput {
    QString topology = "ruthroff91", coreId = "FT240-43", conductorId = "awg16t";
    double stack = 2, turns = NaN;           // turns NaN = auto
    double zLow = 50, powerW = 100, vswr = 3, fMinMHz = 1.8, fMaxMHz = 54;
    QString duty = "digital"; double dutyPercent = 100;
    double xlFactor = 5, chokeTarget = 5000, cmPercent = 10, leadMM = 100, ambientC = 30;
    QString csMode = "k9yc"; double csPF = 1.3;
};

struct PerFreq { double fMHz, r, x, z, bPk_mT, bHeat_G, heatLimit_G, lossPeakW, lossMatchedW; };
struct WireCut { QString name; int turns; double lengthMM; };
struct CompCap { double pF, pFcalc; double rangePF[2]; double vPeak, ratingV; };
struct Fit { int perLayer, layers; double fill; };
struct CondLoad { double value, limit; QString unit; };

struct DesignResult {
    BalunInput input;
    Topology topology;
    const Core *core = nullptr;
    const Material *material = nullptr;
    const Wire *conductor = nullptr;
    int n1 = 0, n2 = 0, nMin = 0;            // nMin 0 = not reachable
    double target = 0;
    Impedance zAtFmin{0, 0, 0};
    double lNom = 0, lMin = 0, lMax = 0;     // inductanceUH {nom,min,max}
    std::vector<PerFreq> perFreq;
    PerFreq worstLoss{}, bMax{};
    double lossPeakW = 0, lossAvgW = 0, efficiencyWorstPct = NaN, lossDensity_mWcm3 = 0, tempRiseC = 0, coreTempC = 0, efficiencyPct = NaN;
    double stray_pF = 0;
    Fit fit{1, 1, 0};
    int passes = 0;
    double turnLengthMM = 0;
    std::vector<WireCut> wires;
    CondLoad conductorLoad;
    double vHighRms = 0;
    double lFullUH = NaN;
    bool hasCompCap = false; CompCap compCap{};
    int coresNeeded = 0;
    std::vector<Check> checks;
    QStringList steps;
    std::vector<Part> parts;
};

struct Preset { QString id, label; BalunInput input; QStringList keys; };

/* ---- data (loaded once from Qt resources: Coil64 data/<mix>-Material-Fair-Rite.csv and ke9esu/data/amidon_ordered.json) */
bool initData(QString *error = nullptr);
const QMap<QString, Material> &materials();
QStringList materialOrder();               // ids in the web app's key order
const Material *material(const QString &id);
const std::vector<Core> &cores();
const Core *coreById(const QString &id);
const std::vector<Wire> &conductors();
const Wire *conductorById(const QString &id);   // falls back to CONDUCTORS[2] like the JS
const std::vector<Band> &bands();
const std::vector<Topology> &topologies();
const Topology *topology(const QString &id);
const std::vector<Preset> &presets();
const Preset *presetById(const QString &id);
double dutyPercentFor(const QString &duty, bool *known = nullptr);   // ssb 30, cw 50, digital 100
extern const double K9YC_CS_PF;
extern const double MAX_CURRENT_DENSITY;

/* ---- Amidon AL helpers */
double alToNH(double v, bool iron);
double alFromNH(double nh, bool iron);

/* ---- physics */
Geometry geometry(double od, double id, double h);
Complex2 permeability(const QString &matId, double fHz);
double inductanceUH(const Core &core, double N, double stack, int tol);
double selfCapacitancePF(const Core &core, double N, double stack);
Impedance windingImpedance(const Core &core, double N, double fHz, double stack, double csPF);
double fluxDensity_mT(double vRms, double fHz, double N, double aeCm2);
double heatingFluxLimitGauss(double fMHz);
double ironCoreLoss_mWcm3(const QString &matId, double fHz, double bGauss);
double coreSurfaceCm2(const Core &core, double stack);
double tempRiseC(double lossW, double surfaceCm2);
double mismatchFactor(double vswr);
double turnLengthMM(const Core &core, double stack, double condOd);
Fit windingFit(const Core &core, double condOd, double passes);
CompCap compensationCap(double fTopMHz, double powerW, double vswr);
BalunInput normalize(const BalunInput &raw);
std::vector<double> logSpace(double a, double b, int n);
std::vector<double> evalFrequencies(double fMin, double fMax);
std::vector<Band> bandsInRange(double fMinMHz, double fMaxMHz);
double strayPF(const BalunInput &inp, const Core &core, double N);
DesignResult design(const BalunInput &raw);
QStringList connectionSteps(const Topology &top, int n1, int n2, bool coax, const BalunInput &inp);
std::vector<Part> partsList(const Topology &top, const Core &core, double stack, const Wire &cond, const std::vector<WireCut> &wires, const CompCap *cap);
double selfResonanceMHz(const Core &core, double N, double stack, double csPF);   // NaN = none
struct CurvePoint { double fMHz, r, x, z; };
std::vector<CurvePoint> impedanceCurve(const Core &core, double N, double stack, double csPF, double f1MHz, double f2MHz, int n = 200);
double clampv(double v, double a, double b, double d);

/* ---- display units (v1.3.1). wire "awg" | "mm" | "in"; len "mixed" | "mm" | "in". Calculations never change. */
struct Units { QString wire = "awg", len = "mixed"; };
Units setUnits(const QString &wire, const QString &len);   // invalid values are ignored
Units getUnits();
QString dispUnit(const QString &canon);
static const int AS_STORED = -1000;   // JS "null" digit argument of mmTxt()
static const int UNDEF = -2000;       // JS "undefined" digit argument
QString mmTxt(double mm, int dMM = UNDEF, int dIn = UNDEF);
QString inchTxt(double inch, int dIn = UNDEF, const QString &mark = QString(), int dMM = UNDEF);
QString lenDual(double mm);
QString dimsTxt(const Core &c);
double wireDiaMM(const Wire &w);       // NaN for coax
QString wireSize(const Wire &w);
QString wireLabel(const Wire &w);

} // namespace ke9esu
#endif
