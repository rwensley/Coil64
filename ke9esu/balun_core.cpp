/* balun_core.cpp - KE9ESU Balun & Unun Designer core, C++ port of core.js v1.3.1. See balun_core.h for credits.
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "balun_core.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
#include <functional>

static void initKe9esuResources()
{
    Q_INIT_RESOURCE(ke9esu);
}

namespace ke9esu {

const double K9YC_CS_PF = 1.3;
const double MAX_CURRENT_DENSITY = 5;   // A/mm2, bundled windings (rule of thumb)

namespace {
QMap<QString, Material> g_mat;
QStringList g_matOrder;
std::vector<Core> g_cores;
std::vector<Wire> g_cond;
std::vector<Band> g_bands;
std::vector<Topology> g_top;
std::vector<Preset> g_presets;
bool g_init = false;
Units g_units;

QString fr(const QString &pn) { return "Fair-Rite " + pn + " datasheet"; }

void addMat(const Material &m) { g_mat.insert(m.id, m); }

Material mkFerrite(const char *id, const char *name, double bsat, double curie)
{
    Material m; m.id = id; m.kind = "ferrite"; m.name = S(name); m.bsat_mT = bsat; m.curieC = curie; return m;
}

Core mkCore(const char *id, const char *amidon, const char *pn, const char *mat, double od, double idia, double h,
            double al, double t1, double t2, double ae, double le, double ve, const QString &src)
{
    Core c; c.id = id; c.amidon = S(amidon); c.pn = pn; c.mat = mat; c.od = od; c.idia = idia; c.h = h; c.al = al;
    c.alTol[0] = t1; c.alTol[1] = t2; c.ae = ae; c.le = le; c.ve = ve; c.src = src; return c;
}

bool readMuCsv(const QString &mix, Material &m)
{
    QFile f(":/ke9esu/mu/" + mix + ".csv");
    if (!f.open(QIODevice::ReadOnly)) return false;
    QString t = QString::fromLatin1(f.readAll());
    QStringList raw = t.split(QRegularExpression("\r\n|\r|\n")), lines;
    for (const QString &l : raw) if (!l.trimmed().isEmpty()) lines << l.trimmed();
    if (lines.size() < 5) return false;
    QStringList hdr = lines[2].split(',');
    m.mui = hdr.value(0).toDouble(); m.br = hdr.value(1).toDouble(); m.hc = hdr.value(2).toDouble();
    m.mu.clear();
    for (int i = 4; i < lines.size(); i++) {
        QStringList p = lines[i].split(',');
        if (p.size() < 3) continue;
        bool o1, o2, o3; double a = p[0].toDouble(&o1), b = p[1].toDouble(&o2), c = p[2].toDouble(&o3);
        if (o1 && o2 && o3) m.mu.push_back({a, b, c});
    }
    return !m.mu.empty();
}

double jnum(const QJsonValue &v) { return v.isDouble() ? v.toDouble() : (v.isString() ? v.toString().toDouble() : NaN); }

void addAmidon(const QJsonObject &AD)
{
    static const QMap<QString, double> IRON_Q = {{"-0", 300}, {"-1", 100}, {"-2", 150}, {"-3", 80}, {"-6", 175}, {"-7", 150}, {"-8", 100}, {"-10", 150}, {"-12", 120}, {"-15", 100}, {"-17", 150}, {"-18", 60}, {"-26", 40}, {"-30", 60}, {"-40", 40}, {"-52", 40}};
    QMap<QString, QJsonObject> web; QStringList webOrder;
    for (const QJsonValue &kv : AD["web"].toArray()) { QJsonArray a = kv.toArray(); web.insert(a[0].toString(), a[1].toObject()); webOrder << a[0].toString(); }
    const double IN = 25.4;
    QMap<QString, bool> seen;
    for (const QJsonValue &kv : AD["iron"].toArray()) {
        QJsonArray a = kv.toArray(); QString m = a[0].toString(); QJsonObject d = a[1].toObject(); QString id = "-" + m;
        if (!g_mat.contains(id)) {
            Material M; M.id = id; M.kind = "iron";
            M.name = "Amidon/Micrometals -" + m + " iron powder (" + d["type"].toString() + ", mu " + jsNum(jnum(d["mu"])) + ")";
            M.mui = jnum(d["mu"]); M.bsat_mT = m == "0" ? 1e6 : 1000; M.curieC = 770; M.q = IRON_Q.value(id, 100);
            addMat(M); g_matOrder << id;
        }
        Material &M = g_mat[id]; M.freq = d["freq"].toString(); M.color = d["color"].toString(); M.type = d["type"].toString(); M.maxTempC = 125;
    }
    for (const QJsonValue &kv : AD["ferrite"].toArray()) {
        QJsonArray a = kv.toArray(); QString m = a[0].toString(); QJsonObject f = a[1].toObject();
        if (g_mat.contains(m)) { Material &M = g_mat[m]; M.hasAmidon = true; M.amMui = jnum(f["mui"]); M.fResonant = f["fResonant"].toString(); M.fWideband = f["fWideband"].toString(); M.fSuppression = f["fSuppression"].toString(); }
    }
    auto make = [&](const QString &kind, const QString &mat, const QString &size, double odIn, double idIn, double hIn, double coilAL) {
        bool iron = kind == "iron";
        QString m2 = mat; int dash = m2.indexOf('-'); if (dash >= 0) m2.remove(dash, 1);   // JS mat.replace("-", "")
        QString name = (iron ? "T-" : "FT-") + size + "-" + m2;
        QString mid = iron ? "-" + m2 : mat;
        if (!g_mat.contains(mid) || seen.value(name)) return;
        bool hasW = web.contains(name); QJsonObject w = web.value(name);
        double od = odIn * IN, idia = idIn * IN, h = hIn * IN; Geometry g = geometry(od, idia, h);
        const Material &M = g_mat[mid];
        double geomNH = g.l0 * M.mui;
        struct Cand { double nh; QString from; };
        Cand cands[2] = {{coilAL > 0 ? alToNH(coilAL, iron) : NaN, "Coil64"}, {hasW ? alToNH(jnum(w["al"]), iron) : NaN, "amidoncorp.com"}};
        bool found = false; double bestNh = 0, bestE = 0; QString bestFrom;
        for (const Cand &c : cands) {
            if (!(c.nh > 0)) continue;
            double e = std::fabs(std::log(c.nh / geomNH));
            if (e > std::log(2.5)) continue;
            if (!found || e < bestE) { found = true; bestNh = c.nh; bestE = e; bestFrom = c.from; }
        }
        if (!found) return;
        seen[name] = true;
        Core c; c.id = "A:" + name; c.amidon = name; c.pn = "Amidon " + name; c.db = "Amidon"; c.mat = mid;
        c.od = toFixedNum(od, 2); c.idia = toFixedNum(idia, 2); c.h = toFixedNum(h, 2);
        c.al = toFixedNum(bestNh, 2);
        if (iron) { c.alTol[0] = 5; c.alTol[1] = 5; } else if (mid == "43" || mid == "31") { c.alTol[0] = 20; c.alTol[1] = 20; } else { c.alTol[0] = 25; c.alTol[1] = 25; }
        c.ae = toPrecisionNum(g.ae, 3); c.le = toPrecisionNum(g.le, 3); c.ve = toPrecisionNum(g.ae * g.le, 3);
        c.hasAlAmidon = true; c.alAmidonValue = toFixedNum(alFromNH(bestNh, iron), 1); c.alAmidonUnit = iron ? "uH/100T" : "nH/T2 (mH/1000T)";
        c.alCoil64 = coilAL > 0 ? alToNH(coilAL, iron) : NaN;
        c.alWeb = hasW ? alToNH(jnum(w["al"]), iron) : NaN;
        c.alGeom = toFixedNum(geomNH, 2); c.alPick = bestFrom;
        c.alSuspect = std::fabs(std::log(bestNh / geomNH)) > std::log(1.35);
        c.alDisagree = coilAL > 0 && hasW && std::fabs(alToNH(coilAL, iron) / alToNH(jnum(w["al"]), iron) - 1) > 0.05;
        c.webUrl = hasW ? w["url"].toString() : QString();
        c.freq = iron ? M.freq : (M.hasAmidon ? M.fWideband + " MHz wideband" : QString());
        c.color = iron ? M.color : QString();
        c.computed = QStringList{"Ae", "le", "Ve"};
        c.src = QString("Amidon catalog via Coil64 amidon.cpp") + (hasW ? "; amidoncorp.com product page" : "") + "; Ae/le/Ve computed from dimensions";
        g_cores.push_back(c);
    };
    auto sizes = [&](const QString &kind, const QString &key) {
        for (const QJsonValue &kv : AD[key].toArray()) {
            QJsonArray a = kv.toArray(); QString m = a[0].toString();
            for (const QJsonValue &sv : a[1].toObject()["sizes"].toArray()) {
                QJsonArray s = sv.toArray();
                double al = s[4].isNull() ? 0 : jnum(s[4]);
                make(kind, m, s[0].isString() ? s[0].toString() : jsNum(s[0].toDouble()), jnum(s[1]), jnum(s[2]), jnum(s[3]), al);
            }
        }
    };
    sizes("ferrite", "ferrite");
    sizes("iron", "iron");
    QRegularExpression re("^(FT|T)-(\\w+)-(\\d+)$");
    for (const QString &k : webOrder) {
        QRegularExpressionMatch mm = re.match(k); QJsonObject w = web[k];
        double wod = jnum(w["od"]);
        if (!mm.hasMatch() || !(wod != 0 && !std::isnan(wod))) continue;
        bool iron = mm.captured(1) == "T"; double wid = jnum(w["id"]);
        make(iron ? "iron" : "ferrite", mm.captured(3), mm.captured(2), std::max(wod, wid), std::min(wod, wid), jnum(w["h"]), 0);
    }
    auto same = [](const Core &a, const Core &b) { return a.mat == b.mat && std::fabs(a.od / b.od - 1) < 0.03 && std::fabs(a.idia / b.idia - 1) < 0.03 && std::fabs(a.h / b.h - 1) < 0.03; };
    for (Core &a : g_cores) {
        if (a.db != "Amidon") continue;
        for (Core &b : g_cores) if (b.db != "Amidon" && same(a, b)) {
            a.equiv.has = true; a.equiv.id = b.id; a.equiv.db = b.db; a.equiv.al = b.al;
            if (!b.equiv.has) { b.equiv.has = true; b.equiv.id = a.id; b.equiv.db = "Amidon"; b.equiv.al = a.al; }
        }
    }
}
} // anon

/* ------------------------------------------------------------------------------------------------ data */
bool initData(QString *error)
{
    if (g_init) return true;
    initKe9esuResources();
    g_mat.clear(); g_matOrder.clear(); g_cores.clear();
    // Ferrite and iron-powder materials. bsat_mT: Fair-Rite "flux density @ field strength"; curieC: Curie temperature.
    addMat(mkFerrite("31", "Fair-Rite 31 (MnZn, EMI/chokes 1-300 MHz)", 360, 130));
    addMat(mkFerrite("43", "Fair-Rite 43 (NiZn, broadband transformers/chokes)", 350, 130));
    addMat(mkFerrite("52", "Fair-Rite 52 (NiZn)", 420, 250));
    addMat(mkFerrite("61", "Fair-Rite 61 (NiZn, low loss to ~25 MHz)", 250, 300));
    addMat(mkFerrite("67", "Fair-Rite 67 (NiZn, low loss to ~50 MHz)", 230, 450));
    addMat(mkFerrite("77", "Fair-Rite 77 (MnZn, power <1 MHz; poor at HF)", 510, 200));
    addMat(mkFerrite("68", "Fair-Rite 68 (NiZn, low loss 80-180 MHz)", 250, 500));
    addMat(mkFerrite("75", "Fair-Rite 75 (MnZn, <1 MHz)", 470, 175));
    {   // Micrometals -2 carbonyl iron. CL[mW/cm3] = f/(a/B^3 + b/B^2.3 + c/B^1.65) + d f^2 B^2, B = peak gauss, f = Hz
        Material m; m.id = "-2"; m.kind = "iron"; m.name = "Micrometals/Amidon -2 carbonyl E iron (mu 10)"; m.mui = 10; m.bsat_mT = 1480; m.curieC = 770;
        m.hasLoss = true; m.la = 4.0e9; m.lb = 3.0e8; m.lc = 2.7e6; m.ld = 8.0e-15; m.q = 150; addMat(m);
    }
    g_matOrder = QStringList{"31", "43", "52", "61", "67", "68", "75", "77", "-2"};   // JS Object.keys order
    QString err;
    for (const QString &k : QStringList{"31", "43", "52", "61", "67", "68", "75", "77"})
        if (!readMuCsv(k, g_mat[k])) err += "missing Fair-Rite data for mix " + k + "; ";

    g_cores = {
        // Mix 43
        mkCore("FT37-43", "FT-37-43", "5943000201", "43", 9.5, 4.75, 3.175, 350, 20, 20, 0.072, 2.07, 0.15, fr("5943000201")),
        mkCore("FT50-43", "FT-50-43 (size equiv.)", "5943001101", "43", 12.7, 7.9, 6.35, 480, 20, 20, 0.15, 3.12, 0.47, fr("5943001101")),
        mkCore("FT82-43", "FT-82-43", "5943000601", "43", 21.0, 13.2, 6.35, 470, 20, 20, 0.243, 5.2, 1.26, fr("5943000601")),
        mkCore("FT114-43", "FT-114-43", "5943001001", "43", 29.0, 19.0, 7.5, 510, 20, 20, 0.37, 7.3, 2.7, fr("5943001001")),
        mkCore("FT140-43", "FT-140-43", "5943002701", "43", 35.55, 23.0, 12.7, 885, 20, 20, 0.78, 8.9, 7.0, fr("5943002701")),
        mkCore("FT240-43", "FT-240-43", "5943003801", "43", 61.0, 35.55, 12.7, 1075, 20, 20, 1.58, 14.5, 22.8, fr("5943003801")),
        mkCore("2643251002", "MM0OPX '100 W' core (39 mm, 43 mix)", "2643251002", "43", 39.1, 16.75, 22.2, NaN, 20, 20, NaN, NaN, NaN, fr("2643251002") + " (cable core: AL/Ae/le computed from dimensions)"),
        // Mix 52
        mkCore("FT240-52", "FT-240-52", "5952003801", "52", 61.0, 35.55, 12.7, 325, 25, 25, 1.58, 14.5, 22.8, fr("5952003801")),
        // Mix 61
        mkCore("FT37-61", "FT-37-61", "5961000201", "61", 9.5, 4.75, 3.175, 55, 25, 25, 0.073, 2.07, 0.15, fr("5961000201")),
        mkCore("FT50-61", "FT-50-61 (size equiv.)", "5961001101", "61", 12.7, 7.9, 6.35, 75, 25, 25, 0.15, 3.12, 0.47, fr("5961001101")),
        mkCore("FT82-61", "FT-82-61", "5961000601", "61", 21.0, 13.2, 6.35, 75, 25, 25, 0.243, 5.2, 1.26, fr("5961000601")),
        mkCore("FT114-61", "FT-114-61", "5961001001", "61", 29.0, 19.0, 7.5, 80, 25, 25, 0.37, 7.3, 2.7, fr("5961001001")),
        mkCore("FT140-61", "FT-140-61", "5961002701", "61", 35.55, 23.0, 12.7, 140, 25, 25, 0.78, 8.9, 7.0, fr("5961002701")),
        mkCore("FT240-61", "FT-240-61", "5961003801", "61", 61.0, 35.55, 12.7, 170, 25, 25, 1.58, 14.5, 22.8, fr("5961003801")),
        // Mix 67
        mkCore("FT240-67", "FT-240-67", "5967003801", "67", 61.0, 35.55, 12.7, 55, 35, 25, 1.58, 14.5, 22.8, fr("5967003801")),
        // Mix 77
        mkCore("FT82-77", "FT-82-77", "5977000601", "77", 21.0, 13.2, 6.35, 1270, 25, 25, 0.243, 5.2, 1.26, fr("5977000601")),
        mkCore("FT140-77", "FT-140-77", "5977002701", "77", 35.55, 23.0, 12.7, 2400, 25, 25, 0.78, 8.9, 7.0, fr("5977002701")),
        mkCore("FT240-77", "FT-240-77", "5977003801", "77", 61.0, 35.55, 12.7, 2950, 25, 25, 1.57, 14.4, 22.8, fr("5977003801")),
        // Mix 31 (sold as round cable cores: specified by impedance only, AL computed from dimensions)
        mkCore("2.4in-31", "FT-240-31 equiv. (K9YC '2.4-inch #31')", "2631803802", "31", 61.0, 35.55, 12.7, NaN, 20, 20, 1.58, 14.5, 22.8, fr("2631803802") + " (cable core: AL computed from dimensions)"),
        mkCore("2.4in-43", "FT-240-43 cable-core version", "2643803802", "43", 61.0, 35.55, 12.7, 1075, 20, 20, 1.58, 14.5, 22.8, fr("2643803802") + " (same size/material as 5943003801)"),
        // Iron powder (Micrometals catalog)
        mkCore("T130-2", "T-130-2", "T130-2", "-2", 33.02, 19.81, 11.10, 11, 5, 5, 0.698, 8.28, 5.78, "Micrometals catalog (T130 size row, -2 AL 11)"),
        mkCore("T200-2", "T-200-2", "T200-2", "-2", 50.80, 31.75, 13.97, 12, 5, 5, 1.27, 13.0, 16.4, "Micrometals T200-2 datasheet")
    };
    // Fill computed values for cable cores
    for (Core &c : g_cores) {
        Geometry g = geometry(c.od, c.idia, c.h); const Material &m = g_mat[c.mat];
        c.computed.clear(); c.db = m.kind == "iron" ? "Micrometals" : "Fair-Rite";
        if (std::isnan(c.al)) { c.al = jsRound(g.l0 * m.mui); c.computed << "AL"; }
        if (std::isnan(c.ae)) { c.ae = toFixedNum(g.ae, 3); c.computed << "Ae"; }
        if (std::isnan(c.le)) { c.le = toFixedNum(g.le, 2); c.computed << "le"; }
        if (std::isnan(c.ve)) { c.ve = toFixedNum(c.ae * c.le, 2); c.computed << "Ve"; }
    }
    QFile af(":/ke9esu/amidon_ordered.json");
    if (af.open(QIODevice::ReadOnly)) addAmidon(QJsonDocument::fromJson(af.readAll()).object());
    else err += "missing Amidon data; ";

    // conductors: od = overall insulated diameter mm; cu = copper area mm2; ratedW ~ continuous at 30 MHz (coax)
    auto W = [](const char *id, const char *label, const char *kind, double od, double cu, const char *jacket, double ratedW = 0) {
        Wire w; w.id = id; w.label = S(label); w.kind = kind; w.od = od; w.cu = cu; w.jacket = jacket; w.ratedW = ratedW; return w; };
    g_cond = {
        W("awg22e", "AWG 22 enamelled", "wire", 0.70, 0.326, "enamel"), W("awg20e", "AWG 20 enamelled", "wire", 0.87, 0.518, "enamel"),
        W("awg18e", "AWG 18 enamelled", "wire", 1.09, 0.823, "enamel"), W("awg16e", "AWG 16 enamelled", "wire", 1.37, 1.31, "enamel"),
        W("awg14e", "AWG 14 enamelled", "wire", 1.71, 2.08, "enamel"),
        W("awg22t", "AWG 22 PTFE hook-up (~1.2 mm OD)", "wire", 1.22, 0.326, "PTFE"), W("awg20t", "AWG 20 PTFE hook-up (~1.4 mm OD)", "wire", 1.40, 0.518, "PTFE"),
        W("awg18t", "AWG 18 PTFE hook-up (~1.7 mm OD)", "wire", 1.70, 0.823, "PTFE"), W("awg16t", "AWG 16 PTFE hook-up (~2.0 mm OD)", "wire", 2.00, 1.31, "PTFE"),
        W("awg14t", "AWG 14 PTFE hook-up (~2.4 mm OD)", "wire", 2.40, 2.08, "PTFE"),
        W("cat5", "Cat5 pair conductor, AWG 24 PE (~0.95 mm)", "wire", 0.95, 0.205, "PE"),
        W("rg316", "RG-316 coax (PTFE)", "coax", 2.5, NaN, "PTFE", 150), W("rg174", "RG-174 coax", "coax", 2.8, NaN, "PVC", 70),
        W("rg400", "RG-400 coax (PTFE, double shield)", "coax", 4.95, NaN, "PTFE", 900), W("rg142", "RG-142 coax (PTFE)", "coax", 4.95, NaN, "PTFE", 700),
        W("rg58", "RG-58 coax", "coax", 4.95, NaN, "PVC", 350), W("rg8x", "RG-8X coax", "coax", 6.1, NaN, "PVC", 450)
    };
    g_bands = {{"160m", 1.8, 2.0}, {"80m", 3.5, 4.0}, {"60m", 5.3305, 5.4065}, {"40m", 7.0, 7.3}, {"30m", 10.1, 10.15}, {"20m", 14.0, 14.35},
               {"17m", 18.068, 18.168}, {"15m", 21.0, 21.45}, {"12m", 24.89, 24.99}, {"10m", 28.0, 29.7}, {"6m", 50.0, 54.0}};
    g_top = {
        {"choke11", "1:1 current balun / common-mode choke", "choke", 1, 2, 1, 0, 1},
        {"guanella41", "4:1 Guanella (current) balun", "choke", 4, 2, 2, 0, 0.5},
        {"ruthroff41", "4:1 Ruthroff (voltage) unun", "transformer", 4, 2, 1, 0, 1},
        {"ruthroff91", "9:1 Ruthroff trifilar unun (random/long wire)", "transformer", 9, 3, 1, 0, 1},
        {"auto91", "9:1 autotransformer unun (single tapped winding, 1:3 turns)", "transformer", 9, 1, 1, 3, 1},
        {"efhw49", "49:1 EFHW autotransformer (2450 ohm)", "transformer", 49, 1, 1, 7, 1},
        {"efhw64", "64:1 EFHW autotransformer (3200 ohm)", "transformer", 64, 1, 1, 8, 1}
    };
    auto P = [](const char *id, const char *label, std::function<void(BalunInput &)> f, QStringList keys) {
        Preset p; p.id = id; p.label = S(label); p.input = BalunInput(); p.input.turns = NaN; f(p.input); p.keys = keys; return p; };
    g_presets = {
        P("ke9esu", "KE9ESU: 9:1 Ruthroff unun, 4 trifilar turns AWG 16 PTFE on 2 × FT240-43, 124 ft EFLW, 160–6 m, 100 W (default)", [](BalunInput &i) { i.topology = "ruthroff91"; i.coreId = "FT240-43"; i.stack = 2; i.conductorId = "awg16t"; i.turns = 4; i.fMinMHz = 1.8; i.fMaxMHz = 54; i.powerW = 100; i.vswr = 3; i.duty = "digital"; }, {}),
        P("ke9esu-choke", "KE9ESU: feedline choke at the 9:1, 16 turns RG-400 on 2.4\" #31, 160–6 m", [](BalunInput &i) { i.topology = "choke11"; i.coreId = "2.4in-31"; i.stack = 1; i.conductorId = "rg400"; i.turns = 16; i.fMinMHz = 1.8; i.fMaxMHz = 54; i.powerW = 100; i.vswr = 3; i.duty = "digital"; i.chokeTarget = 5000; }, {}),
        P("g3txq-t200", "G3TXQ test: 9:1 on T200-2, 9 trifilar turns, 80–10 m", [](BalunInput &i) { i.topology = "ruthroff91"; i.coreId = "T200-2"; i.stack = 1; i.conductorId = "awg18t"; i.turns = 9; i.fMinMHz = 3.5; i.fMaxMHz = 29.7; i.powerW = 100; i.vswr = 3; i.duty = "ssb"; }, {}),
        P("ke9esu-auto91", "KE9ESU: 9:1 autotransformer (single tapped winding) for 124 ft EFLW, 160–6 m, 100 W (FT240-43 ×2)", [](BalunInput &i) { i.topology = "auto91"; i.coreId = "FT240-43"; i.stack = 2; i.conductorId = "awg16t"; i.fMinMHz = 1.8; i.fMaxMHz = 54; i.powerW = 100; i.vswr = 3; i.duty = "digital"; }, {}),
        P("efhw49-80", "EFHW 49:1, 80–10 m, 100 W (FT240-43 ×2)", [](BalunInput &i) { i.topology = "efhw49"; i.coreId = "FT240-43"; i.stack = 2; i.conductorId = "awg18e"; i.fMinMHz = 3.5; i.fMaxMHz = 29.7; i.powerW = 100; i.vswr = 2; i.duty = "ssb"; }, {}),
        P("efhw49-40", "EFHW 49:1, 40–10 m, 100 W SSB (FT240-43, 2:14)", [](BalunInput &i) { i.topology = "efhw49"; i.coreId = "FT240-43"; i.stack = 1; i.conductorId = "awg18e"; i.turns = 2; i.fMinMHz = 7; i.fMaxMHz = 29.7; i.powerW = 100; i.vswr = 2; i.duty = "ssb"; }, {}),
        P("efhw64", "EFHW 64:1, 40–10 m, 100 W", [](BalunInput &i) { i.topology = "efhw64"; i.coreId = "FT240-43"; i.stack = 1; i.conductorId = "awg18e"; i.fMinMHz = 7; i.fMaxMHz = 29.7; i.powerW = 100; i.vswr = 2; i.duty = "ssb"; }, {}),
        P("guanella", "Guanella 4:1 balun, 80–10 m, 100 W", [](BalunInput &i) { i.topology = "guanella41"; i.coreId = "FT140-43"; i.stack = 2; i.conductorId = "awg18t"; i.fMinMHz = 3.5; i.fMaxMHz = 29.7; i.powerW = 100; i.vswr = 3; i.duty = "ssb"; i.chokeTarget = 500; }, {}),
        P("ruthroff4", "Ruthroff 4:1 unun, 80–10 m, 100 W", [](BalunInput &i) { i.topology = "ruthroff41"; i.coreId = "FT240-43"; i.stack = 1; i.conductorId = "awg16t"; i.fMinMHz = 3.5; i.fMaxMHz = 29.7; i.powerW = 100; i.vswr = 3; i.duty = "ssb"; }, {})
    };
    g_init = true;
    if (error) *error = err;
    return err.isEmpty();
}

