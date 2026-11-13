/* combiner_core.cpp - see combiner_core.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "combiner_core.h"
#include "ke9esu_util.h"
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QMap>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace ke9esu {
namespace cc {

static const std::vector<double> E6 = {1.0, 1.5, 2.2, 3.3, 4.7, 6.8};
static const std::vector<double> E12 = {1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9, 4.7, 5.6, 6.8, 8.2};
static const std::vector<double> E24 = {1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1};
static std::vector<double> e192()
{
    std::vector<double> out;
    for (int i = 0; i < 192; i++) { double v = jsRound(std::pow(10, i / 192.0) * 100) / 100; if (i == 185) v = 9.20; out.push_back(v); }  // IEC 60063 exception
    return out;
}

const PartType &partType(QChar type)
{
    static const PartType R{"Resistor", QString::fromUtf8("\u03A9"), 1, 1e7}, C{"Capacitor", "F", 1e-12, 1e-2}, L{"Inductor", "H", 1e-9, 1};
    return type == 'C' ? C : type == 'L' ? L : R;
}

std::vector<double> eSeries(const QString &name, bool *ok)
{
    QString d = name; d.remove(QRegularExpression("\\D")); int n = d.toInt();
    if (ok) *ok = true;
    if (n == 6) return E6; if (n == 12) return E12; if (n == 24) return E24;
    if (n == 48 || n == 96 || n == 192) { static const std::vector<double> E = e192(); std::vector<double> out; int stride = 192 / n; for (int i = 0; i < 192; i += stride) out.push_back(E[i]); return out; }
    if (ok) *ok = false;
    return {};
}

double parseValue(const QString &str)
{
    QString s = str.trimmed(); s.remove(QRegularExpression("\\s+")); s.replace(QRegularExpression(QString::fromUtf8("[\u00B5\u03BC]")), "u");
    if (s.isEmpty()) return NaN;
    s.replace(QRegularExpression(QString::fromUtf8("(ohms?|\u03A9|\u2126)$"), QRegularExpression::CaseInsensitiveOption), "");
    s.replace(QRegularExpression("^(.*[0-9a-zA-Z])[FfHh]$"), "\\1");
    s.replace(QRegularExpression("^(.*[0-9])MEG$", QRegularExpression::CaseInsensitiveOption), "\\1meg");
    QRegularExpressionMatch m = QRegularExpression("^([0-9]*\\.?[0-9]+(?:[eE][+-]?[0-9]+)?)(meg|[pnumkKMGR])?([0-9]*)$").match(s);
    if (!m.hasMatch()) return NaN;
    QString num = m.captured(1), pre = m.captured(2), frac = m.captured(3);
    if (!frac.isEmpty()) { if (pre.isEmpty() || num.contains(QRegularExpression("[.eE]"))) return NaN; num = num + "." + frac; }
    static const QMap<QString, double> MULT = {{"p", 1e-12}, {"n", 1e-9}, {"u", 1e-6}, {"m", 1e-3}, {"k", 1e3}, {"K", 1e3}, {"M", 1e6}, {"meg", 1e6}, {"G", 1e9}, {"R", 1}};
    double v = num.toDouble() * (pre.isEmpty() ? 1 : MULT.value(pre));
    return std::isfinite(v) && v > 0 ? v : NaN;
}

ParsedList parseList(const QString &text)
{
    ParsedList r;
    for (const QString &t : text.split(QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts)) { double v = parseValue(t); if (std::isnan(v)) r.bad << t; else r.values.push_back(v); }
    return r;
}

static double clean(double v) { return toPrecisionNum(v, 12); }

std::vector<double> buildLeaves(QChar type, const QString &seriesName, const std::vector<double> &custom)
{
    if (!custom.empty()) {
        std::vector<double> u = custom; std::sort(u.begin(), u.end()); std::vector<double> out;
        for (size_t i = 0; i < u.size(); i++) if (i == 0 || std::fabs(u[i] - u[i - 1]) > 1e-12 * u[i]) out.push_back(u[i]);
        return out;
    }
    const PartType &t = partType(type); std::vector<double> base = eSeries(seriesName), out;
    int d0 = (int)std::floor(std::log10(t.min)) - 1, d1 = (int)std::ceil(std::log10(t.max)) + 1;
    for (int d = d0; d <= d1; d++) for (double b : base) {
        double v = clean(b * std::pow(10, d));
        if (v >= t.min * (1 - 1e-9) && v <= t.max * (1 + 1e-9)) out.push_back(v);
    }
    return out;
}

QString fmt(double v, int digits, const QString &unit)
{
    static const std::pair<double, const char *> PREF[] = {{1e9, "G"}, {1e6, "M"}, {1e3, "k"}, {1, ""}, {1e-3, "m"}, {1e-6, "\u00B5"}, {1e-9, "n"}, {1e-12, "p"}, {1e-15, "f"}};
    const int NP = 9; if (!digits) digits = 4;
    if (!std::isfinite(v)) return jsNum(v);
    for (int i = 0; i < NP; i++) {
        double m = v / PREF[i].first;
        if (std::fabs(m) >= 1 - 1e-12 || i == NP - 1) {
            double r = toPrecisionNum(m, digits);
            if (std::fabs(r) >= 1000 && i > 0) { i -= 1; r = toPrecisionNum(v / PREF[i].first, digits); }
            return jsNum(r) + (unit.isEmpty() ? "" : " ") + QString::fromUtf8(PREF[i].second) + unit;
        }
    }
    return jsNum(v);
}

namespace {
struct Level { int n = 0; std::vector<double> v; std::vector<unsigned char> op, la, rb; std::vector<int> li, ri; };
struct Cand { double x, ae, err; int op, a, i, b, j; };
}

Solution solve(const Params &p, const std::function<bool(const QString &, double)> &progress)
{
    QElapsedTimer t0; t0.start();
    Solution sol;
    QChar type = p.type; double target = p.target; int maxParts = std::max(1, std::min(6, p.maxParts));
    QString topo = p.topo.isEmpty() ? "mixed" : p.topo;
    bool allowS = topo != "parallel", allowP = topo != "series";
    if (!(target > 0)) { sol.error = "Target must be a positive value"; return sol; }
    bool isC = type == 'C';
    auto tx = [&](double v) { return isC ? 1 / v : v; };
    double T = tx(target);
    std::vector<double> phys = buildLeaves(type, p.series.isEmpty() ? "E12" : p.series, p.custom);
    double W = p.window ? p.window : 1e4, lo = T / W, hi = T * W;
    std::vector<std::pair<double, double>> leafPairs;
    for (double v : phys) { double x = tx(v); if (x >= lo && x <= hi) leafPairs.push_back({x, v}); }
    std::stable_sort(leafPairs.begin(), leafPairs.end(), [](const std::pair<double, double> &a, const std::pair<double, double> &b) { return a.first < b.first; });
    int nL = (int)leafPairs.size();
    if (!nL) { sol.error = "No usable part values near that target"; return sol; }
    std::vector<double> leafPhys; for (auto &a : leafPairs) leafPhys.push_back(a.second);
    std::vector<Level> levels(7);
    Level &L1 = levels[1]; L1.n = nL; L1.v.resize(nL); L1.op.assign(nL, 0); L1.la.assign(nL, 0); L1.li.resize(nL); L1.rb.assign(nL, 0); L1.ri.assign(nL, 0);
    for (int i = 0; i < nL; i++) { L1.v[i] = leafPairs[i].first; L1.li[i] = i; }
    int nb = (int)std::max(20000.0, std::min(200000.0, jsRound((p.budget ? p.budget : 3e7) / nL)));
    double lnLo = std::log(lo), invD = nb / std::log(hi / lo);
    int THIN = p.thin ? p.thin : 1500;
    double total = 1, done = 0;
    for (int k = 2; k <= maxParts - 1; k++) total += (k == 2 ? nL * (double)nL / 2 : nL * std::min((double)nb, (double)nL * nL)) + (k >= 4 ? (double)THIN * THIN : 0);
    total += maxParts * (double)nb * 2;
    auto report = [&](const QString &ph, double fr) { return progress ? progress(ph, fr) : true; };
    if (!report("start", 0)) { sol.error = "aborted"; return sol; }

    for (int k = 2; k <= maxParts - 1; k++) {
        std::vector<double> bv(nb, 0.0); std::vector<unsigned char> bop(nb), bla(nb), brb(nb); std::vector<int> bli(nb), bri(nb);
        for (int a = 1; a <= (k >> 1); a++) {
            int b = k - a; const Level &A = levels[a], &B = levels[b];
            int sa = a >= 2 ? std::max(1, (int)std::ceil(A.n / (double)THIN)) : 1;
            int sb = a >= 2 ? std::max(1, (int)std::ceil(B.n / (double)THIN)) : 1;
            int Bn = B.n; double ops = 0;
            for (int i = 0; i < A.n; i += sa) {
                double x = A.v[i]; int j0 = a == b ? i : 0;
                for (int j = j0; j < Bn; j += sb) {
                    double y = B.v[j], v; int idx;
                    if (allowS) {
                        v = x + y;
                        if (v <= hi) {
                            idx = (int)((std::log(v) - lnLo) * invD);
                            if (idx >= 0 && idx < nb && bv[idx] == 0) { bv[idx] = v; bop[idx] = 1; bla[idx] = a; bli[idx] = i; brb[idx] = b; bri[idx] = j; }
                        } else if (!allowP) break;
                    }
                    if (allowP) {
                        v = x * y / (x + y);
                        if (v >= lo) {
                            idx = (int)((std::log(v) - lnLo) * invD);
                            if (idx >= 0 && idx < nb && bv[idx] == 0) { bv[idx] = v; bop[idx] = 2; bla[idx] = a; bli[idx] = i; brb[idx] = b; bri[idx] = j; }
                        }
                    }
                }
                ops += (Bn - j0) / (double)sb;
                if (ops > 3e6) { done += ops; ops = 0; if (!report("Building " + QString::number(k) + "-part networks", std::min(0.99, done / total))) { sol.error = "aborted"; return sol; } }
            }
            done += ops;
        }
        Level &Lk = levels[k];
        for (int i = 0; i < nb; i++) if (bv[i] != 0) { Lk.v.push_back(bv[i]); Lk.op.push_back(bop[i]); Lk.la.push_back(bla[i]); Lk.li.push_back(bli[i]); Lk.rb.push_back(brb[i]); Lk.ri.push_back(bri[i]); }
        Lk.n = (int)Lk.v.size();
        if (!report("Built " + QString::number(k) + "-part table (" + QString::number(Lk.n) + " values)", std::min(0.99, done / total))) { sol.error = "aborted"; return sol; }
    }

    const int KEEP = 12;
    std::vector<std::vector<Cand>> tops(maxParts + 1);
    auto lower = [](const std::vector<double> &arr, int n, double y) { int l = 0, h = n; while (l < h) { int mid = (l + h) >> 1; if (arr[mid] < y) l = mid + 1; else h = mid; } return l; };
    auto consider = [&](std::vector<Cand> &list, double r, int op, int a, int i, int b, int j) {
        double pv = isC ? 1 / r : r, err = (pv - target) / target, ae = std::fabs(err);
        if ((int)list.size() >= KEEP && ae >= list.back().ae) return;
        for (const Cand &c : list) if (std::fabs(c.x - r) <= 1e-12 * r) return;
        list.push_back({r, ae, err, op, a, i, b, j});
        std::stable_sort(list.begin(), list.end(), [](const Cand &u, const Cand &w) { return u.ae < w.ae; });
        if ((int)list.size() > KEEP) list.pop_back();
    };
    for (int m = 1; m <= maxParts; m++) {
        std::vector<Cand> list;
        if (m == 1) {
            int q = lower(L1.v, nL, T);
            for (int d = q - 2; d <= q + 1; d++) if (d >= 0 && d < nL) consider(list, L1.v[d], 0, 1, d, 0, 0);
        } else {
            for (int a = 1; a <= (m >> 1); a++) {
                int b = m - a; const Level &A = levels[a], &B = levels[b];
                if (A.n == 0 && a != 1) continue; if (B.v.empty()) continue;
                for (int i = 0; i < A.n; i++) {
                    double x = A.v[i];
                    if (allowS && x < T) { double y = T - x; int q = lower(B.v, B.n, y); for (int d = q - 1; d <= q; d++) if (d >= 0 && d < B.n) consider(list, x + B.v[d], 1, a, i, b, d); }
                    if (allowP && x > T) { double y = T * x / (x - T); int q = lower(B.v, B.n, y); for (int d = q - 1; d <= q; d++) if (d >= 0 && d < B.n) { double yy = B.v[d]; consider(list, x * yy / (x + yy), 2, a, i, b, d); } }
                }
            }
        }
        tops[m] = list;
        if (!report("Searching " + QString::number(m) + "-part combinations", std::min(0.995, (done + m * (double)nb * 2) / total))) { sol.error = "aborted"; return sol; }
    }

    std::function<Node(int, int)> node;
    std::function<Node(int, const Node &, const Node &)> combine = [&](int op, const Node &l, const Node &r) {
        Node n; n.leaf = false; n.op = op;
        for (const Node *c : {&l, &r}) { if (!c->leaf && c->op == op) n.kids.insert(n.kids.end(), c->kids.begin(), c->kids.end()); else n.kids.push_back(*c); }
        n.x = op == 1 ? l.x + r.x : l.x * r.x / (l.x + r.x);
        std::stable_sort(n.kids.begin(), n.kids.end(), [op](const Node &u, const Node &w) { return op == 1 ? w.x < u.x : u.x < w.x; });
        return n;
    };
    node = [&](int level, int idx) {
        const Level &Lv = levels[level];
        if (level == 1) { Node n; n.leaf = true; n.v = leafPhys[Lv.li[idx]]; n.x = Lv.v[idx]; return n; }
        return combine(Lv.op[idx], node(Lv.la[idx], Lv.li[idx]), node(Lv.rb[idx], Lv.ri[idx]));
    };
    std::function<void(const Node &, std::vector<double> &)> parts = [&](const Node &n, std::vector<double> &out) { if (n.leaf) out.push_back(n.v); else for (const Node &c : n.kids) parts(c, out); };
    std::function<QString(const Node &, bool)> expr = [&](const Node &n, bool top) -> QString {
        if (n.leaf) return fmt(n.v, 4, "");
        QStringList s; for (const Node &c : n.kids) s << expr(c, false);
        QString j = s.join(n.op == 1 ? " + " : " || ");
        return top ? j : "(" + j + ")";
    };
    std::vector<Result> results; QSet<QString> seen;
    for (int m = 1; m <= maxParts; m++) for (const Cand &c : tops[m]) {
        Node tree = c.op == 0 ? node(1, c.i) : combine(c.op, node(c.a, c.i), node(c.b, c.j));
        std::vector<double> ps; parts(tree, ps);
        QString e = expr(tree, true);
        if (seen.contains(e)) continue;
        seen.insert(e);
        double value = isC ? 1 / tree.x : tree.x;
        results.push_back({tree, ps, (int)ps.size(), value, (value - target) / target, e});
    }
    std::stable_sort(results.begin(), results.end(), [](const Result &u, const Result &w) { return u.count != w.count ? u.count < w.count : std::fabs(u.err) < std::fabs(w.err); });
    std::vector<Result> kept;
    for (const Result &r : results) {
        bool drop = false;
        for (const Result &k : kept) if (k.count < r.count && std::fabs(k.value - r.value) <= 1e-9 * r.value) { drop = true; break; }
        if (!drop) kept.push_back(r);
    }
    sol.results = kept; sol.leaves = nL;
    for (int k = 1; k < (int)levels.size(); k++) if (k == 1 || k <= maxParts - 1) sol.tableSizes.push_back(levels[k].n);
    sol.ms = t0.elapsed();
    return sol;
}

std::vector<Result> sortResults(const std::vector<Result> &results, const QString &mode, double tolPct)
{
    double tol = tolPct / 100;
    auto key = [](const Result &r) { return jsRound(std::fabs(r.err) * 1e7); };
    std::vector<Result> arr = results;
    if (mode == "accuracy") std::stable_sort(arr.begin(), arr.end(), [&](const Result &u, const Result &w) { double a = key(u), b = key(w); return a != b ? a < b : u.count < w.count; });
    else std::stable_sort(arr.begin(), arr.end(), [&](const Result &u, const Result &w) {
        bool iu = std::fabs(u.err) <= tol + 1e-12, iw = std::fabs(w.err) <= tol + 1e-12;
        if (iu != iw) return iu;
        if (iu) return u.count != w.count ? u.count < w.count : key(u) < key(w);
        double a = key(u), b = key(w); return a != b ? a < b : u.count < w.count;
    });
    return arr;
}

double evalTree(const Node &n, QChar type)
{
    if (n.leaf) return n.v;
    std::vector<double> v; for (const Node &k : n.kids) v.push_back(evalTree(k, type));
    double s = 0, inv = 0; for (double x : v) { s += x; inv += 1 / x; }
    double ser = type == 'C' ? 1 / inv : s, par = type == 'C' ? s : 1 / inv;
    return n.op == 1 ? ser : par;
}

} // namespace cc
} // namespace ke9esu
