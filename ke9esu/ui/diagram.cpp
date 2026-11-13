/* diagram.cpp - see diagram.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "diagram.h"
#include "widgets.h"
#include "ke9esu_util.h"
#include "drawings.h"
#include "theme.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace ke9esu {

void paintWinding(QPainter &p, const QRectF &rect, const DesignResult &d)
{
    // Coil64 drawing style: shaded ferrite, 3-D wire tubes, orange-red leads, black dimension lines, italic serif labels
    using namespace draw;
    p.save(); p.setRenderHint(QPainter::Antialiasing); fit(p, rect, kWindW, kWindH);
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
        QPointF b = pt(Ro + 50, ang); p.setPen(Qt::black); p.setFont(serif(15, true));
        p.drawText(QRectF(start ? b.x() + 2 : b.x() - 182, b.y() - 22, 180, 20), (start ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, txt);
    };
    lead(angles.front() - 7, aut ? S("START → GND") : S("START"), true);
    lead(angles.back() + 7, aut ? S("END → ANT") : S("END"), false);
    if (top.turnsRatio && d.n1 >= 1 && d.n1 < N) {
        double ta = (angles[d.n1 - 1] + angles[d.n1]) / 2;
        tubeLine(p, pt(Ri - 8, ta), pt(Ri - 40, ta), sw * 0.9, leadRed());
        QPointF b = pt(Ri - 46, ta); p.setPen(Qt::black); p.setFont(serif(15, true));
        p.drawText(QRectF(b.x() - 50, b.y() - 10, 100, 20), Qt::AlignCenter, aut ? S("TAP %1").arg(d.n1) : S("tap %1").arg(d.n1));
    }
    // dimensions (true core size in the labels)
    if (d.core) {
        const Core &k = *d.core;
        dim(p, QPointF(cx - Ro, cy), QPointF(cx + Ro, cy), Ro + 40, S("OD = %1 mm").arg(f(k.od, 1)), true);
        double yI = Ri * 0.38, xI = std::sqrt(Ri * Ri - yI * yI);
        dim(p, QPointF(cx - xI, cy + yI), QPointF(cx + xI, cy + yI), 0, S("ID = %1 mm").arg(f(k.idia, 1)));
        // core section with height (stack drawn as stacked sections)
        int st = std::max(1, std::min(3, int(std::lround(d.input.stack))));
        double wmm = (k.od - k.idia) / 2, hmm = k.h * st, s = std::min(40.0 / std::max(wmm, 0.1), 64.0 / std::max(hmm, 0.1));
        QRectF sec(380, 14, wmm * s, k.h * s);
        for (int i = 0; i < st; i++) hatched(p, sec.translated(0, i * k.h * s));
        dim(p, sec.topRight(), sec.topRight() + QPointF(0, hmm * s), -12, S("h"));
        p.setPen(Qt::black); p.setFont(serif(12)); p.drawText(QRectF(340, 14 + hmm * s + 4, 118, 16), Qt::AlignCenter, S("h = %1 mm%2").arg(f(k.h, 1), st > 1 ? S(" ×") + QString::number(st) : QString()));
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
    p.restore();
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

QImage renderWinding(const DesignResult &d, int w, int h) { return renderPaper([&](QPainter &p, const QRectF &r) { paintWinding(p, r, d); }, w, h); }
QImage renderImpedance(const DesignResult &d, int w, int h)
{
    QImage img(w * 2, h * 2, QImage::Format_ARGB32); img.fill(Qt::white); QPainter p(&img); p.setPen(Qt::black); p.scale(2, 2);
    paintImpedance(p, QRectF(0, 0, w, h), d); return img;
}

DesignView::DesignView(Kind k, QWidget *parent) : QWidget(parent), m_kind(k)
{
    setObjectName("ke9esuDiagram"); setAttribute(Qt::WA_StyledBackground, true);
    setMinimumSize(k == Winding ? QSize(300, 320) : QSize(420, 240));
}
QSize DesignView::sizeHint() const { return m_kind == Winding ? QSize(kWindW, kWindH) : QSize(760, 320); }
void DesignView::paintEvent(QPaintEvent *)
{
    // drawings live on "paper" (white, light grey in dark themes) with black ink, like Coil64's own pictures
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const ThemeColors &c = theme(); p.setPen(QPen(c.line, 1)); p.setBrush(c.paper); p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    if (!m_has || !m_d.core) return;
    p.setPen(c.paperInk);
    if (m_kind == Winding) paintWinding(p, QRectF(rect()).adjusted(6, 6, -6, -6), m_d);
    else paintImpedance(p, QRectF(rect()).adjusted(4, 4, -4, -4), m_d);
}

} // namespace ke9esu