const QMap<QString, Material> &materials() { initData(); return g_mat; }
QStringList materialOrder() { initData(); return g_matOrder; }
const Material *material(const QString &id) { initData(); auto it = g_mat.constFind(id); return it == g_mat.constEnd() ? nullptr : &it.value(); }
const std::vector<Core> &cores() { initData(); return g_cores; }
const Core *coreById(const QString &id) { initData(); for (const Core &c : g_cores) if (c.id == id) return &c; return nullptr; }
const std::vector<Wire> &conductors() { initData(); return g_cond; }
const Wire *conductorById(const QString &id) { initData(); for (const Wire &w : g_cond) if (w.id == id) return &w; return &g_cond[2]; }
const std::vector<Band> &bands() { initData(); return g_bands; }
const std::vector<Topology> &topologies() { initData(); return g_top; }
const Topology *topology(const QString &id) { initData(); for (const Topology &t : g_top) if (t.id == id) return &t; return nullptr; }
const std::vector<Preset> &presets() { initData(); return g_presets; }
const Preset *presetById(const QString &id) { initData(); for (const Preset &p : g_presets) if (p.id == id) return &p; return nullptr; }
double dutyPercentFor(const QString &duty, bool *known)
{
    double v = duty == "ssb" ? 30 : duty == "cw" ? 50 : duty == "digital" ? 100 : NaN;
    if (known) *known = !std::isnan(v);
    return v;
}

