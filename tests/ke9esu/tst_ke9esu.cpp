/* tst_ke9esu.cpp - KE9ESU fork of Coil64: tests for the RF transformer / trap / long-wire / component-combiner cores.
   1) every test of the KE9ESU web app's test.js (v1.3.1) and the Component Combiner's test.js, ported with the same
      published reference values (G4AON, Fair-Rite, K9YC, DG1JAN, G3TXQ, Amidon, K7MEM, VE6YP, W4ULD, W8JI, Cromwell ...);
   2) a differential "golden" test: golden.json / golden_cc.json were produced by the JavaScript originals
      (ke9esu/tools/gen_golden*.js) and every number, check, step and parts-list text must match the C++ port.
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <cstdio>
#include "balun_core.h"
#include "trap_core.h"
#include "eflw_core.h"
#include "combiner_core.h"
#include "serialize.h"
#include "ke9esu_util.h"
#include "project.h"
#include <QTemporaryDir>

using namespace ke9esu;
static int g_pass = 0, g_fail = 0;
static QStringList g_failed;
struct Fail : std::runtime_error { using std::runtime_error::runtime_error; };
#define REQ(cond, msg) do { if (!(cond)) throw Fail(QString("%1 [line %2] %3").arg(#cond).arg(__LINE__).arg(QString(msg)).toStdString()); } while (0)
#define REQ1(cond) REQ(cond, "")
static void t(const QString &name, const std::function<void()> &fn)
{
    try { fn(); g_pass++; std::printf("PASS %s\n", name.toUtf8().constData()); }
    catch (const std::exception &e) { g_fail++; g_failed << name; std::printf("FAIL %s - %s\n", name.toUtf8().constData(), e.what()); }
    std::fflush(stdout);
}
static bool near(double a, double b, double rel) { return std::fabs(a - b) <= rel * std::fabs(b); }
static QString n2s(double v) { return jsNum(v); }
static QString allText(const QStringList &steps, const std::vector<Part> &parts, const std::vector<Check> &checks)
{
    QStringList l = steps; for (const Part &p : parts) l << p.item; for (const Check &c : checks) l << c.text; return l.join("\n");
}
static bool rx(const QString &re, const QString &s) { return QRegularExpression(re).match(s).hasMatch(); }
static const Check *findCheck(const std::vector<Check> &cs, const QString &id) { for (const Check &c : cs) if (c.id == id) return &c; return nullptr; }
static bool anyStatus(const std::vector<Check> &cs, const QString &st) { for (const Check &c : cs) if (c.status == st) return true; return false; }
static BalunInput BI(const QJsonObject &o) { return balunInputFromJson(o); }
static BalunInput presetInput(const QString &id) { return presetById(id)->input; }

/* ------------------------------------------------------------------ ported test.js ----------------------------------- */
static void portedDesignerTests()
{
    t("core DB integrity (dims, AL, Ae, le, Ve consistent)", [] {
        for (const Core &c : cores()) {
            REQ(c.od > c.idia && c.idia > 0 && c.h > 0, c.id + " dims");
            REQ(c.al > 0 && c.ae > 0 && c.le > 0 && c.ve > 0, c.id + " magnetics");
            REQ(near(c.ae * c.le, c.ve, 0.12), c.id + " Ve vs Ae*le");
            Geometry g = geometry(c.od, c.idia, c.h); double alg = g.l0 * material(c.mat)->mui;
            if (c.db == "Amidon") REQ(near(alg, c.al, 0.35) || c.alSuspect, c.id + " Amidon AL far from geometry but not flagged");
            else REQ(near(alg, c.al, 0.35), c.id + " AL vs geometry");
            REQ(c.alTol[0] > 0 && !c.src.isEmpty(), c.id + " tol/src");
        }
    });
    t("Fair-Rite FT240-43 datasheet values", [] {
        const Core *c = coreById("FT240-43");
        REQ1(c->pn == "5943003801" && c->al == 1075 && c->ae == 1.58 && c->le == 14.5 && c->ve == 22.8 && c->od == 61 && c->idia == 35.55 && c->h == 12.7);
        REQ1(c->alTol[0] == 20 && c->alTol[1] == 20);
    });
    t("permeability tables loaded from Coil64 for 31/43/52/61/67/77", [] {
        for (const char *m : {"31", "43", "52", "61", "67", "77"}) { REQ(material(m)->mu.size() > 40, m); REQ1(material(m)->mui > 0); }
        REQ1(near(permeability("43", 1e4).re, 816, 0.01));
    });
    t("G4AON 3-turn inductance on 240 cores (model + ~0.45 µH test-lead allowance)", [] {
        QList<QPair<QString, double>> meas = {{"FT240-43", 10}, {"FT240-52", 3.5}, {"FT240-61", 2}, {"FT240-67", 1}};
        for (auto &m : meas) { const Core *c = coreById(m.first); Complex2 mu = permeability(c->mat, 2e5);
            double L = inductanceUH(*c, 3, 1, 0) * mu.re / material(c->mat)->mui; REQ(near(L + 0.45, m.second, 0.2), m.first + " " + n2s(L)); }
        REQ1(near(inductanceUH(*coreById("FT240-43"), 3, 1, 0), 10, 0.05));
    });
    t("Fair-Rite single-turn impedance of 2.4\" cable cores (43 and 31), <=25 MHz within 15%", [] {
        QList<std::tuple<QString, double, double>> d = {{"2.4in-43", 10, 34}, {"2.4in-43", 25, 53}, {"2.4in-31", 5, 32.5}, {"2.4in-31", 10, 41}, {"2.4in-31", 25, 61}, {"2643251002", 10, 111}, {"2643251002", 25, 160}};
        for (auto &x : d) { double m = windingImpedance(*coreById(std::get<0>(x)), 1, std::get<1>(x) * 1e6, 1, 0).z; REQ(near(m, std::get<2>(x), 0.15), std::get<0>(x) + " " + n2s(m)); }
    });
    t("K9YC 2.4\" #31 RG-400 cookbook (R at best band) - rms log error <= 0.45 with fitted Cs", [] {
        const Core *c = coreById("2.4in-31"); double pts[][3] = {{18, 1.9, 10}, {16, 3.75, 8}, {14, 7.15, 6.2}, {14, 10.12, 6.5}, {13, 14.2, 5.4}, {11.5, 21.2, 4.8}, {10, 28.5, 4.4}};
        double e = 0;
        for (auto &p : pts) { Impedance z = windingImpedance(*c, p[0], p[1] * 1e6, 1, 1.3); e += std::pow(std::log(z.r / 1000 / p[2]), 2); REQ(z.r / 1000 > p[2] / 2.5 && z.r / 1000 < p[2] * 2.5, n2s(p[1])); }
        double rms = std::sqrt(e / 7); std::printf("      rms log err %.2f\n", rms); REQ1(rms <= 0.45);
    });
    t("DG1JAN FT240-43 EFHW efficiency (2-turn primary, matched): model within 10 points of measurement", [] {
        double meas[][2] = {{3.61, 78.3}, {7.09, 75.9}, {9.99, 77.1}, {14.34, 77.0}, {21.3, 75.8}, {28.26, 72.7}};
        for (auto &m : meas) { BalunInput i; i.topology = "efhw49"; i.coreId = "2.4in-43"; i.stack = 1; i.turns = 2; i.vswr = 1; i.fMinMHz = m[0]; i.fMaxMHz = m[0]; i.conductorId = "awg18e";
            DesignResult d = design(i); REQ(std::fabs(d.efficiencyPct - m[1]) <= 10, n2s(m[0]) + ": " + n2s(d.efficiencyPct)); }
    });
    t("G3TXQ: 9 trifilar turns on T200-2 is far from an ideal transformer at 80 m (XL check fails)", [] {
        BalunInput i; i.topology = "ruthroff91"; i.coreId = "T200-2"; i.stack = 1; i.turns = 9; i.fMinMHz = 3.5; i.fMaxMHz = 29.7; i.conductorId = "awg18t";
        DesignResult d = design(i); REQ1(near(d.lNom, 0.972, 0.01)); REQ1(findCheck(d.checks, "xl")->status == "fail");
    });
    t("Amidon flux example: 25 V, 15 T, Ae 0.133 cm2, 7 MHz -> 40.3 G", [] { REQ1(near(fluxDensity_mT(25, 7e6, 15, 0.133) * 10, 40.3, 0.01)); });
    t("temperature rise formula and surface", [] { REQ1(near(tempRiseC(1, 10), std::pow(100, 0.833), 1e-9)); REQ1(near(coreSurfaceCm2(*coreById("FT240-43"), 1), 77.1, 0.02)); });
    t("mismatch factor & compensation cap (100 pF, 1 kV at 100 W, 10 m, SWR 3)", [] {
        REQ1(mismatchFactor(1) == 1); REQ1(near(mismatchFactor(3), 1.5, 1e-9));
        CompCap c = compensationCap(29.7, 100, 3); REQ1(c.pF == 100); REQ1(c.ratingV == 1000); REQ1(near(c.vPeak, 150, 1e-9));
        REQ1(compensationCap(54, 100, 1).pF == 100);
    });
    t("choke with stray C has a self-resonance; R peaks near it", [] { double f0 = selfResonanceMHz(*coreById("2.4in-31"), 13, 1, 1.3); REQ(f0 > 5 && f0 < 40, n2s(f0)); });
    t("EFHW topologies use 7:1 and 8:1 turns", [] {
        BalunInput a; a.topology = "efhw49"; a.turns = 3; REQ1(design(a).n2 == 21);
        BalunInput b; b.topology = "efhw64"; b.turns = 2; REQ1(design(b).n2 == 16);
    });
    t("all presets produce finite results and checks", [] {
        for (const Preset &p : presets()) { DesignResult d = design(p.input);
            REQ(d.n1 >= 1 && std::isfinite(d.lossPeakW) && std::isfinite(d.tempRiseC) && d.checks.size() > 4 && !d.steps.isEmpty() && !d.parts.empty(), p.id); }
    });
    t("transformer forced off coax; normalize clamps", [] {
        BalunInput i; i.topology = "efhw49"; i.conductorId = "rg58"; i.stack = 9; i.vswr = 0; BalunInput n = normalize(i);
        REQ1(n.conductorId == "awg16t"); REQ1(n.stack == 4); REQ1(n.vswr == 1);
    });
    t("EFLW 124 ft: 160 m low-Z, 80 m high-Z, 20 m moderate; zones & good lengths consistent", [] {
        auto a = eflwAnalysis(124); QMap<QString, EflwRow> by; for (auto &r : a) by[r.band] = r;
        REQ1(by["160m"].kinds.contains("low")); REQ1(by["80m"].kinds.contains("high")); REQ1(by["20m"].kinds == QStringList{"medium"});
        auto zones = avoidZones(300); bool found = false;
        for (auto &z : zones) if (z.band == "80m" && z.n == 1) { found = true; REQ1(z.loFt < 124 && z.hiFt > 124); break; }
        REQ1(found);
        auto good = goodLengths(30, 300);
        for (auto &g : good) { double mid = (g.first + g.second) / 2; for (auto &z : zones) REQ(!(mid > z.loFt && mid < z.hiFt), "good " + n2s(mid)); }
        REQ1(!good.empty());
    });
    t("Amidon AL unit conversions (ferrite mH/1000T = nH/T2; iron uH/100T = 10 nH/T2)", [] {
        REQ1(alToNH(120, true) == 12); REQ1(alToNH(1075, false) == 1075); REQ1(alFromNH(12, true) == 120); REQ1(alFromNH(2053, false) == 2053);
        const Core *c = coreById("A:T-200-2"); REQ1(c->alAmidonValue == 120); REQ1(c->alAmidonUnit.contains("uH/100T")); REQ1(c->al == 12);
    });
    t("Amidon AL cross-check vs datasheets (amidoncorp.com, Fair-Rite, Micrometals)", [] {
        QList<QPair<QString, double>> ex = {{"A:FT-240-43", 1075}, {"A:FT-240-31", 2053}, {"A:T-200-2", 12}, {"A:T-50-2", 4.9}, {"A:T-68-2", 5.7}, {"A:T-130-2", 11}, {"A:FT-50-43", 440}, {"A:FT-140-77", 2545}};
        for (auto &e : ex) { const Core *c = coreById(e.first); REQ(c, e.first); REQ(near(c->al, e.second, 0.005), e.first + " " + n2s(c->al)); }
        REQ1(coreById("A:FT-240-43")->al == coreById("FT240-43")->al);
        REQ1(near(coreById("A:FT-240-31")->al, coreById("FT240-31") ? coreById("FT240-31")->al : 2057, 0.01));
        REQ1(coreById("A:T-200-2")->al == coreById("T200-2")->al);
    });
    t("Amidon AL source pick: rejects Coil64 FT-240-43 1300 and web typos (T-106-6 166, T-106-17 1851)", [] {
        const Core *a = coreById("A:FT-240-43"); REQ1(a->alCoil64 == 1300 && a->alWeb == 1075 && a->al == 1075 && a->alDisagree);
        REQ1(coreById("A:T-106-6")->alAmidonValue == 116);
        const Core *d = coreById("A:T-106-17"); REQ1(!d || d->alAmidonValue < 1000);
        const Core *e = coreById("A:FT-140-77"); REQ1(e->equiv.has && e->equiv.al == 2400 && e->al == 2545);
    });
    t("Amidon DB coverage: ferrite 43/61/77/31/52/67 and iron -0..-26 mixes, iron colours", [] {
        int n = 0; QSet<QString> mats; for (const Core &c : cores()) if (c.db == "Amidon") { n++; mats << c.mat; }
        for (const char *m : {"43", "61", "77", "31", "52", "67", "-2", "-6", "-1", "-3", "-7", "-10", "-12", "-17", "-26", "-0"}) REQ(mats.contains(m), m);
        REQ1(material("-2")->color.contains("red", Qt::CaseInsensitive) && material("-6")->color.contains("yellow", Qt::CaseInsensitive));
        REQ1(n > 250);
    });
    t("iron-powder Amidon core usable in transformer designer", [] { BalunInput i; i.coreId = "A:T-200-2"; DesignResult d = design(i); REQ1(d.core && std::isfinite(d.lossPeakW)); });
    t("Ron's T-50-6 correction: Amidon AL 40 (web) instead of Coil64 46", [] { const Core *c = coreById("A:T-50-6"); REQ(c && near(c->al, 4.0, 1e-9), c ? n2s(c->al) : "missing"); });
    // traps
    t("coax trap = K7MEM RG-8X on 2.375in form: turns within 1%, cut length within 6%", [] {
        double k7[][3] = {{3.75, 119.75, 14.01}, {7.15, 68.875, 8.00}, {10.075, 52, 6.01}, {14.175, 39.625, 4.55}, {18.118, 32.875, 3.74}, {21.225, 29.125, 3.31}, {24.94, 25.875, 2.92}, {28.85, 23.25, 2.61}};
        for (auto &k : k7) { CoaxTrapInput i; i.fMHz = k[0]; i.coaxId = "rg8x"; i.formOdIn = 2.375; CoaxTrapResult r = coaxTrap(i);
            REQ(near(r.n, k[2], 0.01), n2s(k[0]) + " n " + n2s(r.n)); REQ(near(r.cutLenIn, k[1], 0.06), n2s(r.cutLenIn)); REQ1(near(r.fCheckMHz, k[0], 0.002)); }
    });
    t("coax trap = VE6YP CoaxTrap example (7.04 MHz, 3.5in form, RG-58 26 pF/ft): 3.689 uH, 138.5 pF, 64in", [] {
        CoaxTrapInput i; i.fMHz = 7.04; i.coaxId = "custom"; i.customPfFt = 26; i.customOdIn = 0.195; i.formOdIn = 3.5; CoaxTrapResult r = coaxTrap(i);
        REQ(near(r.L_uH, 3.689, 0.01) && near(r.C_pF, 138.5, 0.01) && near(r.coaxLenIn, 64, 0.02), n2s(r.L_uH) + " " + n2s(r.C_pF) + " " + n2s(r.coaxLenIn));
    });
    t("coax trap = W4ULD/WD8PNL RG-58 table: reactance within 3%", [] {
        double w[][3] = {{7.075, 1.00, 133.0}, {7.075, 2.25, 155.6}, {14.075, 2.00, 135.7}, {21.1, 1.75, 123.5}, {28.15, 1.75, 117.1}, {10.125, 1.5, 140.6}};
        for (auto &x : w) { CoaxTrapInput i; i.fMHz = x[0]; i.coaxId = "custom"; i.customPfFt = 28.5; i.customOdIn = 0.195; i.formOdIn = x[1]; REQ(near(coaxTrap(i).X, x[2], 0.03), n2s(x[0])); }
    });
    t("coax stub correction ~ W8JI measurement (59in RG-58A at 7.04 MHz measured 164 pF)", [] {
        double c = stubCapPF(*coaxById("rg58a"), 59, 7.04); REQ(near(c, 164, 0.06), n2s(c));
        CoaxTrapInput i; i.fMHz = 7.04; i.coaxId = "rg58"; i.formOdIn = 3.5; double q = coaxTrap(i).q; REQ(q > 90 && q < 350, n2s(q));
    });
    t("coax DB consistent: pF/ft = 1016.7/(Z0*VF) within 8%; diagram steps cross shield/centre", [] {
        for (const Coax &c : coaxList()) REQ(near(1016.7 / (c.z0 * c.vf), c.pfFt, 0.08), c.id);
        QString s = coaxTrap(CoaxTrapInput()).steps.join(" "); REQ1(s.contains("shield", Qt::CaseInsensitive) && s.contains("cent", Qt::CaseInsensitive));
    });
    t("LC trap math: W8JI 7.04 MHz trap with 105 pF -> 4.87 uH (measured 4.92 uH)", [] {
        LcTrapInput i; i.fMHz = 7.04; i.mode = "C"; i.cPF = 105; i.snap = "none"; REQ1(near(lcTrap(i).L_uH, 4.92, 0.02));
        LcTrapInput j; j.fMHz = 7.05; j.mode = "X"; j.xOhm = 200; j.snap = "none"; LcTrapResult x = lcTrap(j);
        REQ1(near(x.X, 200, 0.001) && near(1 / (2 * PI * std::sqrt(x.L_uH * 1e-6 * x.C_pF * 1e-12)) / 1e6, 7.05, 1e-6));
    });
    t("air coil: Coil64 helical (Weaver) port vs Nagaoka+Rosa within 2% and Wheeler within 5%; SRF sane", [] {
        double cs[][4] = {{48.26, 10, 4.2, 2.05}, {25, 20, 2, 1}, {60, 6, 8, 2.6}, {33.4, 15, 3, 1.6}};
        for (auto &c : cs) { double h = helicalUH(c[1], c[2], c[0], c[3]).uH, nr = nagaokaRosaUH(c[0], c[2], c[3], c[1]), wh = wheelerUH(c[0] / 50.8, c[1] * c[2] / 25.4, c[1]);
            REQ(near(h, nr, 0.02), "NR"); REQ(near(h, wh, 0.05), "Wh"); }
        LcTrapResult r = lcTrap(LcTrapInput()); double lw = r.coil.wireLenMM / 1000, half = 300 / (2 * lw);
        REQ1(r.coil.srfMHz > 0.5 * half && r.coil.srfMHz < 1.5 * half); REQ1(r.coil.q > 200 && r.coil.q < 1200);
    });
    t("E-series snap, capacitor voltage rating, ferrite toroid -> fail", [] {
        REQ1(snapE(112.9, "E12") == 120 && snapE(47.2, "E6") == 47 && snapE(91.5, "E24") == 91);
        REQ1(capRating(775) == 2000 && capRating(300) == 1000);
        LcTrapInput f; f.coreType = "toroid"; f.coreId = "A:FT-240-43"; REQ1(anyStatus(lcTrap(f).checks, "fail"));
        LcTrapInput i; i.coreType = "toroid"; i.coreId = "A:T-200-2"; REQ1(!anyStatus(lcTrap(i).checks, "fail"));
    });
    t("trap reactance at other bands: inductive below, capacitive above resonance", [] {
        auto b = lcTrap(LcTrapInput()).bands; const TrapBand *lo = nullptr, *hi = nullptr;
        for (auto &x : b) { if (!lo && x.fMHz < 5) lo = &x; if (!hi && x.fMHz > 10) hi = &x; }
        REQ1(lo && hi && lo->x > 0 && hi->x < 0 && lo->kind == "inductive" && hi->kind == "capacitive");
    });
    t("9:1 autotransformer: 1:3 turns, tap at N/3, single wire, L full = 9 x tap L, high side 3 x V", [] {
        const Topology *top = topology("auto91"); REQ1(top && top->ratio == 9 && top->turnsRatio == 3 && top->wires == 1 && top->kind == "transformer");
        BalunInput i; i.topology = "auto91"; i.coreId = "FT240-43"; i.stack = 2; i.conductorId = "awg16t"; i.fMinMHz = 1.8; i.fMaxMHz = 54; i.powerW = 100; i.vswr = 3; i.duty = "digital";
        DesignResult d = design(i); REQ1(d.n2 == 3 * d.n1); REQ1(near(d.lFullUH, 9 * d.lNom, 1e-9));
        REQ1(d.wires.size() == 1 && d.wires[0].name.contains("tapped at turn"));
        REQ1(near(d.vHighRms, 3 * std::sqrt(100.0 * 50) * mismatchFactor(3), 1e-9));
        bool tap = false, shell = false, tri = false; for (auto &s : d.steps) { tap |= s.contains("Tap at turn"); shell |= s.contains("SO-239 shell"); tri |= s.contains("trifilar bundle"); }
        REQ1(tap && shell && !tri);
        for (const char *id : {"auto", "leak", "split", "comp"}) { const Check *c = findCheck(d.checks, id); REQ(c && c->status == "info", id); }
        REQ1(!d.hasCompCap);
    });
    t("9:1 autotransformer = Cromwell example (FT140-43: 8 turns 56.6 uH, 1245 ohm at 3.5 MHz; >= 4 tap turns)", [] {
        double L = inductanceUH(*coreById("FT140-43"), 8, 1, 0); REQ1(near(L, 56.6, 0.005)); REQ1(near(2 * PI * 3.5 * L, 1245, 0.005));
        BalunInput i; i.topology = "auto91"; i.coreId = "FT140-43"; i.stack = 1; i.conductorId = "awg16t"; i.fMinMHz = 3.5; i.fMaxMHz = 29.7; i.xlFactor = 5; i.vswr = 2;
        REQ1(design(i).nMin == 4);
    });
    t("9:1 autotransformer KE9ESU (2 x FT240-43, 1.8-54 MHz, 100 W FT8): auto 4:12, XL >= 5x, no FAIL; 3:9 flagged", [] {
        BalunInput b; b.topology = "auto91"; b.coreId = "FT240-43"; b.stack = 2; b.conductorId = "awg16t"; b.fMinMHz = 1.8; b.fMaxMHz = 54; b.powerW = 100; b.vswr = 3; b.duty = "digital";
        DesignResult d = design(b); REQ1(d.n1 == 4 && d.n2 == 12 && d.zAtFmin.x >= 250); REQ1(!anyStatus(d.checks, "fail"));
        b.turns = 3; DesignResult d3 = design(b); const Check *x = findCheck(d3.checks, "xl"); REQ1(x && x->status != "ok");
        REQ1(presetById("ke9esu-auto91") && presetById("ke9esu")->input.topology == "ruthroff91");
    });
    t("units: default AWG + mixed keeps v1.3.0 text", [] {
        REQ1(getUnits().wire == "awg" && getUnits().len == "mixed");
        DesignResult d = design(presetInput("ke9esu")); bool ok = false;
        for (auto &p : d.parts) ok |= rx(S("^AWG 16 PTFE hook-up \\(~2\\.0 mm OD\\) — Wire A: \\d+ mm \\(\\d+\\.\\d in\\) incl\\. leads$"), p.item);
        REQ1(ok); REQ1(d.parts[0].item == S("FT240-43 core (5943003801, 61×35.55×12.7 mm)"));
        CoaxTrapResult r = coaxTrap(CoaxTrapInput()); REQ1(rx("^Cut \\d+\\.\\d\" of ", r.steps[0]) && allText(r.steps, r.parts, r.checks).contains(S("AWG 18–14")));
        REQ1(wireLabel(*conductorById("awg16e")) == "AWG 16 enamelled");
    });
    t("units: metric - no AWG and no inch values in Designer, coax trap and LC trap text; numbers unchanged", [] {
        BalunInput bi; bi.topology = "ruthroff91"; bi.coreId = "FT240-43"; bi.stack = 2; bi.conductorId = "awg16t";
        DesignResult base = design(bi); CoaxTrapResult cb = coaxTrap(CoaxTrapInput()); LcTrapResult lb = lcTrap(LcTrapInput());
        struct Restore { ~Restore() { setUnits("awg", "mixed"); } } restore;
        setUnits("mm", "mm");
        for (const Wire &cond : conductors()) if (cond.kind == "wire") for (const char *top : {"ruthroff91", "auto91", "guanella41"}) {
            BalunInput i; i.topology = top; i.coreId = "FT240-43"; i.stack = 2; i.conductorId = cond.id; DesignResult d = design(i); QString tx = allText(d.steps, d.parts, d.checks);
            REQ(!tx.contains("AWG"), cond.id + " AWG left"); REQ(!rx(S("\\d(\\.\\d+)? in\\b|\\d\"|″"), tx), cond.id + " inch left");
        }
        DesignResult d = design(bi); REQ1(d.n1 == base.n1 && d.lNom == base.lNom && d.lossAvgW == base.lossAvgW);
        bool ok = false; for (auto &p : d.parts) ok |= rx(S("^Ø1\\.29 mm PTFE hook-up \\(~2\\.0 mm OD\\) — Wire A: \\d+ mm incl\\. leads$"), p.item); REQ1(ok);
        CoaxTrapResult r = coaxTrap(CoaxTrapInput()); QString tr = allText(r.steps, r.parts, r.checks);
        REQ1(r.n == cb.n); REQ(rx("^Cut \\d+ mm of ", r.steps[0]), r.steps[0]); REQ1(tr.contains(S("Ø1.0–1.6 mm")));
        QString tr2 = tr; tr2.replace(QRegularExpression("\\d(-\\d/\\d)?\"( |-)Sch 40"), "");
        REQ1(!tr.contains("AWG") && !rx("\\d\"", tr2));
        for (const char *ct : {"air", "toroid"}) { LcTrapInput q; q.coreType = ct; LcTrapResult qr = lcTrap(q); QString tq = allText(qr.steps, qr.parts, qr.checks);
            REQ(!tq.contains("AWG") && !rx("\\d(\\.\\d+)? in\\b|\\d\"", tq), ct); }
        REQ1(lcTrap(LcTrapInput()).L_uH == lb.L_uH);
        REQ1(wireLabel(*coilWireById("tube6")) == S("Ø6.35 mm copper tubing (1/4\" nominal)"));
    });
    t("units: inches - no mm values; diameters converted (AWG 16 = 0.051 in, FT240 = 2.402 in OD)", [] {
        struct Restore { ~Restore() { setUnits("awg", "mixed"); } } restore;
        setUnits("in", "in");
        BalunInput i; i.topology = "auto91"; i.coreId = "FT240-43"; i.stack = 2; i.conductorId = "awg16e"; DesignResult d = design(i); QString tx = allText(d.steps, d.parts, d.checks);
        REQ1(!rx("\\d(\\.\\d+)? mm\\b", tx) && !tx.contains("AWG"));
        REQ1(tx.contains(S("Ø0.051 in enamelled")) && tx.contains(S("2.402×1.400×0.500 in")));
        LcTrapResult q = lcTrap(LcTrapInput()); REQ1(!rx("\\d mm\\b", allText(q.steps, q.parts, q.checks)));
        setUnits("awg", "in"); REQ1(wireLabel(*conductorById("awg16t")) == "AWG 16 PTFE hook-up (~0.079 in OD)");
    });
    t("units: helpers, validation and round trip", [] {
        Units u = setUnits("furlong", "cubit"); REQ1(u.wire == "awg" && u.len == "mixed");
        REQ1(std::fabs(wireDiaMM(*conductorById("awg16e")) - 1.291) < 0.002 && std::fabs(wireDiaMM(*conductorById("awg22e")) - 0.644) < 0.002);
        REQ1(dispUnit("in") == "in" && dispUnit("mm") == "mm");
        setUnits("", "mm"); REQ1(inchTxt(2.375, 3) == "60.32 mm"); REQ1(lenDual(1245) == "1245 mm");
        setUnits("", "in"); REQ1(mmTxt(1245) == "49.02 in"); REQ1(lenDual(1245) == "49.0 in");
        setUnits("", "mixed"); REQ1(lenDual(1245) == "1245 mm (49.0 in)"); REQ1(inchTxt(2.375, 3) == "2.375\"");
        REQ1(getUnits().wire == "awg" && getUnits().len == "mixed");
    });
    // fork additions
    t("fork: Ron's presets (9:1 Ruthroff 4 trifilar AWG16 PTFE on 2 x FT240-43; 4:12 auto; 16 T RG-400 on 2.4\" #31)", [] {
        DesignResult a = design(presetInput("ke9esu")); REQ1(a.topology.id == "ruthroff91" && a.n1 == 4 && a.input.stack == 2 && a.input.conductorId == "awg16t" && a.core->id == "FT240-43");
        DesignResult b = design(presetInput("ke9esu-auto91")); REQ1(b.n1 == 4 && b.n2 == 12 && b.input.stack == 2);
        DesignResult c = design(presetInput("ke9esu-choke")); REQ1(c.topology.id == "choke11" && c.n1 == 16 && c.core->id == "2.4in-31" && c.input.conductorId == "rg400");
    });
    t("fork: long-wire 124 ft -> 9:1 recommendation and trap suggestions on high-Z bands", [] {
        auto rows = eflwAnalysis(124); TransformerAdvice a = recommendTransformer(124, rows);
        REQ1(a.topology == "ruthroff91" && a.presetId == "ke9esu" && a.highBands.contains("80m"));
        auto s = trapSuggestions(124, rows); bool has80 = false;
        for (auto &x : s) { if (x.band == "80m") has80 = true; REQ1(x.distanceFt > 0 && x.distanceFt < 124 - 3); }
        REQ1(has80);
    });
    t("fork: input JSON round trip (project files)", [] {
        BalunInput i = presetInput("ke9esu-choke"); BalunInput j = balunInputFromJson(toJson(i)); REQ1(toJson(i) == toJson(j));
        LcTrapInput l; l.coreType = "toroid"; l.fMHz = 3.6; REQ1(toJson(lcInputFromJson(toJson(l))) == toJson(l));
        CoaxTrapInput c; c.coaxId = "custom"; REQ1(toJson(coaxInputFromJson(toJson(c))) == toJson(c));
    });
}

