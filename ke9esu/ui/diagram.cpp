/* diagram.cpp - see diagram.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "diagram.h"
#include "widgets.h"
#include "ke9esu_util.h"
#include "drawings.h"
#include "theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <cmath>

namespace ke9esu {

void paintWinding(QPainter &p, const QRectF &rect, const DesignResult &d)
{
    // Coil64 drawing style: shaded ferrite, 3-D wire tubes, orange-red leads, black dimension lines, italic serif labels
    using namespace draw;
    p.save(); p.setRenderHint(QPainter::Antialiasing); fit(p, rect, kWindW, windH(d));
    const double cx = 230, cy = 222, Ro = 146, Ri = 80;
    const Topology &top = d.topology; bool isChoke = top.kind == "choke";
    bool coax = isChoke && d.conductor && d.conductor->kind == "coax";
    int N = top.turnsRatio ? d.n2 : d.n1, wires = coax ? 1 : top.wires;
    const QColor cols[3] = {wireYellow(), wireOrange(), wireBlue()};
    ferriteRing(p, QPointF(cx, cy), Ro, Ri);
    double a0 = -90 + 25, span = 310; std::vector<double> angles; bool aut = top.id == "auto91";
    if (top.turnsRatio && !aut) {
        double tight = std::min(span * 0.28, d.n1 * 9.0), rest = span - tight - 10;
        for (int i = 0; i < N; i++) angles.push_back(i < d.n1 ? a0 + (d.n1 > 1 ? i * tight / (d.n1 - 1) : 0) : a0 + tight + 10 + (N - d.n1 > 1 ? (i - d.n1) * rest / (N - d.n1 - 1) : rest));
    } else for (int j = 0; j < N; j++) angles.push_back(a0 + (N > 1 ? j * span / (N - 1) : span / 2));
    if (angles.empty()) { p.restore(); return; }
    double dA = wires > 1 ? std::min(4.5, 120.0 / std::max(N, 1) / wires) : 0;
    double sw = coax ? std::min(10.0, std::max(4.0, 260.0 / N)) : std::max(2.6, std::min(6.5, 170.0 / (N * wires)));
    auto pt = [&](double r, double deg) { double a = deg * PI / 180; return QPointF(cx + r * std::cos(a), cy + r * std::sin(a)); };
    auto colOf = [&](int t, int w) { return coax ? coaxJacket() : top.turnsRatio ? (t < d.n1 ? wireYellow() : wireOrange()) : cols[w % 3]; };
    for (int t = 0; t < (int)angles.size(); t++) {
        for (int w = 0; w < wires; w++) { double a = angles[t] + (w - (wires - 1) / 2.0) * dA; tubeLine(p, pt(Ri - 9, a), pt(Ro + 9, a), sw, colOf(t, w)); }
        if (N <= 24 || (t + 1) % 5 == 0 || t == 0) { QPointF lp = pt(Ro + 26, angles[t]); p.setPen(Qt::black); p.setFont(serif(14));
            p.drawText(QRectF(lp.x() - 15, lp.y() - 9, 30, 18), Qt::AlignCenter, QString::number(t + 1)); }
    }
    auto lead = [&](double ang, const QString &txt, bool start) {
        double base = start ? angles.front() : angles.back();
        for (int w = 0; w < wires; w++) { double a = base + (w - (wires - 1) / 2.0) * dA; tubeLine(p, pt(Ro + 9, a), pt(Ro + 46, ang + (w - (wires - 1) / 2.0) * 2.5), sw * 0.9, coax ? coaxJacket() : leadRed()); }
        QPointF b = pt(Ro + 50, ang); p.setPen(Qt::black); p.setFont(serif(aut ? 13 : 15, true));   // auto91 labels are longer: keep clear of the core section
        p.drawText(QRectF(start ? b.x() + 2 : b.x() - 182, b.y() - 22, 180, 20), (start ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, txt);
    };
    lead(angles.front() - 7, aut ? S("START → GND") : S("START"), true);
    lead(angles.back() + 7, aut ? S("END → ANT") : S("END"), false);
    bool tapBelow = false;
    if (top.turnsRatio && d.n1 >= 1 && d.n1 < N) {
        double ta = (angles[d.n1 - 1] + angles[d.n1]) / 2;
        tubeLine(p, pt(Ri - 8, ta), pt(Ri - 40, ta), sw * 0.9, leadRed());
        // label above the lead end when the tap points down, below it otherwise (same rule as the standalone designer)
        QPointF b = pt(Ri - 44, ta); bool above = std::sin(ta * PI / 180) > 0.2; tapBelow = !above;
        p.setPen(Qt::black); p.setFont(serif(15, true));
        p.drawText(QRectF(b.x() - 50, above ? b.y() - 24 : b.y() + 2, 100, 20), Qt::AlignCenter, aut ? S("TAP %1").arg(d.n1) : S("tap %1").arg(d.n1));
    }
    // dimensions (true core size in the labels)
    if (d.core) {
        const Core &k = *d.core;
        dim(p, QPointF(cx - Ro, cy), QPointF(cx + Ro, cy), Ro + 40, S("OD = %1 mm").arg(f(k.od, 1)), true);
        double yI = tapBelow ? -Ri * 0.45 : top.turnsRatio ? Ri * 0.52 : Ri * 0.38, xI = std::sqrt(Ri * Ri - yI * yI);   // ID dimension kept clear of the TAP label
        dim(p, QPointF(cx - xI, cy + yI), QPointF(cx + xI, cy + yI), 0, S("ID = %1 mm").arg(f(k.idia, 1)));
        // core section with height (stack drawn as stacked sections)
        int st = std::max(1, std::min(3, int(std::lround(d.input.stack))));
        double wmm = (k.od - k.idia) / 2, hmm = k.h * st, s = std::min(40.0 / std::max(wmm, 0.1), 64.0 / std::max(hmm, 0.1));
        QRectF sec(aut ? 404 : 380, 14, wmm * s, k.h * s);
        for (int i = 0; i < st; i++) hatched(p, sec.translated(0, i * k.h * s));
        dim(p, sec.topRight(), sec.topRight() + QPointF(0, hmm * s), -12, S("h"));
        p.setPen(Qt::black); p.setFont(serif(12)); p.drawText(QRectF(aut ? 362 : 340, 14 + hmm * s + 4, aut ? 98 : 118, 16), Qt::AlignCenter, S("h = %1 mm%2").arg(f(k.h, 1), st > 1 ? S(" ×") + QString::number(st) : QString()));
    }
    p.setPen(Qt::black); p.setFont(serif(16, true));
    p.drawText(QRectF(0, kWindH - 58, kWindW, 20), Qt::AlignCenter, (d.core ? d.core->id : QString()) + (d.input.stack > 1 ? S(" ×") + jsNum(d.input.stack) : QString()) + S(" — ") +
               (top.turnsRatio ? QString("%1 : %2 turns").arg(d.n1).arg(d.n2) : QString("%1 turn%2").arg(N).arg(N == 1 ? "" : "s")));
    QList<QPair<QColor, QString>> leg;
    if (coax) leg << qMakePair(coaxJacket(), d.conductor->label);
    else if (aut) { leg << qMakePair(wireYellow(), S("turns 1–%1 (tap → SO-239 centre)").arg(d.n1)) << qMakePair(wireOrange(), S("turns %1–%2").arg(d.n1 + 1).arg(d.n2)); }
    else if (top.turnsRatio) { leg << qMakePair(wireYellow(), S("primary 1–%1 (tight)").arg(d.n1)) << qMakePair(wireOrange(), S("turns %1–%2 (spread)").arg(d.n1 + 1).arg(d.n2)); }
    else { const char *names[3] = {"A", "B", "C"}; for (int w = 0; w < wires; w++) leg << qMakePair(cols[w], QString("wire ") + names[w]); }
    p.setFont(serif(14)); double total = 0; for (auto &l : leg) total += 34 + 14 + p.fontMetrics().horizontalAdvance(l.second);
    double lx = std::max(8.0, (kWindW - total) / 2), ly = kWindH - 18;
    for (auto &l : leg) { tubeLine(p, QPointF(lx, ly - 5), QPointF(lx + 26, ly - 5), 5, l.first); p.setPen(Qt::black); p.drawText(QPointF(lx + 34, ly), l.second); lx += 48 + p.fontMetrics().horizontalAdvance(l.second); }
    if (top.coreSets > 1) { p.setFont(serif(14, true)); p.drawText(QRectF(8, 8, 320, 36), Qt::TextWordWrap, "Make TWO identical windings, one per core set."); }
    if (d.hasCompCap) {
        // v1.3.2 low-side shunt capacitor across the SO-239: flange + centre pin, red leads, capacitor plates, shell return
        const double y0 = kWindH + 4, cy2 = y0 + 40, sx = 62;
        p.setPen(QPen(QColor(0, 0, 0, 60), 1)); p.drawLine(QPointF(8, y0), QPointF(kWindW - 8, y0));
        QLinearGradient gr(QPointF(sx - 22, cy2 - 22), QPointF(sx + 22, cy2 + 22)); gr.setColorAt(0, QColor("#e3e6ea")); gr.setColorAt(1, QColor("#8d939b"));
        p.setPen(QPen(Qt::black, 1.2)); p.setBrush(gr); p.drawRoundedRect(QRectF(sx - 22, cy2 - 22, 44, 44), 5, 5);
        p.setBrush(QColor("#5f656d")); p.drawEllipse(QPointF(sx, cy2), 14, 14);
        p.setBrush(QColor("#f1f2f4")); p.drawEllipse(QPointF(sx, cy2), 9, 9);
        p.setBrush(QColor("#d4a017")); p.drawEllipse(QPointF(sx, cy2), 4, 4);
        for (QPointF h : {QPointF(sx - 17, cy2 - 17), QPointF(sx + 17, cy2 - 17), QPointF(sx - 17, cy2 + 17), QPointF(sx + 17, cy2 + 17)}) { p.setBrush(Qt::white); p.drawEllipse(h, 2.2, 2.2); }
        tubeLine(p, QPointF(sx + 4, cy2), QPointF(160, cy2), 4, leadRed());
        tubeLine(p, QPointF(172, cy2), QPointF(214, cy2), 4, leadRed());
        tubeLine(p, QPointF(214, cy2), QPointF(214, cy2 + 35), 4, leadRed());
        tubeLine(p, QPointF(214, cy2 + 35), QPointF(sx + 17, cy2 + 35), 4, leadRed());
        tubeLine(p, QPointF(sx + 17, cy2 + 35), QPointF(sx + 17, cy2 + 19), 4, leadRed());
        p.setPen(QPen(Qt::black, 4.5, Qt::SolidLine, Qt::FlatCap)); p.drawLine(QPointF(162, cy2 - 15), QPointF(162, cy2 + 15)); p.drawLine(QPointF(170, cy2 - 15), QPointF(170, cy2 + 15));
        p.setPen(Qt::black); p.setFont(serif(12)); p.drawText(QRectF(sx - 40, y0 + 2, 80, 16), Qt::AlignCenter, "SO-239");
        p.drawText(QRectF(80, cy2 - 20, 80, 16), Qt::AlignCenter, S("centre")); p.drawText(QRectF(84, cy2 + 12, 126, 16), Qt::AlignCenter, S("shell (ground)"));
        p.setFont(serif(16, true)); p.drawText(QPointF(228, cy2 - 4), S("C = %1 pF · %2").arg(jsNum(d.compCap.pF), kvTxt(d.compCap.ratingV)));
        p.setFont(serif(13)); p.drawText(QPointF(228, cy2 + 16), S("low-side shunt, SO-239 centre → shell"));
    }
    p.restore();
}

/* ---- v1.3.2 transformer frequency sweep ---- */
namespace {
struct SweepLayout { double L, R, T, gap, B, ph, pw, f1, f2; bool lg; double panel[3]; };
SweepLayout sweepLayout(double W, double H, const DesignResult &d)
{
    SweepLayout g; g.L = 72; g.R = 70; g.T = 8; g.gap = 30; g.B = 40; g.ph = (H - g.T - g.B - 2 * g.gap) / 3; g.pw = W - g.L - g.R;
    const auto &pts = d.sweep.points; g.f1 = pts.front().fMHz; g.f2 = pts.back().fMHz; g.lg = d.input.sweepScale != "lin";
    if (g.f2 <= g.f1) g.f2 = g.f1 * 1.01;
    for (int i = 0; i < 3; i++) g.panel[i] = g.T + i * (g.ph + g.gap);
    return g;
}
double sweepX(const SweepLayout &g, double v) { return g.L + g.pw * (g.lg ? std::log(v / g.f1) / std::log(g.f2 / g.f1) : (v - g.f1) / (g.f2 - g.f1)); }
}
void paintSweep(QPainter &p, const QRectF &rect, const DesignResult &d, double cursorMHz)
{
    if (!d.hasSweep || d.sweep.points.empty()) return;
    p.save(); p.setRenderHint(QPainter::Antialiasing); p.translate(rect.topLeft());
    double W = rect.width(), H = rect.height(); QColor ink = p.pen().color();
    const Sweep &sw = d.sweep; const auto &pts = sw.points; SweepLayout g = sweepLayout(W, H, d);
    auto X = [&](double v) { return sweepX(g, v); };
    double swrMax = 0; for (auto &q : pts) swrMax = std::max(swrMax, q.swr); swrMax = std::min(10.0, std::max(3.0, std::ceil(swrMax)));
    double zAbs = 0; for (auto &q : pts) zAbs = std::max(zAbs, std::max(q.z, std::fabs(q.x))); double zTop = std::min(1000.0, std::max(100.0, std::ceil(zAbs / 50) * 50));
    auto Ys = [&](double v) { return g.panel[0] + g.ph * (1 - (std::min(v, swrMax) - 1) / (swrMax - 1)); };
    auto Yz = [&](double v) { return g.panel[1] + g.ph * (1 - (std::max(-zTop, std::min(zTop, v)) + zTop) / (2 * zTop)); };
    auto Yr = [&](double v) { return g.panel[2] + g.ph * std::min(1.0, std::max(0.0, v / 40)); };
    auto Ye = [&](double v) { return g.panel[2] + g.ph * (1 - v / 100); };
    QFont fn = p.font(); fn.setPixelSize(12); p.setFont(fn);
    QColor grid(128, 128, 128, 80);
    for (int i = 0; i < 3; i++) {
        double py = g.panel[i];
        for (const Band &b : bands()) { if (b.hi < g.f1 || b.lo > g.f2) continue; double xa = X(std::max(b.lo, g.f1)), xb = std::max(X(std::min(b.hi, g.f2)), xa + 3);
            p.fillRect(QRectF(xa, py, xb - xa, g.ph), QColor(255, 200, 0, 50)); if (i == 0) { p.setPen(ink); p.drawText(QRectF(xa - 15, py + 1, xb - xa + 30, 14), Qt::AlignCenter, QString(b.id).remove('m')); } }
    }
    std::vector<double> ticks;
    if (g.lg) ticks = {1, 2, 3, 5, 7, 10, 14, 20, 30, 50, 70, 100, 150, 200};
    else { double span = g.f2 - g.f1, st = span > 40 ? 10 : span > 15 ? 5 : span > 6 ? 2 : 1; for (double t = std::ceil(g.f1 / st) * st; t <= g.f2 + 1e-9; t += st) ticks.push_back(t); }
    for (double t : ticks) { if (t < g.f1 || t > g.f2) continue; double x = X(t); p.setPen(QPen(grid, 1)); for (double py : g.panel) p.drawLine(QPointF(x, py), QPointF(x, py + g.ph));
        p.setPen(ink); p.drawText(QRectF(x - 20, H - g.B + 6, 40, 16), Qt::AlignCenter, jsNum(t)); }
    auto hl = [&](double y, const QString &txt) { p.setPen(QPen(grid, 1)); p.drawLine(QPointF(g.L, y), QPointF(g.L + g.pw, y)); p.setPen(ink); p.drawText(QRectF(0, y - 8, g.L - 6, 16), Qt::AlignRight | Qt::AlignVCenter, txt); };
    for (double v : {1.0, 1.5, 2.0, 3.0, 5.0, 10.0}) if (v <= swrMax) hl(Ys(v), jsNum(v) + ":1");
    for (double v : {-zTop, -zTop / 2, 0.0, zTop / 2, zTop}) hl(Yz(v), rnd(v) + S(" Ω"));
    for (double v : {0.0, 10.0, 20.0, 30.0, 40.0}) { hl(Yr(v), (v ? S("−") : QString()) + jsNum(v) + " dB"); double e = 100 - v * 2.5; p.drawText(QRectF(g.L + g.pw + 6, Ye(e) - 8, g.R - 6, 16), Qt::AlignLeft | Qt::AlignVCenter, jsNum(e) + " %"); }
    auto path = [&](const std::function<double(int)> &val, const std::function<double(double)> &fy) { QPainterPath pp; for (size_t i = 0; i < pts.size(); i++) { QPointF q(X(pts[i].fMHz), fy(val((int)i))); if (i) pp.lineTo(q); else pp.moveTo(q); } return pp; };
    p.setBrush(Qt::NoBrush);
    const QColor lc[6] = {QColor("#7d3cb5"), QColor("#2e8b57"), QColor("#c0392b"), QColor("#2a5db0"), QColor("#c46a00"), QColor("#777777")};
    p.save(); p.setClipRect(QRectF(g.L, g.panel[0], g.pw, g.ph));
    for (size_t j = 0; j < sw.loads.size(); j++) { p.setPen(QPen(lc[j % 6], 1.3, Qt::DashLine)); p.drawPath(path([&](int i) { return sw.loads[j].swr[i]; }, Ys)); }
    p.setPen(QPen(QColor("#1f5fbf"), 2.6)); p.drawPath(path([&](int i) { return pts[i].swr; }, Ys));
    p.restore();
    p.setPen(QPen(QColor("#c46a00"), 2.2)); p.drawPath(path([&](int i) { return pts[i].r; }, Yz));
    p.setPen(QPen(QColor("#1f7a3a"), 2, Qt::DashLine)); p.drawPath(path([&](int i) { return pts[i].x; }, Yz));
    p.setPen(QPen(ink, 2.6)); p.drawPath(path([&](int i) { return pts[i].z; }, Yz));
    p.setPen(QPen(QColor("#c0392b"), 2.2)); p.drawPath(path([&](int i) { return pts[i].rlDb; }, Yr));
    p.setPen(QPen(QColor("#2e8b57"), 2.2, Qt::DashLine)); p.drawPath(path([&](int i) { return pts[i].effPct; }, Ye));
    for (double py : g.panel) { p.setPen(QPen(Qt::gray, 1)); p.drawRect(QRectF(g.L, py, g.pw, g.ph)); }
    const char *labs[3] = {"SWR", "R · X · |Z|", "RL · eff."};
    for (int i = 0; i < 3; i++) { p.save(); p.setPen(ink); p.translate(14, g.panel[i] + g.ph / 2); p.rotate(-90); p.drawText(QRectF(-g.ph / 2, -9, g.ph, 18), Qt::AlignCenter, S(labs[i])); p.restore(); }
    struct Key { QColor c; QString t; int panel; Qt::PenStyle st; };
    QList<Key> keys = {{QColor("#1f5fbf"), S("SWR re %1 Ω into %2 Ω").arg(jsNum(d.input.zLow), rnd(sw.model.loadOhm)), 0, Qt::SolidLine}, {QColor("#c46a00"), "R", 1, Qt::SolidLine}, {QColor("#1f7a3a"), "X", 1, Qt::DashLine},
                       {ink, "|Z|", 1, Qt::SolidLine}, {QColor("#c0392b"), "return loss", 2, Qt::SolidLine}, {QColor("#2e8b57"), "efficiency (right axis)", 2, Qt::DashLine}};
    double kx[3] = {g.L + 8, g.L + 8, g.L + 8}; QFontMetricsF fm(p.font());
    for (auto &k : keys) { double y = g.panel[k.panel] + g.ph - 10; p.setPen(QPen(k.c, 3, k.st)); p.drawLine(QPointF(kx[k.panel], y - 4), QPointF(kx[k.panel] + 22, y - 4)); p.setPen(ink); p.drawText(QPointF(kx[k.panel] + 27, y), k.t); kx[k.panel] += 40 + fm.horizontalAdvance(k.t); }
    for (size_t j = 0; j < sw.loads.size(); j++) { p.setPen(lc[j % 6]); p.drawText(QRectF(g.L + g.pw - 120, g.panel[0] + 16 + j * 14, 114, 14), Qt::AlignRight | Qt::AlignVCenter, jsNum(sw.loads[j].ohm) + S(" Ω")); }
    p.setPen(ink); p.drawText(QRectF(g.L, H - 20, g.pw, 18), Qt::AlignCenter, QString("Frequency (MHz, %1)").arg(g.lg ? "log" : "linear"));
    if (std::isfinite(cursorMHz) && cursorMHz >= g.f1 && cursorMHz <= g.f2) { double x = X(cursorMHz); p.setPen(QPen(ink, 1, Qt::DashLine)); p.drawLine(QPointF(x, g.T), QPointF(x, H - g.B)); }
    p.restore();
}
int sweepAtX(const QRectF &r, const DesignResult &d, double x)
{
    if (!d.hasSweep || d.sweep.points.empty()) return -1;
    SweepLayout g = sweepLayout(r.width(), r.height(), d); double u = std::max(0.0, std::min(1.0, (x - r.left() - g.L) / g.pw));
    double fv = g.lg ? g.f1 * std::pow(g.f2 / g.f1, u) : g.f1 + u * (g.f2 - g.f1);
    int best = 0; for (int i = 0; i < (int)d.sweep.points.size(); i++) if (std::fabs(d.sweep.points[i].fMHz - fv) < std::fabs(d.sweep.points[best].fMHz - fv)) best = i;
    return best;
}
QString sweepPointText(const SweepPoint &q)
{
    QString band; for (const Band &b : bands()) if (q.fMHz >= b.lo && q.fMHz <= b.hi) band = " (" + b.id + ")";
    return f(q.fMHz, 3) + " MHz" + band + S(": R ") + f(q.r, 1) + S(" Ω, X ") + f(std::fabs(q.x) < 0.05 ? 0.0 : q.x, 1) + S(" Ω, |Z| ") + f(q.z, 1) + S(" Ω, SWR ") + f(q.swr, 2) + S(":1, return loss ") + f(q.rlDb, 1) + " dB, efficiency " + f(q.effPct, 1) + " %";
}
QString sweepCsv(const DesignResult &d)
{
    const Sweep &sw = d.sweep; QString out = "# Coil64 KE9ESU / Balun & Unun Designer v" + QString(CORE_VERSION) + " sweep: " + d.topology.name + ", " + d.core->id + " x" + jsNum(d.input.stack) + ", " +
        (d.topology.turnsRatio ? QString("%1:%2").arg(d.n1).arg(d.n2) : QString::number(d.n1)) + " turns, load " + rnd(sw.model.loadOhm) + " ohm, shunt C " + (d.hasCompCap ? jsNum(d.compCap.pF) + " pF" : QString("off")) + ", " + sw.model.kind + " model\n";
    out += "MHz,R_ohm,X_ohm,Z_ohm,SWR,ReturnLoss_dB,Efficiency_pct,MismatchLoss_dB"; for (auto &l : sw.loads) out += ",SWR_" + jsNum(l.ohm) + "ohm"; out += "\n";
    for (size_t i = 0; i < sw.points.size(); i++) { const SweepPoint &q = sw.points[i];
        out += toFixed(q.fMHz, 4) + "," + toFixed(q.r, 3) + "," + toFixed(q.x, 3) + "," + toFixed(q.z, 3) + "," + toFixed(q.swr, 4) + "," + toFixed(q.rlDb, 2) + "," + toFixed(q.effPct, 2) + "," + toFixed(q.mismatchDb, 3);
        for (auto &l : sw.loads) out += "," + toFixed(l.swr[i], 4); out += "\n"; }
    return out;
}
QImage renderSweep(const DesignResult &d, int w, int h)
{
    QImage img(w * 2, h * 2, QImage::Format_ARGB32); img.fill(Qt::white); QPainter p(&img); p.setPen(Qt::black); p.scale(2, 2);
    paintSweep(p, QRectF(0, 0, w, h), d); return img;
}