double alToNH(double v, bool iron) { return iron ? v / 10 : v; }      // Amidon catalog number -> nH/T^2
double alFromNH(double nh, bool iron) { return iron ? nh * 10 : nh; } // nH/T^2 -> Amidon catalog number

/* ------------------------------------------------------------------------------------------------ physics */
Geometry geometry(double od, double id, double h)
{
    double ln = std::log(od / id);
    double l0 = MU0 * (h * 1e-3) * ln / (2 * PI) * 1e9;     // nH/T^2 at mu_r = 1
    double le = 2 * PI * ln / (2 / id - 2 / od) / 10;      // cm  (C1^2/C2 effective path)
    double ae = (h * (od - id) / 2) / 100;                  // cm2 (rectangular approx)
    return {ae, le, ae * le, l0};
}

Complex2 permeability(const QString &matId, double fHz)
{
    const Material &m = g_mat[matId];
    if (m.kind == "iron") return {m.mui, m.mui / m.q};
    const auto &t = m.mu; int n = (int)t.size();
    if (fHz <= t[0][0]) return {t[0][1], t[0][2]};
    if (fHz >= t[n - 1][0]) return {t[n - 1][1], t[n - 1][2]};
    int lo = 0, hi = n - 1;
    while (hi - lo > 1) { int mid = (lo + hi) >> 1; if (t[mid][0] <= fHz) lo = mid; else hi = mid; }
    const auto &a = t[lo], &b = t[hi]; double u = std::log(fHz / a[0]) / std::log(b[0] / a[0]);
    return {a[1] + u * (b[1] - a[1]), a[2] + u * (b[2] - a[2])};
}