/* ------------------------------------------------------------------ ported Component Combiner tests ------------------ */
static cc::Result best(const cc::Params &p) { cc::Solution s = cc::solve(p); auto r = cc::sortResults(s.results, "accuracy", 1); if (r.empty()) throw Fail("no results"); return r[0]; }
static cc::Params P(QChar type, double target, const QString &series, int maxParts, const QString &topo) { cc::Params p; p.type = type; p.target = target; p.series = series; p.maxParts = maxParts; p.topo = topo; return p; }
static bool close(double a, double b, double r = 1e-9) { return std::fabs(a - b) <= r * std::fabs(b); }
static void portedCombinerTests()
{
    t("combiner: parse", [] {
        QList<QPair<QString, double>> cases = {{"4k7", 4700}, {"4.7k", 4700}, {"2.2u", 2.2e-6}, {"2u2", 2.2e-6}, {"100n", 1e-7}, {"33p", 33e-12}, {"10mH", 0.01}, {"1M", 1e6}, {"1meg", 1e6}, {"4R7", 4.7}, {"100nF", 1e-7}, {"47uH", 47e-6}, {S("1k5Ω"), 1500}, {"220 ohm", 220}, {S("10µF"), 1e-5}, {"1e-6", 1e-6}, {"1234", 1234}};
        for (auto &c : cases) REQ(close(cc::parseValue(c.first), c.second), c.first);
        for (const char *b : {"abc", "", "4.7k7", "-5", "k"}) REQ(std::isnan(cc::parseValue(b)), b);
    });
    t("combiner: E-series tables", [] {
        QStringList e96 = QString("100 102 105 107 110 113 115 118 121 124 127 130 133 137 140 143 147 150 154 158 162 165 169 174 178 182 187 191 196 200 205 210 215 221 226 232 237 243 249 255 261 267 274 280 287 294 301 309 316 324 332 340 348 357 365 374 383 392 402 412 422 432 442 453 464 475 487 499 511 523 536 549 562 576 590 604 619 634 649 665 681 698 715 732 750 768 787 806 825 845 866 887 909 931 953 976").split(' ');
        auto s = cc::eSeries("E96"); REQ1(s.size() == 96); for (int i = 0; i < 96; i++) REQ(s[i] == e96[i].toDouble() / 100, e96[i]);
        auto e48 = cc::eSeries("E48"); REQ1(e48.size() == 48); double f6[] = {1, 1.05, 1.1, 1.15, 1.21, 1.27}; for (int i = 0; i < 6; i++) REQ1(e48[i] == f6[i]);
        auto e192 = cc::eSeries("E192"); REQ1(e192.size() == 192); REQ1(std::count(e192.begin(), e192.end(), 9.2) == 1 && std::count(e192.begin(), e192.end(), 9.19) == 0);
        REQ1(cc::eSeries("E24").size() == 24);
    });
    t("combiner: leaf ranges", [] {
        auto r = cc::buildLeaves('R', "E12"); REQ1(r.front() == 1 && r.back() == 1e7);
        auto c = cc::buildLeaves('C', "E6"); REQ1(close(c.front(), 1e-12) && close(c.back(), 1e-2));
        auto l = cc::buildLeaves('L', "E24"); REQ1(close(l.front(), 1e-9) && close(l.back(), 1));
    });
    t("combiner: R 1234 E12 max3 sub-1%", [] { auto b = best(P('R', 1234, "E12", 3, "mixed")); REQ1(std::fabs(b.err) < 0.01 && b.count <= 3); std::printf("      %s\n", b.expr.toUtf8().constData()); });
    t("combiner: C 3.3uF exact single", [] { auto r = cc::solve(P('C', 3.3e-6, "E12", 3, "mixed")); auto s = cc::sortResults(r.results, "simple", 0.001)[0]; REQ1(s.count == 1 && std::fabs(s.err) < 1e-12); });
    t("combiner: C math: parallel adds, series reciprocal", [] {
        auto b = best(P('C', 2e-6, "E6", 2, "parallel")); REQ(std::fabs(b.err) < 1e-12 && b.count == 2, b.expr);
        b = best(P('C', 0.5e-6, "E6", 2, "series")); REQ(std::fabs(b.err) < 1e-12 && b.count == 2, b.expr);
        double s = 0; for (double v : b.parts) s += 1 / v; REQ1(close(s, 1 / 0.5e-6));
    });
    t("combiner: L math: series adds, parallel reciprocal", [] {
        auto b = best(P('L', 20e-6, "E6", 2, "series")); REQ(std::fabs(b.err) < 1e-12 && b.count == 2, b.expr);
        b = best(P('L', 5e-6, "E6", 2, "parallel")); REQ(std::fabs(b.err) < 1e-12 && b.count == 2, b.expr);
        double s = 0; for (double v : b.parts) s += 1 / v; REQ1(close(1 / s, 5e-6));
    });
    t("combiner: all results self-consistent (R,C,L, all topologies)", [] {
        for (QChar type : {QChar('R'), QChar('C'), QChar('L')}) for (const char *topo : {"series", "parallel", "mixed"}) {
            double target = type == 'R' ? 3456 : type == 'C' ? 7.77e-9 : 123e-6; auto r = cc::solve(P(type, target, "E24", 5, topo));
            REQ1(!r.results.empty());
            for (auto &x : r.results) {
                REQ(close(cc::evalTree(x.tree, type), x.value, 1e-9), x.expr); REQ1(x.count <= 5 && x.count == (int)x.parts.size());
                if (QString(topo) != "mixed") { QSet<int> ops; std::function<void(const cc::Node &)> w = [&](const cc::Node &n) { if (!n.leaf) { ops << n.op; for (auto &k : n.kids) w(k); } }; w(x.tree);
                    REQ1(ops.size() <= 1 && (ops.isEmpty() || *ops.begin() == (QString(topo) == "series" ? 1 : 2))); }
            }
        }
    });
    t("combiner: custom parts list", [] {
        auto pl = cc::parseList("100, 220 470\n1k;2.2k junk"); REQ1(pl.bad == QStringList{"junk"});
        cc::Params p = P('R', 1570, "E12", 3, "mixed"); p.custom = pl.values; auto r = cc::solve(p);
        for (auto &x : r.results) for (double v : x.parts) REQ1(std::find(pl.values.begin(), pl.values.end(), v) != pl.values.end());
        auto b = cc::sortResults(r.results, "accuracy", 1)[0]; REQ(std::fabs(b.err) < 1e-12, b.expr);
    });
    t("combiner: exhaustive check: 3-part E6 mixed matches brute force", [] {
        std::vector<double> leaves; for (double v : cc::buildLeaves('R', "E6")) if (v >= 0.1 && v <= 1e5) leaves.push_back(v);
        double bestErr = INFINITY, T = 777; auto s = [](double a, double b) { return a + b; }; auto p = [](double a, double b) { return a * b / (a + b); };
        std::function<double(double, double)> fs[] = {s, p};
        for (double a : leaves) { bestErr = std::min(bestErr, std::fabs(a - T) / T); for (double b : leaves) for (auto &f : fs) { double ab = f(a, b); bestErr = std::min(bestErr, std::fabs(ab - T) / T);
            for (double c : leaves) for (auto &g : fs) bestErr = std::min(bestErr, std::fabs(g(ab, c) - T) / T); } }
        auto b = best(P('R', T, "E6", 3, "mixed")); REQ1(std::fabs(b.err) <= bestErr * 1.0001 + 1e-15);
    });
    t("combiner: fewest parts preferred among equal error", [] {
        auto r = cc::solve(P('R', 4700, "E12", 6, "mixed")); REQ1(cc::sortResults(r.results, "accuracy", 1)[0].count == 1 && cc::sortResults(r.results, "simple", 1)[0].count == 1);
    });
    // The JS suite's 6 s budget; shared CI runners are slower, so CI may raise it with KE9ESU_SPEED_LIMIT_MS.
    static const qint64 lim = qEnvironmentVariableIsSet("KE9ESU_SPEED_LIMIT_MS") ? qEnvironmentVariable("KE9ESU_SPEED_LIMIT_MS").toLongLong() : 6000;
    for (const char *ser : {"E12", "E24", "E96", "E192"}) t(QString("combiner: 6-part speed ") + ser, [ser] { auto r = cc::solve(P('R', 1234, ser, 6, "mixed")); std::printf("      %lld ms (limit %lld)\n", (long long)r.ms, (long long)lim); REQ1(r.ms < lim); });
    t("combiner: 6-part caps E96 speed", [] { auto r = cc::solve(P('C', 1.234e-9, "E96", 6, "mixed")); std::printf("      %lld ms (limit %lld)\n", (long long)r.ms, (long long)lim); REQ1(r.ms < lim); });
    t("combiner: edge: target outside range still works", [] { auto b = best(P('R', 2e7, "E12", 2, "mixed")); REQ(std::fabs(b.err) < 1e-12, b.expr); });
}