void paintImpedance(QPainter &p, const QRectF &rect, const DesignResult &d)
{
    p.save(); p.setRenderHint(QPainter::Antialiasing); p.translate(rect.topLeft());
    double W = rect.width(), H = rect.height(); QColor ink = p.pen().color();
    const BalunInput &inp = d.input; bool isChoke = d.topology.kind == "choke";
    double f1 = std::max(0.5, std::min(1.5, inp.fMinMHz / 2)), f2 = std::min(200.0, std::max(60.0, inp.fMaxMHz * 1.4));
    auto curve = impedanceCurve(*d.core, d.n1, inp.stack, isChoke ? d.stray_pF : 0, f1, f2, 240);
    double zMax = isChoke ? 6000 : d.target * 2, rMin = 1e300;
    for (auto &c : curve) { zMax = std::max(zMax, c.z); rMin = std::min(rMin, std::max(c.r, 1.0)); }
    double y2 = std::pow(10, std::ceil(std::log10(zMax * 1.2))), y1 = std::pow(10, std::max(0.0, std::floor(std::log10(std::max(1.0, rMin)))));
    double L = 70, R = 20, T = 15, B = 45, pw = W - L - R, ph = H - T - B;
    auto X = [&](double fv) { return L + pw * std::log(fv / f1) / std::log(f2 / f1); };
    auto Y = [&](double z) { return T + ph * (1 - std::log(std::max(z, y1) / y1) / std::log(y2 / y1)); };
    QFont fn = p.font(); fn.setPixelSize(12); p.setFont(fn);
    for (const Band &b : bands()) { if (b.hi < f1 || b.lo > f2) continue; double xa = X(b.lo), xb = std::max(X(b.hi), xa + 3);
        p.fillRect(QRectF(xa, T, xb - xa, ph), QColor(255, 200, 0, 50)); p.setPen(ink); p.drawText(QRectF(xa - 15, T + 2, xb - xa + 30, 14), Qt::AlignCenter, QString(b.id).remove('m')); }
    p.setPen(QPen(QColor(128, 128, 128, 80), 1));
    for (double g : {0.5, 1.0, 2.0, 3.0, 5.0, 7.0, 10.0, 14.0, 20.0, 30.0, 50.0, 70.0, 100.0, 200.0}) { if (g < f1 || g > f2) continue; double x = X(g);
        p.setPen(QPen(QColor(128, 128, 128, 80), 1)); p.drawLine(QPointF(x, T), QPointF(x, T + ph)); p.setPen(ink); p.drawText(QRectF(x - 20, H - B + 6, 40, 16), Qt::AlignCenter, jsNum(g)); }
    for (double e = std::log10(y1); e <= std::log10(y2) + 1e-9; e++) { double yv = std::pow(10, e), yy = Y(yv);
        p.setPen(QPen(QColor(128, 128, 128, 80), 1)); p.drawLine(QPointF(L, yy), QPointF(L + pw, yy)); p.setPen(ink);
        p.drawText(QRectF(0, yy - 8, L - 6, 16), Qt::AlignRight | Qt::AlignVCenter, yv >= 1000 ? jsNum(yv / 1000) + "k" : jsNum(yv)); }
    p.drawText(QRectF(L, H - 20, pw, 18), Qt::AlignCenter, "Frequency (MHz)");
    p.save(); p.translate(14, T + ph / 2); p.rotate(-90); p.drawText(QRectF(-40, -9, 80, 18), Qt::AlignCenter, "Ohms"); p.restore();
    auto hline = [&](double v, QColor col, const QString &txt) { if (v < y1 || v > y2) return; double yy = Y(v);
        p.setPen(QPen(col, 1.5, Qt::DashLine)); p.drawLine(QPointF(L, yy), QPointF(L + pw, yy)); p.setPen(col); p.drawText(QRectF(L, yy - 18, pw - 4, 16), Qt::AlignRight, txt); };
    if (isChoke) { hline(5000, QColor("#d0342c"), S("K9YC ~5 kΩ")); if (std::fabs(d.target - 5000) > 1) hline(d.target, QColor("#1f7a3a"), "target " + fmtOhm(d.target)); }
    else hline(d.target, QColor("#1f7a3a"), S("XL target ") + rnd(d.target) + S(" Ω"));
    auto path = [&](int which) { QPainterPath pp; for (size_t i = 0; i < curve.size(); i++) { double v = which == 0 ? curve[i].r : which == 1 ? std::fabs(curve[i].x) : curve[i].z;
        QPointF q(X(curve[i].fMHz), Y(std::max(v, y1))); if (i) pp.lineTo(q); else pp.moveTo(q); } return pp; };
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#c46a00"), 2.2)); p.drawPath(path(0));
    p.setPen(QPen(QColor("#1f5fbf"), 2, Qt::DashLine)); p.drawPath(path(1));
    p.setPen(QPen(ink, 3)); p.drawPath(path(2));
    double lx = L + 10, ly = T + 34; QList<std::tuple<QColor, QString, Qt::PenStyle>> leg = {{QColor("#c46a00"), "R", Qt::SolidLine}, {QColor("#1f5fbf"), "|X|", Qt::DashLine}, {ink, "|Z|", Qt::SolidLine}};
    for (int i = 0; i < leg.size(); i++) { p.setPen(QPen(std::get<0>(leg[i]), 3, std::get<2>(leg[i]))); p.drawLine(QPointF(lx, ly + i * 18), QPointF(lx + 26, ly + i * 18)); p.setPen(ink); p.drawText(QPointF(lx + 32, ly + i * 18 + 4), std::get<1>(leg[i])); }
    p.setPen(QPen(Qt::gray, 1)); p.drawRect(QRectF(L, T, pw, ph));
    p.restore();
}