double inductanceUH(const Core &core, double N, double stack, int tol)
{
    double k = 1;
    if (tol > 0) k = 1 + core.alTol[0] / 100; else if (tol < 0) k = 1 - core.alTol[1] / 100;
    return core.al * k * N * N * (stack ? stack : 1) / 1000;
}

double selfCapacitancePF(const Core &core, double N, double stack)
{
    double h = core.h * (stack ? stack : 1), od = core.od, id = core.idia, iR = id / 2, oR = od / 2;
    double C1 = 2 * PI / (h * std::log(od / id));
    double lp = od - id + 2 * h;
    double le = 2 * PI * std::log(od / id) / (1 / iR - 1 / oR);
    double A = (1.13e-3 * C1 * lp * lp) / (0.4 * PI);
    double B = (1.13e-1 * C1 * std::pow(0.9 * le, 2)) / (0.4 * PI);
    return A + B / (N * N);
}

Impedance windingImpedance(const Core &core, double N, double fHz, double stack, double csPF)
{
    const Material &m = g_mat[core.mat]; Complex2 mu = permeability(core.mat, fHz);
    double w = 2 * PI * fHz;
    double k = w * N * N * core.al * 1e-9 * (stack ? stack : 1) / m.mui;
    double r = k * mu.im, x = k * mu.re;
    if (csPF > 0) {   // Y = 1/(r+jx) + jwC
        double d = r * r + x * x, gr = r / d, bi = -x / d + w * csPF * 1e-12;
        double d2 = gr * gr + bi * bi;
        r = gr / d2; x = -bi / d2;
    }
    return {r, x, std::hypot(r, x)};
}

double fluxDensity_mT(double vRms, double fHz, double N, double aeCm2) { return SQRT2 * vRms / (2 * PI * fHz * N * aeCm2 * 1e-4) * 1000; }
double heatingFluxLimitGauss(double fMHz) { return 150 / std::sqrt(std::max(fMHz, 0.1)); }
double ironCoreLoss_mWcm3(const QString &matId, double fHz, double bGauss)
{
    const Material &c = g_mat[matId]; double B = std::max(bGauss, 1e-6);
    return fHz / (c.la / std::pow(B, 3) + c.lb / std::pow(B, 2.3) + c.lc / std::pow(B, 1.65)) + c.ld * fHz * fHz * B * B;
}
double coreSurfaceCm2(const Core &core, double stack)
{
    double od = core.od / 10, id = core.idia / 10, h = core.h / 10 * (stack ? stack : 1);
    return PI / 4 * (od * od - id * id) * 2 + PI * (od + id) * h;
}
double tempRiseC(double lossW, double surfaceCm2) { return lossW <= 0 ? 0 : std::pow(lossW * 1000 / surfaceCm2, 0.833); }
double mismatchFactor(double vswr) { double s = std::max(vswr, 1.0); return 2 * s / (s + 1); }
double turnLengthMM(const Core &core, double stack, double condOd) { return 2 * (core.h * (stack ? stack : 1) + (core.od - core.idia) / 2) + 4 * condOd; }
Fit windingFit(const Core &core, double condOd, double passes)
{
    int perLayer = (int)std::max(1.0, std::floor(PI * (core.idia - condOd) / condOd));
    return {perLayer, (int)std::ceil(passes / perLayer), passes * condOd * condOd / (core.idia * core.idia)};
}
CompCap compensationCap(double fTopMHz, double powerW, double vswr)
{
    double f = std::min(std::max(fTopMHz, 7.0), 30.0);
    double pF = 1e12 / (2 * PI * f * 1e6 * 55);
    pF = std::min(220.0, std::max(47.0, pF));
    const double stdv[] = {47, 56, 68, 82, 100, 120, 150, 180, 220}; double best = stdv[0];
    for (double s : stdv) if (std::fabs(s - pF) < std::fabs(best - pF)) best = s;
    double vPk = std::sqrt(2 * powerW * 50) * mismatchFactor(vswr);
    double need = 5 * vPk; const double ratings[] = {500, 1000, 2000, 3000, 5000, 15000}; double rating = ratings[5];
    for (double r : ratings) if (r >= need) { rating = r; break; }
    CompCap c; c.pF = best; c.pFcalc = pF; c.rangePF[0] = jsRound(best * 0.7); c.rangePF[1] = jsRound(best * 1.5); c.vPeak = vPk; c.ratingV = rating;
    return c;
}