/* ------------------------------------------------------------------ golden differential tests -------------------------- */
static QJsonObject loadJson(const QString &path)
{
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) throw Fail(("cannot open " + path).toStdString());
    return QJsonDocument::fromJson(f.readAll()).object();
}
struct Diff {
    QStringList errs; int n = 0;
    void num(const QString &what, const QJsonValue &js, double cpp, double rel = 1e-9) {
        n++;
        if (js.isNull() || js.isUndefined()) { if (std::isfinite(cpp)) err(what, "null (NaN/Infinity in JS)", n2s(cpp)); return; }   // JSON.stringify writes NaN and Infinity as null
        double a = js.toDouble();
        if (std::isnan(cpp) || std::fabs(a - cpp) > rel * std::max(std::fabs(a), 1e-12) + 1e-12) err(what, n2s(a), n2s(cpp));
    }
    void str(const QString &what, const QJsonValue &js, const QString &cpp) { n++; if (js.toString() != cpp) err(what, js.toString(), cpp); }
    void err(const QString &what, const QString &a, const QString &b) { if (errs.size() < 40) errs << what + "\n        js : " + a + "\n        c++: " + b; else if (errs.size() == 40) errs << "..."; }
    template <class C> void checks(const QString &w, const QJsonArray &js, const C &cs) {
        if (js.size() != (int)cs.size()) { err(w + " checks count", QString::number(js.size()), QString::number(cs.size())); return; }
        for (int i = 0; i < js.size(); i++) { QJsonArray a = js[i].toArray(); str(w + " check " + a[0].toString() + " id", a[0], cs[i].id); str(w + " check " + a[0].toString() + " status", a[1], cs[i].status); str(w + " check " + a[0].toString(), a[2], cs[i].text); }
    }
    void steps(const QString &w, const QJsonArray &js, const QStringList &s) {
        if (js.size() != s.size()) { err(w + " steps count", QString::number(js.size()), QString::number(s.size())); return; }
        for (int i = 0; i < js.size(); i++) str(w + " step " + QString::number(i), js[i], s[i]);
    }
    void parts(const QString &w, const QJsonArray &js, const std::vector<Part> &p) {
        if (js.size() != (int)p.size()) { err(w + " parts count", QString::number(js.size()), QString::number(p.size())); return; }
        for (int i = 0; i < js.size(); i++) { QJsonArray a = js[i].toArray(); num(w + " part qty " + QString::number(i), a[0], p[i].qty); str(w + " part " + QString::number(i), a[1], p[i].item); }
    }
    template <class R> void txt(const QString &w, const QJsonObject &o, const R &r) { checks(w, o["checks"].toArray(), r.checks); steps(w, o["steps"].toArray(), r.steps); parts(w, o["parts"].toArray(), r.parts); }
    void bands(const QString &w, const QJsonArray &js, const std::vector<TrapBand> &b) {
        if (js.size() != (int)b.size()) { err(w + " bands count", QString::number(js.size()), QString::number(b.size())); return; }
        for (int i = 0; i < js.size(); i++) { QJsonArray a = js[i].toArray(); str(w + " band", a[0], b[i].band); num(w + " band f", a[1], b[i].fMHz); num(w + " band x " + b[i].band, a[2], b[i].x, 1e-7);
            str(w + " band kind", a[3], b[i].kind); num(w + " eqUH", a[4], b[i].eqUH, 1e-7); num(w + " eqPF", a[5], b[i].eqPF, 1e-7); n++; if (a[6].toBool() != b[i].nearRes) err(w + " near " + b[i].band, a[6].toBool() ? "true" : "false", b[i].nearRes ? "true" : "false"); }
    }
    void done() { if (!errs.isEmpty()) throw Fail((QString::number(errs.size()) + " mismatches (of " + QString::number(n) + " values):\n      " + errs.join("\n      ")).toStdString()); std::printf("      %d values identical\n", n); }
};
static QString inputTag(const QJsonObject &o) { return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)); }