QImage renderWinding(const DesignResult &d, int w, int h) { return renderPaper([&](QPainter &p, const QRectF &r) { paintWinding(p, r, d); }, w, h ? h : windH(d)); }
QImage renderImpedance(const DesignResult &d, int w, int h)
{
    QImage img(w * 2, h * 2, QImage::Format_ARGB32); img.fill(Qt::white); QPainter p(&img); p.setPen(Qt::black); p.scale(2, 2);
    paintImpedance(p, QRectF(0, 0, w, h), d); return img;
}

DesignView::DesignView(Kind k, QWidget *parent) : QWidget(parent), m_kind(k)
{
    setObjectName("ke9esuDiagram"); setAttribute(Qt::WA_StyledBackground, true);
    setMinimumSize(k == Winding ? QSize(300, 320) : k == Sweep ? QSize(480, 420) : QSize(420, 240));
    if (k == Sweep) { setMouseTracking(true); setCursor(Qt::CrossCursor); }
}
QSize DesignView::sizeHint() const { return m_kind == Winding ? QSize(kWindW, m_has ? windH(m_d) : kWindH) : m_kind == Sweep ? QSize(860, 560) : QSize(760, 320); }
int DesignView::heightForWidth(int w) const { return m_kind == Winding ? int(w * double(m_has ? windH(m_d) : kWindH) / kWindW) : -1; }
QRectF DesignView::plotRect() const { return QRectF(rect()).adjusted(4, 4, -4, -4); }
void DesignView::mouseMoveEvent(QMouseEvent *e)
{
    if (m_kind != Sweep || !m_has) return;
    int i = sweepAtX(plotRect(), m_d, e->pos().x()); if (i < 0) return;
    m_cursor = m_d.sweep.points[i].fMHz; update(); emit hovered(sweepPointText(m_d.sweep.points[i]));
}
void DesignView::paintEvent(QPaintEvent *)
{
    // drawings live on "paper" (white, light grey in dark themes) with black ink, like Coil64's own pictures
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const ThemeColors &c = theme(); p.setPen(QPen(c.line, 1)); p.setBrush(c.paper); p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    if (!m_has || !m_d.core) return;
    p.setPen(c.paperInk);
    if (m_kind == Winding) { QRectF r = QRectF(rect()).adjusted(6, 6, -6, -6);   // top-aligned when the tab is taller than the drawing
        r.setHeight(std::min(r.height(), r.width() * double(windH(m_d)) / kWindW)); paintWinding(p, r, m_d); }
    else if (m_kind == Sweep) paintSweep(p, plotRect(), m_d, m_cursor);
    else paintImpedance(p, QRectF(rect()).adjusted(4, 4, -4, -4), m_d);
}

} // namespace ke9esu