double clampv(double v, double a, double b, double d) { if (!std::isfinite(v)) v = d; return std::min(b, std::max(a, v)); }

BalunInput normalize(const BalunInput &i)
{
    initData();
    BalunInput o = i;
    if (!topology(o.topology)) o.topology = BalunInput().topology;
    if (!coreById(o.coreId)) o.coreId = BalunInput().coreId;
    o.stack = jsRound(clampv(o.stack, 1, 4, 1));
    o.zLow = clampv(o.zLow, 1, 1000, 50); o.powerW = clampv(o.powerW, 0.1, 5000, 100); o.vswr = clampv(o.vswr, 1, 20, 1);
    o.fMinMHz = clampv(o.fMinMHz, 0.1, 500, 1.8); o.fMaxMHz = clampv(o.fMaxMHz, 0.1, 500, 30);
    if (o.fMaxMHz < o.fMinMHz) o.fMaxMHz = o.fMinMHz;
    bool known; double dp = dutyPercentFor(o.duty, &known);
    if (known) o.dutyPercent = dp; else o.dutyPercent = clampv(o.dutyPercent, 1, 100, 50);
    o.xlFactor = clampv(o.xlFactor, 1, 20, 5); o.chokeTarget = clampv(o.chokeTarget, 100, 1e5, 5000);
    o.cmPercent = clampv(o.cmPercent, 0.1, 100, 10); o.leadMM = clampv(o.leadMM, 0, 2000, 100); o.ambientC = clampv(o.ambientC, -20, 60, 30);
    o.turns = std::isnan(o.turns) ? NaN : jsRound(clampv(o.turns, 1, 60, 1));
    const Topology *t = topology(o.topology);
    if (t->kind == "transformer" && conductorById(o.conductorId)->kind == "coax") o.conductorId = "awg16t";
    return o;
}

std::vector<double> logSpace(double a, double b, int n)
{
    std::vector<double> out; double la = std::log(a), lb = std::log(b);
    for (int i = 0; i < n; i++) out.push_back(std::exp(la + (lb - la) * i / (n - 1)));
    return out;
}

std::vector<Band> bandsInRange(double fMinMHz, double fMaxMHz)
{
    std::vector<Band> out;
    for (const Band &b : bands()) if (b.hi >= fMinMHz * 0.999 && b.lo <= fMaxMHz * 1.001) out.push_back(b);
    return out;
}

std::vector<double> evalFrequencies(double fMin, double fMax)
{
    std::vector<double> fs = {fMin, fMax};
    for (const Band &b : bandsInRange(fMin, fMax)) { fs.push_back(std::max(b.lo, fMin)); fs.push_back(std::min(b.hi, fMax)); fs.push_back((b.lo + b.hi) / 2); }
    std::vector<double> f2;
    for (double f : fs) if (f >= fMin * 0.999 && f <= fMax * 1.001) f2.push_back(f);
    std::stable_sort(f2.begin(), f2.end());
    std::vector<double> out;
    for (size_t i = 0; i < f2.size(); i++) if (i == 0 || f2[i] - f2[i - 1] > 1e-6 * f2[i]) out.push_back(f2[i]);
    return out;
}

double strayPF(const BalunInput &inp, const Core &core, double N)
{
    if (inp.csMode == "manual") return std::max(0.0, std::isnan(inp.csPF) ? 0.0 : inp.csPF);
    if (inp.csMode == "none") return 0;
    if (inp.csMode == "g3rbj") return selfCapacitancePF(core, N, inp.stack);
    return K9YC_CS_PF;
}

static QString f1(double v, int d = 1) { return toFixed(v, d); }