static void goldenTests(const QString &dir)
{
    QJsonObject g;
    t("golden: load golden.json (JS core v1.3.1 reference outputs)", [&] { g = loadJson(dir + "/golden.json"); REQ1(g["design"].toArray().size() > 300); });
    if (g.isEmpty()) return;
    t("golden: input defaults identical to the web app", [&] {
        Diff D; QJsonObject d = g["defaults"].toObject();
        auto cmp = [&](const QString &w, const QJsonObject &js, const QJsonObject &cpp) { for (auto k : js.keys()) { D.n++; if (js[k] != cpp[k]) D.err(w + "." + k, js[k].toVariant().toString(), cpp[k].toVariant().toString()); } };
        cmp("design", d["design"].toObject(), toJson(BalunInput())); cmp("coax", d["coax"].toObject(), toJson(CoaxTrapInput())); cmp("lc", d["lc"].toObject(), toJson(LcTrapInput()));
        D.done();
    });
    t("golden: materials and permeability tables", [&] {
        Diff D; QJsonArray ms = g["materials"].toArray(); REQ1(ms.size() == materialOrder().size());
        for (int i = 0; i < ms.size(); i++) { QJsonArray a = ms[i].toArray(); QString id = a[0].toString(); D.str("mat order", a[0], materialOrder()[i]); const Material *m = material(id); REQ(m, id);
            D.str(id + " kind", a[1], m->kind); D.str(id + " name", a[2], m->name); D.num(id + " mui", a[3], m->mui); D.num(id + " bsat", a[4], m->bsat_mT); D.num(id + " curie", a[5], m->curieC);
            QJsonArray mu = a[7].toArray(); D.num(id + " mu rows", a[6], (double)m->mu.size());
            for (int j = 0; j < mu.size() && j < (int)m->mu.size(); j++) for (int k = 0; k < 3; k++) D.num(id + " mu", mu[j].toArray()[k], m->mu[j][k]); }
        D.done();
    });
    t("golden: core database (352 cores incl. Amidon AL picks)", [&] {
        Diff D; QJsonArray cs = g["cores"].toArray(); REQ(cs.size() == (int)cores().size(), QString::number(cores().size()));
        for (int i = 0; i < cs.size(); i++) { QJsonArray a = cs[i].toArray(); const Core &c = cores()[i]; QString w = a[0].toString();
            int k = 0; D.str(w + " id", a[k++], c.id); D.str(w + " amidon", a[k++], c.amidon); D.str(w + " pn", a[k++], c.pn); D.str(w + " db", a[k++], c.db); D.str(w + " mat", a[k++], c.mat);
            for (double v : {c.od, c.idia, c.h, c.al, c.ae, c.le, c.ve, c.alTol[0], c.alTol[1]}) D.num(w + " #" + QString::number(k), a[k++], v);
            D.num(w + " alAmidon", a[k++], c.hasAlAmidon ? c.alAmidonValue : NaN); D.num(w + " alCoil64", a[k++], c.alCoil64); D.num(w + " alWeb", a[k++], c.alWeb); D.num(w + " alGeom", a[k++], c.alGeom);
            D.str(w + " alPick", a[k++], c.alPick); D.n++; if (a[k++].toBool() != c.alSuspect) D.err(w + " suspect", "", ""); D.n++; if (a[k++].toBool() != c.alDisagree) D.err(w + " disagree", "", "");
            D.str(w + " equiv", a[k++], c.equiv.has ? c.equiv.id : QString()); D.num(w + " equiv al", a[k++], c.equiv.has ? c.equiv.al : NaN);
            D.str(w + " src", a[k++], c.src); D.str(w + " color", a[k++], c.color); D.str(w + " freq", a[k++], c.freq); D.str(w + " computed", a[k++], c.computed.join(",")); D.str(w + " webUrl", a[k++], c.webUrl); }
        D.done();
    });
    t("golden: balun/unun designer - 363 designs, every number and text", [&] {
        Diff D;
        for (auto v : g["design"].toArray()) {
            QJsonObject o = v.toObject(); BalunInput in = BI(o["input"].toObject()); DesignResult d = design(in); QString w = inputTag(o["input"].toObject());
            D.num(w + " n1", o["n1"], d.n1); D.num(w + " n2", o["n2"], d.n2); D.num(w + " nMin", o["nMin"], d.nMin ? d.nMin : NaN); D.num(w + " target", o["target"], d.target);
            D.num(w + " lNom", o["lNom"], d.lNom); D.num(w + " lMin", o["lMin"], d.lMin); D.num(w + " lMax", o["lMax"], d.lMax);
            D.num(w + " zr", o["zr"], d.zAtFmin.r, 1e-7); D.num(w + " zx", o["zx"], d.zAtFmin.x, 1e-7);
            D.num(w + " lossPeak", o["lossPeakW"], d.lossPeakW, 1e-7); D.num(w + " lossAvg", o["lossAvgW"], d.lossAvgW, 1e-7); D.num(w + " eff", o["effPct"], d.efficiencyPct, 1e-7); D.num(w + " effWorst", o["effWorstPct"], d.efficiencyWorstPct, 1e-7);
            D.num(w + " lossDensity", o["lossDensity"], d.lossDensity_mWcm3, 1e-7); D.num(w + " tRise", o["tempRiseC"], d.tempRiseC, 1e-7); D.num(w + " tCore", o["coreTempC"], d.coreTempC, 1e-7);
            D.num(w + " stray", o["stray"], d.stray_pF); D.num(w + " passes", o["passes"], d.passes); D.num(w + " turnLen", o["turnLen"], d.turnLengthMM); D.num(w + " vHigh", o["vHighRms"], d.vHighRms);
            D.num(w + " lFull", o["lFullUH"], d.lFullUH); D.num(w + " compCap", o["compCap"], d.hasCompCap ? d.compCap.pF : NaN);
            QJsonArray pf = o["perFreq"].toArray();
            if (pf.size() != (int)d.perFreq.size()) D.err(w + " perFreq count", QString::number(pf.size()), QString::number(d.perFreq.size()));
            else for (int i = 0; i < pf.size(); i++) { QJsonArray a = pf[i].toArray(); const PerFreq &p = d.perFreq[i]; double vals[] = {p.fMHz, p.r, p.x, p.z, p.bPk_mT, p.bHeat_G, p.heatLimit_G, p.lossPeakW, p.lossMatchedW};
                for (int k = 0; k < 9; k++) D.num(w + " perFreq[" + QString::number(i) + "][" + QString::number(k) + "]", a[k], vals[k], 1e-7); }
            QJsonArray ws = o["wires"].toArray();
            if (ws.size() != (int)d.wires.size()) D.err(w + " wires", QString::number(ws.size()), QString::number(d.wires.size()));
            else for (int i = 0; i < ws.size(); i++) { QJsonArray a = ws[i].toArray(); D.str(w + " wire", a[0], d.wires[i].name); D.num(w + " wire turns", a[1], d.wires[i].turns); D.num(w + " wire len", a[2], d.wires[i].lengthMM); }
            D.txt(w, o, d);
        }
        D.done();
    });
    t("golden: coax traps - 150 cases", [&] {
        Diff D;
        for (auto v : g["coax"].toArray()) {
            QJsonObject o = v.toObject(); CoaxTrapResult r = coaxTrap(coaxInputFromJson(o["input"].toObject())); QString w = inputTag(o["input"].toObject());
            QList<QPair<const char *, double>> f = {{"n", r.n}, {"L", r.L_uH}, {"C", r.C_pF}, {"X", r.X}, {"fCheck", r.fCheckMHz}, {"fStub", r.fStubMHz}, {"fLumped", r.fLumpedMHz}, {"lNag", r.lNagaokaUH}, {"q", r.q}, {"rp", r.rpOhm},
                {"pdKey", r.pdKeyW}, {"pdAvg", r.pdAvgW}, {"cut", r.cutLenIn}, {"fQ", r.fQuarterMHz}, {"pitch", r.pitchIn}, {"meanDia", r.meanDiaIn}, {"wlen", r.windingLenIn}, {"coaxLen", r.coaxLenIn}, {"ld", r.ldRatio}};
            for (auto &x : f) D.num(w + " " + x.first, o[x.first], x.second, 1e-7);
            D.num(w + " vTrap.rms", o["vTrap"].toObject()["rms"], r.vTrap.rms); D.num(w + " vTrap.pk", o["vTrap"].toObject()["pk"], r.vTrap.pk);
            D.bands(w, o["bands"].toArray(), r.bands); D.txt(w, o, r);
        }
        D.done();
    });
    t("golden: LC traps - 58 cases (air coil and toroid)", [&] {
        Diff D;
        for (auto v : g["lc"].toArray()) {
            QJsonObject o = v.toObject(); LcTrapResult r = lcTrap(lcInputFromJson(o["input"].toObject())); QString w = inputTag(o["input"].toObject());
            QList<QPair<const char *, double>> f = {{"L", r.L_uH}, {"C", r.C_pF}, {"cIdeal", r.cIdealPF}, {"X", r.X}, {"q", r.q}, {"qCoil", r.qCoil}, {"rp", r.rpOhm}, {"cap", r.capRatingV}, {"ic", r.capCurrentA},
                {"pd", r.pdKeyW}, {"pdAvg", r.pdAvgW}, {"fcs", r.fWithCsMHz}};
            for (auto &x : f) D.num(w + " " + x.first, o[x.first], x.second, 1e-7);
            D.num(w + " vTrap.rms", o["vTrap"].toObject()["rms"], r.vTrap.rms);
            if (o["coil"].isArray() != r.hasCoil) D.err(w + " coil?", "", ""); else if (r.hasCoil) { QJsonArray a = o["coil"].toArray(); const LcCoil &c = r.coil;
                double vals[] = {c.N, c.pitchMM, c.DkMM, c.lenMM, c.esr, c.uH, c.csPF, c.srfMHz, c.q, c.wheelerUH, c.nagaokaRosaUH, c.wireLenMM}; for (int k = 0; k < 12; k++) D.num(w + " coil[" + QString::number(k) + "]", a[k], vals[k], 1e-7); }
            if (o["toroid"].isArray() != r.hasToroid) D.err(w + " toroid?", "", ""); else if (r.hasToroid) { QJsonArray a = o["toroid"].toArray(); const LcToroid &c = r.toroid;
                double vals[] = {(double)c.N, c.uH, c.qCore, c.qWire, c.wireLenMM, c.bGauss, c.limGauss, c.pCoreW, c.tempRiseC, c.coreTempC}; for (int k = 0; k < 10; k++) D.num(w + " toroid[" + QString::number(k) + "]", a[k], vals[k], 1e-7); }
            D.bands(w, o["bands"].toArray(), r.bands); D.txt(w, o, r);
        }
        D.done();
    });
    t("golden: end-fed long wire analysis, avoid zones and good lengths", [&] {
        Diff D;
        for (auto v : g["eflw"].toArray()) {
            QJsonObject o = v.toObject(); double len = o["len"].toDouble(), k = o["k"].toDouble(); QString w = QString("EFLW %1 ft k %2").arg(len).arg(k);
            EflwOpts op; op.k = k; auto rows = eflwAnalysis(len, op); QJsonArray jr = o["rows"].toArray();
            if (jr.size() != (int)rows.size()) { D.err(w + " rows", "", ""); continue; }
            for (int i = 0; i < jr.size(); i++) { QJsonArray a = jr[i].toArray(); const EflwRow &r = rows[i]; D.str(w + " band", a[0], r.band); D.num(w + " hw0", a[1], r.halfWaves[0]); D.num(w + " hw1", a[2], r.halfWaves[1]);
                D.num(w + " ll0", a[3], r.lambdaLo[0]); D.num(w + " ll1", a[4], r.lambdaLo[1]); D.str(w + " kinds", a[5], r.kinds.join(",")); D.str(w + " verdict " + r.band, a[6], r.verdict); D.str(w + " 9:1", a[7], r.unun9); D.str(w + " 49:1", a[8], r.unun49); }
            EflwOpts oz; oz.k = k; oz.tol = 0.05; auto zones = avoidZones(300, oz); QJsonArray jz = o["zones"].toArray();
            if (jz.size() != (int)zones.size()) D.err(w + " zones", QString::number(jz.size()), QString::number(zones.size()));
            else for (int i = 0; i < jz.size(); i++) { QJsonArray a = jz[i].toArray(); D.str(w + " zone", a[0], zones[i].band); D.num(w + " zone n", a[1], zones[i].n); D.num(w + " zone lo", a[2], zones[i].loFt); D.num(w + " zone hi", a[3], zones[i].hiFt); }
            EflwOpts og; og.k = k; for (const Band &b : bands()) if (b.id != "6m") og.bands.push_back(b);
            auto good = goodLengths(30, 300, og); QJsonArray jg = o["good"].toArray();
            if (jg.size() != (int)good.size()) D.err(w + " good", QString::number(jg.size()), QString::number(good.size()));
            else for (int i = 0; i < jg.size(); i++) { D.num(w + " good lo", jg[i].toArray()[0], good[i].first); D.num(w + " good hi", jg[i].toArray()[1], good[i].second); }
        }
        D.done();
    });
    t("golden: display units (AWG/mm/in x mixed/mm/in) - labels, dims and every text", [&] {
        Diff D; struct Restore { ~Restore() { setUnits("awg", "mixed"); } } restore;
        for (auto v : g["units"].toArray()) {
            QJsonObject o = v.toObject(); setUnits(o["wire"].toString(), o["len"].toString()); QString w = o["wire"].toString() + "/" + o["len"].toString();
            BalunInput i1 = presetById("ke9esu-auto91")->input; BalunInput i2; i2.topology = "efhw49"; i2.coreId = "A:FT-240-43"; i2.conductorId = "awg18e";
            QJsonArray ds = o["design"].toArray(); D.txt(w + " auto91", ds[0].toObject(), design(i1)); D.txt(w + " efhw49", ds[1].toObject(), design(i2));
            CoaxTrapInput ci; ci.coaxId = "custom"; D.txt(w + " coax", o["coax"].toObject(), coaxTrap(ci));
            LcTrapInput l1; l1.wireId = "tube5"; LcTrapInput l2; l2.coreType = "toroid"; QJsonArray ls = o["lc"].toArray(); D.txt(w + " lc air", ls[0].toObject(), lcTrap(l1)); D.txt(w + " lc toroid", ls[1].toObject(), lcTrap(l2));
            QJsonArray lb = o["labels"].toArray(); int k = 0; for (const Wire &c : conductors()) D.str(w + " label", lb[k++], wireLabel(c)); for (const Wire &c : coilWires()) D.str(w + " label", lb[k++], wireLabel(c));
            QJsonArray dm = o["dims"].toArray(); for (int j = 0; j < dm.size(); j++) D.str(w + " dims", dm[j], dimsTxt(cores()[j]));
            QJsonArray mm = o["mm"].toArray(); D.str(w + " mm0", mm[0], mmTxt(1245)); D.str(w + " mm1", mm[1], mmTxt(35.55, AS_STORED, 3)); D.str(w + " mm2", mm[2], mmTxt(10, 0, 1)); D.str(w + " mm3", mm[3], mmTxt(48.26, 1)); D.str(w + " mm4", mm[4], mmTxt(2.13, 2));
            QJsonArray in = o["inch"].toArray(); D.str(w + " in0", in[0], inchTxt(2.375, 3)); D.str(w + " in1", in[1], inchTxt(64.2, 1)); D.str(w + " in2", in[2], inchTxt(1, 0)); D.str(w + " in3", in[3], inchTxt(0.195, 3, S("″")));
            D.str(w + " dual", o["dual"], lenDual(1245));
        }
        D.done();
    });
    QJsonObject gc;
    t("golden: load golden_cc.json (Component Combiner solver.js reference outputs)", [&] { gc = loadJson(dir + "/golden_cc.json"); REQ1(gc["solve"].toArray().size() > 90); });
    if (gc.isEmpty()) return;
    t("golden: combiner parse/format", [&] {
        Diff D; for (auto v : gc["parse"].toArray()) { QJsonArray a = v.toArray(); D.num("parse " + a[0].toString(), a[1], cc::parseValue(a[0].toString())); }
        for (auto v : gc["fmt"].toArray()) { QJsonArray a = v.toArray(); D.str("fmt " + n2s(a[0].toDouble()), a[2], cc::fmt(a[0].toDouble(), a[1].toInt(), "F")); }
        D.done();
    });
    t("golden: combiner solve - 97 searches, every network and ranking identical", [&] {
        Diff D;
        for (auto v : gc["solve"].toArray()) {
            QJsonObject o = v.toObject(), p = o["p"].toObject(); cc::Params q = P(p["type"].toString()[0], p["target"].toDouble(), p["series"].toString(), p["maxParts"].toInt(), p["topo"].toString());
            for (auto x : p["custom"].toArray()) q.custom.push_back(x.toDouble());
            QString w = inputTag(p); cc::Solution s = cc::solve(q); QJsonArray rs = o["results"].toArray();
            if (rs.size() != (int)s.results.size()) { D.err(w + " count", QString::number(rs.size()), QString::number(s.results.size())); continue; }
            for (int i = 0; i < rs.size(); i++) { QJsonArray a = rs[i].toArray(); D.str(w + " expr", a[0], s.results[i].expr); D.num(w + " count", a[1], s.results[i].count); D.num(w + " value", a[2], s.results[i].value); D.num(w + " err", a[3], s.results[i].err, 1e-6); }
            auto acc = cc::sortResults(s.results, "accuracy", 1), sim = cc::sortResults(s.results, "simple", 1);
            QJsonArray ja = o["acc"].toArray(), js = o["simple"].toArray();
            for (int i = 0; i < ja.size(); i++) D.str(w + " acc#" + QString::number(i), ja[i], acc[i].expr);
            for (int i = 0; i < js.size(); i++) D.str(w + " simple#" + QString::number(i), js[i], sim[i].expr);
        }
        D.done();
    });
}