DesignResult design(const BalunInput &raw)
{
    initData();
    DesignResult R;
    BalunInput inp = normalize(raw);
    const Topology &top = *topology(inp.topology); const Core &core = *coreById(inp.coreId); const Material &mat = g_mat[core.mat];
    const Wire &cond = *conductorById(inp.conductorId); double stack = inp.stack, fMinHz = inp.fMinMHz * 1e6;
    bool isChoke = top.kind == "choke";
    double target = isChoke ? inp.chokeTarget : inp.xlFactor * inp.zLow;
    auto metric = [&](int N, double fHz) {
        if (isChoke) return windingImpedance(core, N, fHz, stack, strayPF(inp, core, N)).z;
        return windingImpedance(core, N, fHz, stack, 0).x;
    };
    int nMin = 0;
    for (int N = 1; N <= 60; N++) { if (metric(N, fMinHz) >= target) { nMin = N; break; } }
    int n1 = !std::isnan(inp.turns) ? (int)inp.turns : (nMin ? nMin : 60);
    int n2 = top.turnsRatio ? n1 * top.turnsRatio : 0;
    bool useCoax = isChoke && cond.kind == "coax";
    int wiresPerTurn = (isChoke && useCoax) ? 1 : top.wires;

    double mf = mismatchFactor(inp.vswr);
    double vIn = std::sqrt(inp.powerW * inp.zLow);       // RMS, matched
    double vInWorst = vIn * mf;                          // RMS, worst-case SWR
    double iIn = std::sqrt(inp.powerW / inp.zLow) * mf;
    double vCm = (inp.cmPercent / 100) * vIn;
    const double Z_CM_PATH = 100;
    double aeTot = core.ae * stack, veTot = core.ve * stack * top.coreSets;
    double cs = isChoke ? strayPF(inp, core, n1) : 0;
    double duty = inp.dutyPercent / 100;

    for (double fMHz : evalFrequencies(inp.fMinMHz, inp.fMaxMHz)) {
        double fHz = fMHz * 1e6; Impedance z = windingImpedance(core, n1, fHz, stack, isChoke ? cs : 0);
        double vW, pW = 0, pM = 0, bPk, bHeat;
        if (isChoke) {
            double icm = vCm / std::hypot(z.r + Z_CM_PATH, z.x);
            vW = icm * z.z; pW = icm * icm * z.r * top.coreSets;
            bPk = fluxDensity_mT(vW * mf, fHz, n1, aeTot); bHeat = fluxDensity_mT(vW, fHz, n1, aeTot);
        } else {
            bPk = fluxDensity_mT(vInWorst, fHz, n1, aeTot); bHeat = fluxDensity_mT(vIn, fHz, n1, aeTot);
            if (mat.kind == "iron" && mat.hasLoss) { pW = ironCoreLoss_mWcm3(core.mat, fHz, bHeat * 10 * mf) * veTot / 1000; pM = ironCoreLoss_mWcm3(core.mat, fHz, bHeat * 10) * veTot / 1000; }
            else { pW = vInWorst * vInWorst * z.r / (z.r * z.r + z.x * z.x); pM = vIn * vIn * z.r / (z.r * z.r + z.x * z.x); }
        }
        R.perFreq.push_back({fMHz, z.r, z.x, z.z, bPk, bHeat * 10, heatingFluxLimitGauss(fMHz), pW, isChoke ? pW : pM});
    }
    PerFreq worst = R.perFreq[0], bMax = R.perFreq[0], worstM = R.perFreq[0];
    for (const PerFreq &b : R.perFreq) { if (b.lossPeakW > worst.lossPeakW) worst = b; }
    for (const PerFreq &b : R.perFreq) { if (b.bPk_mT > bMax.bPk_mT) bMax = b; }
    double lossPeakW = worst.lossPeakW, lossAvgW = lossPeakW * duty;
    double lossDensity = lossAvgW * 1000 / veTot;   // mW/cm3
    double surf = coreSurfaceCm2(core, stack) * top.coreSets;
    double dT = tempRiseC(lossAvgW, surf), coreTemp = inp.ambientC + dT;
    for (const PerFreq &b : R.perFreq) { if (b.lossMatchedW > worstM.lossMatchedW) worstM = b; }
    double effPct = isChoke ? NaN : std::max(0.0, 100 * (1 - worstM.lossMatchedW / inp.powerW));
    double effWorstPct = isChoke ? NaN : std::max(0.0, 100 * (1 - lossPeakW / inp.powerW));

    Impedance zFmin = windingImpedance(core, n1, fMinHz, stack, isChoke ? cs : 0);
    double lNom = inductanceUH(core, n1, stack, 0), lMin = inductanceUH(core, n1, stack, -1), lMax = inductanceUH(core, n1, stack, 1);
    double xlMinLF = 2 * PI * fMinHz * lMin * 1e-6;

    double turnLen = turnLengthMM(core, stack, cond.od);
    int passes = top.turnsRatio ? n2 : n1 * wiresPerTurn;
    Fit fit = windingFit(core, cond.od, passes);
    std::vector<WireCut> wires;
    if (top.turnsRatio) wires.push_back({"Winding (tapped at turn " + QString::number(n1) + ")", n2, n2 * turnLen + 2 * inp.leadMM + 30});
    else if (top.id == "guanella41") { for (int g = 0; g < 2; g++) for (int w2 = 0; w2 < wiresPerTurn; w2++) wires.push_back({"Line " + QString::number(g + 1) + " wire " + QString("AB")[w2], n1, n1 * turnLen + 2 * inp.leadMM}); }
    else for (int w = 0; w < wiresPerTurn; w++) wires.push_back({useCoax ? QString("Coax") : "Wire " + QString(QString("ABC")[w]), n1, n1 * turnLen + 2 * inp.leadMM});

    double iWire = top.turnsRatio ? iIn : iIn * top.currentFactor;
    CondLoad condLoad = cond.kind == "coax" ? CondLoad{inp.powerW * std::max(inp.vswr, 1.0) / (top.id == "guanella41" ? 2 : 1), cond.ratedW, "W"}
                                            : CondLoad{iWire / cond.cu, MAX_CURRENT_DENSITY, "A/mm2"};
    double vHighRms = vInWorst * std::sqrt(top.ratio);
    bool hasCap = top.id == "efhw49" || top.id == "efhw64";
    CompCap cap{}; if (hasCap) cap = compensationCap(std::min(inp.fMaxMHz, 30.0), inp.powerW, inp.vswr);

    /* checks */
    std::vector<Check> &checks = R.checks;
    auto add = [&](const QString &id, const QString &status, const QString &text) { checks.push_back({id, status, text}); };
    QStringList L;
    if (isChoke) {
        if (zFmin.z >= target) add("target", "ok", "|Z| at " + f1(inp.fMinMHz, 2) + " MHz is " + rnd(zFmin.z) + S(" Ω (R ") + rnd(zFmin.r) + S(" Ω), meets the ") + jsNum(target) + S(" Ω target (K9YC suggests ~5 kΩ resistive)."));
        else add("target", "fail", "|Z| at " + f1(inp.fMinMHz, 2) + " MHz is only " + rnd(zFmin.z) + S(" Ω, below the ") + jsNum(target) + S(" Ω target. Add turns or stack cores."));
        QStringList weak; bool anyLow = false;
        for (const PerFreq &p : R.perFreq) if (p.z < target) { weak << f1(p.fMHz, 2) + " MHz (" + rnd(p.z) + S(" Ω)"); if (p.z < 1000) anyLow = true; }
        if (!weak.isEmpty()) add("bands", anyLow ? "warn" : "info", "Below target at: " + weak.join(", ") + ".");
        QStringList reactive;
        for (const PerFreq &p : R.perFreq) if (std::fabs(p.x) > p.r) reactive << f1(p.fMHz, 2);
        if (!reactive.isEmpty()) add("reactive", "info", "Mostly reactive (|X| > R) at " + reactive.join(", ") + S(" MHz — G3TXQ: reactive chokes can resonate with the feedline and choke less than |Z| suggests."));
        if (cs > 0) {
            QString why = inp.csMode == "manual" ? "your value" : inp.csMode == "g3rbj" ? "G3RBJ/Coil64 close-wound estimate" : "fitted to K9YC measurements";
            add("cs", "info", "Stray capacitance " + f1(cs, 1) + " pF assumed (" + why + "); self-resonance near " + f1(selfResonanceMHz(core, n1, stack, cs), 1) + " MHz (where X crosses zero).");
        }
    } else {
        double ratio = zFmin.x / inp.zLow;
        QString txt = "Primary reactance at " + f1(inp.fMinMHz, 2) + " MHz: " + rnd(zFmin.x) + S(" Ω = ") + f1(ratio, 1) + S("× ") + jsNum(inp.zLow) + S(" Ω (L = ") + f1(lNom, 2) + S(" µH, ") + f1(lMin, 2) + S("–") + f1(lMax, 2) + S(" µH with AL tolerance).");
        if (ratio >= inp.xlFactor && xlMinLF / inp.zLow >= 4) add("xl", "ok", txt);
        else if (ratio >= 4) add("xl", "warn", txt + S(" Meets 4× but not your ") + jsNum(inp.xlFactor) + S("× target, or a low-tolerance core could drop below 4×."));
        else add("xl", "fail", txt + S(" Below 4×: add primary turns or stack cores."));
        if (mat.kind == "ferrite") {
            Complex2 muTop = permeability(core.mat, inp.fMaxMHz * 1e6);
            if (muTop.im > muTop.re) add("lossy", "warn", "Mix " + core.mat + " is lossy at " + f1(inp.fMaxMHz, 1) + S(" MHz (µ'' ") + rnd(muTop.im) + S(" > µ' ") + rnd(std::max(muTop.re, 0.0)) + "); expect extra loss at the top end.");
        }
    }
    double sat = bMax.bPk_mT / mat.bsat_mT;
    QString satTxt = "Peak flux (saturation, PEP, SWR " + jsNum(inp.vswr) + ":1) " + f1(bMax.bPk_mT, 1) + " mT = " + f1(sat * 100, 1) + "% of " + jsNum(mat.bsat_mT) + " mT at " + f1(bMax.fMHz, 2) + " MHz.";
    add("sat", sat <= 0.1 ? "ok" : sat <= 0.25 ? "warn" : "fail", satTxt);
    QStringList hv;
    for (const PerFreq &p : R.perFreq) if (p.bHeat_G > p.heatLimit_G) hv << f1(p.fMHz, 2) + " MHz (" + rnd(p.bHeat_G) + " G > " + rnd(p.heatLimit_G) + " G)";
    if (!hv.isEmpty()) add("heatflux", "warn", S("Flux above the Amidon/DL5SWB heating guideline (~150/√f G) at ") + hv.join(", ") + ".");
    else add("heatflux", "ok", "Flux stays under the Amidon/DL5SWB heating guideline at all evaluated frequencies.");
    QString ldTxt = "Average core loss " + f1(lossAvgW, 2) + " W (" + jsNum(jsRound(lossPeakW * 100) / 100) + " W key-down at " + f1(worst.fMHz, 2) + S(" MHz × ") + jsNum(inp.dutyPercent) + "% duty) = " + rnd(lossDensity) + S(" mW/cm³ (Fair-Rite: ≤300 conservative, ≤500 max).");
    add("lossd", lossDensity <= 300 ? "ok" : lossDensity <= 500 ? "warn" : "fail", ldTxt);
    QString tTxt = "Estimated temperature rise " + rnd(dT) + S(" °C → core about ") + rnd(coreTemp) + S(" °C at ") + jsNum(inp.ambientC) + S(" °C ambient (Curie ") + jsNum(mat.curieC) + S(" °C; Amidon: keep ≤75 °C continuous, ≤100 °C intermittent).");
    add("temp", coreTemp <= 75 ? "ok" : coreTemp <= 100 ? "warn" : "fail", tTxt);
    if (!std::isnan(effPct)) add("eff", effPct >= 90 ? "ok" : effPct >= 80 ? "warn" : "fail", S("Core (magnetizing) efficiency into a matched load ≈ ") + f1(effPct, 1) + "% (" + f1(-10 * std::log10(std::max(1e-6, effPct / 100)), 2) + " dB) at " + f1(worstM.fMHz, 2) + " MHz, the worst evaluated frequency; " + f1(effWorstPct, 1) + "% with the voltage raised by SWR " + jsNum(inp.vswr) + S(":1. For scale: DG1JAN measured ~76–78% total for an FT240-43 2:15 EFHW (this model gives ~74% for 2 turns at 7 MHz)."));
    if (condLoad.unit == "W") add("cond", condLoad.value <= condLoad.limit ? "ok" : "fail", "Coax carries up to " + rnd(condLoad.value) + " W at SWR " + jsNum(inp.vswr) + ":1; " + cond.label + " is rated about " + jsNum(condLoad.limit) + " W at 30 MHz.");
    else add("cond", condLoad.value <= condLoad.limit ? "ok" : "fail", "Conductor current up to " + f1(iWire, 2) + " A RMS = " + f1(condLoad.value, 1) + S(" A/mm² (limit ") + jsNum(condLoad.limit) + ").");
    QString fitTxt = QString::number(passes) + " conductor passes; about " + QString::number(fit.perLayer) + " fit in one layer around the " + mmTxt(core.idia, AS_STORED, 3) + S(" hole → ") + QString::number(fit.layers) + " layer(s), " + rnd(fit.fill * 100) + "% of the hole area.";
    add("fit", fit.layers <= 1 && fit.fill <= 0.35 ? "ok" : fit.fill <= 0.55 ? "warn" : "fail", fitTxt);
    if (!top.turnsRatio && !isChoke) add("hv", "info", S("High-impedance side ≈ ") + rnd(vHighRms) + " V RMS (" + rnd(vHighRms * SQRT2) + " V peak) at worst-case SWR.");
    if (top.turnsRatio) add("hv", vHighRms * SQRT2 > 500 && cond.jacket != "PTFE" ? "warn" : "info", S("Antenna terminal ≈ ") + rnd(vHighRms) + " V RMS (" + rnd(vHighRms * SQRT2) + " V peak) at worst-case SWR" + (cond.jacket != "PTFE" ? S(" — use PTFE/FEP sleeving or tape at the hot end and keep it clear of the core edge and enclosure.") : QString(".")));
    if (cond.jacket == "PVC") add("jacket", "warn", S("PVC insulation has ~10× the dielectric loss of PTFE/FEP/PE and softens with heat; prefer PTFE or FEP (G3TXQ / ham.stackexchange 9:1 notes)."));
    else if (cond.jacket == "enamel" && top.wires > 1 && !useCoax) add("jacket", "info", "Multi-filar windings work better with PTFE/FEP insulated wire: it sets the line impedance and adds breakdown margin.");
    if (!std::isnan(inp.turns) && nMin && inp.turns < nMin) add("manual", "warn", "Manual turns (" + jsNum(inp.turns) + ") are below the " + QString::number(nMin) + " needed for the target.");
    if (nMin == 0 && std::isnan(inp.turns)) add("nmin", "fail", S("Target not reachable with up to 60 turns on this core — use a bigger/higher-permeability core or stack."));
    if (top.id == "guanella41") add("z0", "info", "Each line should be ~" + jsNum(2 * inp.zLow) + S(" Ω (twisted pair or 100 Ω line). 50 Ω coax works but narrows bandwidth."));
    if (top.id == "auto91") {
        double lFull = lNom * top.turnsRatio * top.turnsRatio;
        add("auto", "info", "Autotransformer 1:" + QString::number(top.turnsRatio) + ": tap section " + QString::number(n1) + S(" turns (50 Ω side) = ") + f1(lNom, 2) + S(" µH, full winding ") + QString::number(n2) + " turns (" + jsNum(9 * inp.zLow) + S(" Ω side) = ") + f1(lFull, 1) + S(" µH (L ∝ N², ×9). Rule of thumb: tap-section XL ≥ 4–5 × ") + jsNum(inp.zLow) + S(" Ω at the lowest frequency (Cromwell; ARRL handbook practice)."));
        add("leak", "info", S("High-frequency limit: unlike the trifilar Ruthroff (a transmission-line transformer), a single tapped winding couples its sections only through the core, so leakage inductance and inter-turn/inter-section capacitance set the upper limit — keep the winding to one layer, spread evenly, and expect SWR to rise on 10–6 m; verify with a ") + jsNum(9 * inp.zLow) + S(" Ω test load."));
        add("split", "info", S("Typical published splits: 3:9 turns on FT140-43 or FT240-43, 4:12 on FT82-43 or stacked FT240-43 (hamradiobase table; Cromwell used 8 tap turns on FT140-43 for extra low-band margin). More turns help 160/80 m but add capacitance that hurts 10–6 m."));
        CompCap cOpt = compensationCap(std::min(inp.fMaxMHz, 30.0), inp.powerW, inp.vswr);
        add("comp", "info", S("Optional compensation capacitor across the SO-239 (as on the 49:1): usually not needed on a 9:1. If SWR into the test load climbs on 10–6 m, try values up to the 49:1 starting value (") + jsNum(cOpt.pF) + S(" pF, ≥ ") + jsNum(cOpt.ratingV) + " V) and keep what flattens the high end.");
    }
    if (top.id == "ruthroff91" || top.id == "ruthroff41") add("line", "info", "Ruthroff: wind the wires as a tight, evenly twisted bundle. The core carries the full signal flux, so it runs warmer than a Guanella (ham.stackexchange 9:1 notes; G3TXQ unun experiments).");
    if (inp.fMaxMHz > 30 && !isChoke) add("6m", "info", "Above 30 MHz (6 m) most ferrite transformers lose efficiency and match quality; check 6 m on the analyser or use a separate 6 m antenna.");

    R.input = inp; R.topology = top; R.core = &core; R.material = &mat; R.conductor = &cond;
    R.n1 = n1; R.n2 = n2; R.nMin = nMin; R.target = target; R.zAtFmin = zFmin; R.lNom = lNom; R.lMin = lMin; R.lMax = lMax;
    R.worstLoss = worst; R.bMax = bMax; R.lossPeakW = lossPeakW; R.lossAvgW = lossAvgW; R.efficiencyWorstPct = effWorstPct;
    R.lossDensity_mWcm3 = lossDensity; R.tempRiseC = dT; R.coreTempC = coreTemp; R.efficiencyPct = effPct; R.stray_pF = cs;
    R.fit = fit; R.passes = passes; R.turnLengthMM = turnLen; R.wires = wires; R.conductorLoad = condLoad; R.vHighRms = vHighRms;
    R.lFullUH = top.turnsRatio ? inductanceUH(core, n1, stack, 0) * top.turnsRatio * top.turnsRatio : NaN;
    R.hasCompCap = hasCap; R.compCap = cap; R.coresNeeded = (int)stack * top.coreSets;
    R.steps = connectionSteps(top, n1, n2, useCoax, inp);
    R.parts = partsList(top, core, stack, cond, wires, hasCap ? &cap : nullptr);
    return R;
}