/* ---- shared RF project (the object that links the windows and the Coil64 main window) */
static void projectTests()
{
    t("project: values carry their source window, change signal, num()", [] {
        Project *p = Project::instance(); int n = 0; QString lastSrc;
        QObject ctx; QObject::connect(p, &Project::changed, &ctx, [&](const QString &, const QString &s) { n++; lastSrc = s; });
        p->set(Key::L, 34.4, Win::Balun); p->set(Key::L, 34.4, Win::Balun);   // unchanged: one signal
        REQ1(n == 1 && lastSrc == Win::Balun && p->num(Key::L) == 34.4 && p->source(Key::L) == Win::Balun);
        p->set(Key::F, 7.05, Win::Coil64Lc); REQ1(p->source(Key::F) == Win::Coil64Lc && n == 2);
    });
    t("project: JSON save/open round trip (shared values, sources, window states)", [] {
        Project *p = Project::instance();
        p->set(Key::Core, "FT240-43", Win::CoreDb); p->set(Key::Stack, 2, Win::CoreDb); p->set(Key::Power, 100, Win::Project);
        p->set(Key::Wire, QVariantMap{{"diaMM", 1.29}, {"odMM", 2.0}, {"label", "AWG 16 PTFE"}, {"id", "awg16t"}}, Win::Balun);
        p->set(Key::ProblemBands, QStringList{"80m", "40m"}, Win::Eflw);
        p->set(Key::Coil, QVariantMap{{"N", 10}, {"D", 25.0}, {"d", 1.0}, {"k", 1.1}, {"p", 1.5}, {"L", 2.5}, {"f", 7.1}}, Win::Coil64Coil);
        QJsonObject st; st.insert("preset", "ke9esu"); p->setState(Win::Balun, st);
        QTemporaryDir td; REQ1(td.isValid()); QString path = td.path() + "/t.rfproj.json"; QString e;
        REQ(p->saveFile(path, &e), e);
        p->set(Key::Core, "FT140-43", Win::Balun); p->set(Key::L, 1, Win::Lc); p->setState(Win::Balun, QJsonObject());
        REQ(p->openFile(path, &e), e);
        REQ1(p->get(Key::Core).toString() == "FT240-43" && p->source(Key::Core) == Win::CoreDb && p->num(Key::Stack) == 2 && p->num(Key::Power) == 100);
        REQ1(p->get(Key::Wire).toMap().value("diaMM").toDouble() == 1.29 && p->get(Key::Wire).toMap().value("id").toString() == "awg16t");
        REQ1(p->get(Key::ProblemBands).toStringList() == QStringList({"80m", "40m"}));
        REQ1(p->get(Key::Coil).toMap().value("L").toDouble() == 2.5 && p->source(Key::Coil) == Win::Coil64Coil);
        REQ1(p->state(Win::Balun).value("preset").toString() == "ke9esu");
        QFile f(path); REQ1(f.open(QIODevice::ReadOnly)); QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        REQ1(o.value("format").toString() == "KE9ESU Coil64 RF project" && o.contains("shared") && o.contains("windows"));
        REQ1(!p->openFile(td.path() + "/missing.json", &e) && !e.isEmpty());
    });
    t("project: units follow Coil64 Preferences (mm / cm / in / mil)", [] {
        Project *p = Project::instance(); Project::Units u; u.len = "in"; u.lenMul = 25.4; p->setUnits(u);
        REQ1(p->toDispLen(25.4) == 1 && p->fromDispLen(2) == 50.8 && p->lenUnit() == "in");
        u.len = "mil"; u.lenMul = 0.0254; p->setUnits(u); REQ1(std::fabs(p->toDispLen(1.29) - 50.787) < 1e-3);
        u.len = "mm"; u.lenMul = 1; p->setUnits(u); REQ1(p->toDispLen(12.7) == 12.7);
    });
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QString err; if (!initData(&err)) { std::printf("FAIL initData: %s\n", err.toUtf8().constData()); return 1; }
    QString dir = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString(KE9ESU_TEST_DIR);
    portedDesignerTests();
    portedCombinerTests();
    projectTests();
    goldenTests(dir);
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    if (g_fail) std::printf("Failed: %s\n", g_failed.join("; ").toUtf8().constData());
    return g_fail ? 1 : 0;
}