QStringList connectionSteps(const Topology &top, int n1, int n2, bool coax, const BalunInput &inp)
{
    QString N1 = QString::number(n1), N2 = QString::number(n2);
    if (top.id == "choke11") {
        if (coax) return {"Wind the coax through the core(s) for " + N1 + " turns, spread evenly with a gap between start and finish (crossing back over the core is fine and lowers stray capacitance).",
                          S("Radio side: normal coax connector. Antenna side: connector or centre → antenna, shield → counterpoise."),
                          "Secure turns with cable ties; do not let the winding bunch up."};
        return {"Lay wires A and B side by side (bifilar) and wind " + N1 + " turns through the core(s).",
                S("Source hot → start of A, source ground → start of B."),
                "Ends of A and B go to the two load terminals."};
    }
    if (top.id == "guanella41") return {
        "Make two identical lines: L1 (wires A1, B1) and L2 (A2, B2), each " + N1 + S(" bifilar turns on its own core set; line Z0 ≈ ") + jsNum(2 * inp.zLow) + S(" Ω."),
        S("Inputs in parallel with L2 reversed: source hot → starts of A1 and B2; source ground → starts of B1 and A2."),
        "Outputs in series: join the ends of B1 and B2. The ends of A1 and A2 are the balanced " + jsNum(4 * inp.zLow) + S(" Ω output.")};
    if (top.id == "ruthroff41") return {
        "Wind wires A and B together (bifilar), " + N1 + " turns.",
        S("Source hot → start of A and end of B."),
        "Join start of B, source ground and load ground.",
        "Load hot (" + jsNum(4 * inp.zLow) + S(" Ω side) → end of A.")};
    if (top.id == "ruthroff91") return {
        "Twist wires A, B and C into an even trifilar bundle and wind " + N1 + " turns.",
        S("Source hot (SO-239 centre) → end of C and start of B."),
        "Join end of B to start of A.",
        "Join start of C, SO-239 shell, ground/counterpoise terminal.",
        S("End of A → antenna wire terminal (") + jsNum(9 * inp.zLow) + S(" Ω side).")};
    if (top.id == "auto91") return {
        "Wind " + N2 + " turns of ONE wire in the same direction, spread evenly around the core(s) (not trifilar). Make a tap at turn " + N1 + " counted from the start: scrape the insulation/enamel for ~" + mmTxt(10, 0, 1) + " and solder a short lead, or break the wire there and join both ends to the tap lead.",
        S("Start of the winding (turn 0) → SO-239 shell and the ground/counterpoise post."),
        "Tap at turn " + N1 + S(" → SO-239 centre (50 Ω side: turns 0–") + N1 + ", the low-impedance section).",
        "End of the winding (turn " + N2 + S(") → antenna wire terminal (") + jsNum(9 * inp.zLow) + S(" Ω side: the full winding)."),
        "Check on the analyser with a " + jsNum(9 * inp.zLow) + S(" Ω (e.g. 2 × 910 Ω in parallel, non-inductive) resistor from antenna post to ground post: SWR at the SO-239 should stay below ≈1.5–2:1 across the bands you use.")};
    return {
        "Wind " + N2 + " turns of one wire in the same direction around the core(s). Bunch the first " + N1 + " turns tightly; spread the rest evenly (G4AON: first half tight, second half spaced).",
        S("Start of the winding → SO-239 shell and the ground/counterpoise post."),
        "Scrape the enamel and tap at turn " + N1 + S(" → SO-239 centre pin (the 50 Ω input)."),
        "End of the winding (turn " + N2 + S(") → antenna wire post. Sleeve the last turns with PTFE."),
        S("Compensation capacitor from SO-239 centre to shell (see parts list); tune 47–150 pF on the analyser for best 10–15 m SWR."),
        "Alternative: a separate " + N1 + "-turn primary twisted over the first turns of the secondary (common in published builds)."};
}

std::vector<Part> partsList(const Topology &top, const Core &core, double stack, const Wire &cond, const std::vector<WireCut> &wires, const CompCap *cap)
{
    std::vector<Part> p = {{stack * top.coreSets, core.id + " core (" + core.pn + ", " + dimsTxt(core) + ")"}};
    for (const WireCut &w : wires) p.push_back({1, wireLabel(cond) + S(" — ") + w.name + ": " + lenDual(w.lengthMM) + " incl. leads"});
    if (cap) p.push_back({1, jsNum(cap->pF) + S(" pF capacitor, ≥ ") + jsNum(cap->ratingV) + S(" V (e.g. C0G/NP0 ceramic or silver mica), across the 50 Ω input")});
    p.push_back({1, "SO-239 (or BNC) chassis connector"});
    if (top.kind == "transformer") p.push_back({2, "Wing-nut/binding posts: antenna and ground/counterpoise"});
    p.push_back({1, "Weatherproof non-metal enclosure, cable ties, PTFE tape/sleeving"});
    return p;
}

double selfResonanceMHz(const Core &core, double N, double stack, double csPF)
{
    std::vector<double> fs = logSpace(0.1, 500, 400); bool hasPrev = false; double prev = 0;
    for (size_t i = 0; i < fs.size(); i++) {
        double x = windingImpedance(core, N, fs[i] * 1e6, stack, csPF).x;
        if (hasPrev && prev > 0 && x <= 0) {
            double lo = fs[i - 1], hi = fs[i];
            for (int k = 0; k < 40; k++) { double mid = std::sqrt(lo * hi); if (windingImpedance(core, N, mid * 1e6, stack, csPF).x > 0) lo = mid; else hi = mid; }
            return std::sqrt(lo * hi);
        }
        prev = x; hasPrev = true;
    }
    return NaN;
}

std::vector<CurvePoint> impedanceCurve(const Core &core, double N, double stack, double csPF, double f1MHz, double f2MHz, int n)
{
    std::vector<CurvePoint> out;
    for (double f : logSpace(f1MHz, f2MHz, n ? n : 200)) { Impedance z = windingImpedance(core, N, f * 1e6, stack, csPF); out.push_back({f, z.r, z.x, z.z}); }
    return out;
}

/* ------------------------------------------------------------------------------------------------ display units */
Units setUnits(const QString &wire, const QString &len)
{
    if (wire == "awg" || wire == "mm" || wire == "in") g_units.wire = wire;
    if (len == "mixed" || len == "mm" || len == "in") g_units.len = len;
    return g_units;
}
Units getUnits() { return g_units; }
QString dispUnit(const QString &canon) { return g_units.len == "mixed" ? canon : g_units.len; }

QString mmTxt(double mm, int dMM, int dIn)
{
    if (dMM == AS_STORED) { if (dispUnit("mm") == "in") return toFixed(mm / 25.4, dIn != UNDEF ? dIn : 3) + " in"; return jsNum(mm) + " mm"; }
    if (dMM == UNDEF) dMM = 0;
    if (dispUnit("mm") == "in") return toFixed(mm / 25.4, dIn != UNDEF ? dIn : std::min(3, dMM + 2)) + " in";
    return dMM ? toFixed(mm, dMM) + " mm" : rnd(mm) + " mm";
}
QString inchTxt(double inch, int dIn, const QString &mark, int dMM)
{
    if (dispUnit("in") == "mm") {
        double m = inch * 25.4; int d = dMM != UNDEF ? dMM : std::max(0, (dIn == UNDEF ? 0 : dIn) - 1);
        return (d ? toFixed(m, d) : rnd(m)) + " mm";
    }
    return toFixed(inch, dIn == UNDEF ? 0 : dIn) + (mark.isEmpty() ? QString("\"") : mark);
}
QString lenDual(double mm)
{
    if (g_units.len == "mm") return rnd(mm) + " mm";
    if (g_units.len == "in") return toFixed(mm / 25.4, 1) + " in";
    return rnd(mm) + " mm (" + toFixed(mm / 25.4, 1) + " in)";
}
QString dimsTxt(const Core &c)
{
    if (dispUnit("mm") == "in") return toFixed(c.od / 25.4, 3) + S("×") + toFixed(c.idia / 25.4, 3) + S("×") + toFixed(c.h / 25.4, 3) + " in";
    return jsNum(c.od) + S("×") + jsNum(c.idia) + S("×") + jsNum(c.h) + " mm";
}
double wireDiaMM(const Wire &w)
{
    if (w.kind == "coax") return NaN;
    if (!std::isnan(w.dia) && w.dia) return w.dia;
    return (!std::isnan(w.cu) && w.cu) ? std::sqrt(4 * w.cu / PI) : NaN;
}
QString wireSize(const Wire &w)
{
    QRegularExpressionMatch m = QRegularExpression("AWG (\\d+)").match(w.label); double d = wireDiaMM(w);
    if (g_units.wire == "awg" && m.hasMatch()) return "AWG " + m.captured(1);
    if (std::isnan(d)) return QString();
    return g_units.wire == "in" || (g_units.wire == "awg" && dispUnit("mm") == "in") ? S("Ø") + toFixed(d / 25.4, 3) + " in" : S("Ø") + toFixed(d, 2) + " mm";
}
QString wireLabel(const Wire &w)
{
    QString lab = w.label;
    if (w.kind == "coax") return lab;
    QRegularExpressionMatch awg = QRegularExpression("AWG (\\d+)").match(lab), tube = QRegularExpression("^([\\d/]+)\" copper tubing").match(lab);
    auto odTxt = [](QString txt) {
        QRegularExpression re("~([\\d.]+) mm( OD)?");
        QRegularExpressionMatch m = re.match(txt);
        if (!m.hasMatch()) return txt;
        QString v = m.captured(1), od = m.captured(2);
        QString rep = "~" + (dispUnit("mm") == "in" ? mmTxt(v.toDouble(), AS_STORED, 3) : v + " mm") + od;
        return txt.left(m.capturedStart()) + rep + txt.mid(m.capturedEnd());
    };
    if (tube.hasMatch()) return g_units.wire == "awg" ? lab : wireSize(w) + " copper tubing (" + tube.captured(1) + "\" nominal)";
    if (g_units.wire == "awg") return dispUnit("mm") == "mm" ? lab : odTxt(lab);
    if (!awg.hasMatch()) return lab;
    QString l2 = lab; QRegularExpressionMatch m2 = QRegularExpression("AWG \\d+").match(l2);
    l2 = l2.left(m2.capturedStart()) + wireSize(w) + l2.mid(m2.capturedEnd());
    return odTxt(l2);
}

} // namespace ke9esu
