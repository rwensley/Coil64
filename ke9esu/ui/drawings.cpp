/* drawings.cpp - see drawings.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "drawings.h"
#include "theme.h"
#include "widgets.h"
#include "ke9esu_util.h"
#include "balun_core.h"
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <QRadialGradient>
#include <cmath>
#include <algorithm>

namespace ke9esu {
namespace draw {

QFont serif(double px, bool bold, bool italic)
{
    QFont f(QStringLiteral("Times New Roman")); f.setStyleHint(QFont::Serif); f.setFamilies({"Times New Roman", "Liberation Serif", "DejaVu Serif", "serif"});
    f.setPixelSize(int(std::lround(px))); f.setBold(bold); f.setItalic(italic); return f;
}
QColor wireYellow() { return QColor("#e8d81c"); }
QColor wireOrange() { return QColor("#ec7a2c"); }
QColor wireBlue() { return QColor("#3f9be0"); }
QColor leadRed() { return QColor("#e2401c"); }
QColor coaxJacket() { return QColor("#3a3a3a"); }

void fit(QPainter &p, const QRectF &r, double W, double H)
{
    double s = std::min(r.width() / W, r.height() / H);
    p.translate(r.x() + (r.width() - W * s) / 2, r.y() + (r.height() - H * s) / 2); p.scale(s, s);
}
void tube(QPainter &p, const QPainterPath &path, double w, const QColor &base)
{
    p.save(); p.setBrush(Qt::NoBrush);
    p.setPen(QPen(base.darker(260), w + 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPath(path);
    p.setPen(QPen(base, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPath(path);
    QColor hi = base.lighter(175); hi.setAlpha(220);
    p.translate(-w * 0.18, -w * 0.18); p.setPen(QPen(hi, std::max(0.8, w * 0.32), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPath(path);
    p.restore();
}
void tubeLine(QPainter &p, QPointF a, QPointF b, double w, const QColor &base) { QPainterPath pp(a); pp.lineTo(b); tube(p, pp, w, base); }
void arrowHead(QPainter &p, QPointF tip, QPointF from, double size)
{
    QPointF d = tip - from; double l = std::hypot(d.x(), d.y()); if (l < 1e-6) return; d /= l; QPointF n(-d.y(), d.x());
    QPolygonF tri; tri << tip << tip - d * size + n * size * 0.32 << tip - d * size - n * size * 0.32;
    p.save(); p.setPen(Qt::NoPen); p.setBrush(Qt::black); p.drawPolygon(tri); p.restore();
}
void dim(QPainter &p, QPointF a, QPointF b, double off, const QString &label, bool labelBelow)
{
    QPointF d = b - a; double l = std::hypot(d.x(), d.y()); if (l < 1e-6) return; QPointF u = d / l, n(-u.y(), u.x());
    QPointF a2 = a + n * off, b2 = b + n * off, ext = n * (off >= 0 ? 5 : -5);
    p.save(); p.setPen(QPen(Qt::black, 0.9)); p.setBrush(Qt::NoBrush);
    if (std::fabs(off) > 0.5) { p.drawLine(a + n * (off >= 0 ? 2 : -2), a2 + ext); p.drawLine(b + n * (off >= 0 ? 2 : -2), b2 + ext); }
    p.drawLine(a2, b2); arrowHead(p, a2, b2); arrowHead(p, b2, a2);
    p.setFont(serif(15)); QPointF m = (a2 + b2) / 2; QFontMetricsF fm(p.font()); double tw = fm.horizontalAdvance(label) + 6;
    if (std::fabs(u.x()) >= std::fabs(u.y())) {   // horizontal or sloped: text runs along the dimension line
        double ang = std::atan2(u.y(), u.x()) * 180 / PI; if (ang > 90) ang -= 180; if (ang < -90) ang += 180;
        p.translate(m); p.rotate(ang);
        QRectF tr(-tw / 2, labelBelow ? 2 : -fm.height() - 1, tw, fm.height());
        p.drawText(tr, Qt::AlignCenter, label);
    } else {
        QRectF tr(labelBelow ? m.x() - tw - 4 : m.x() + 4, m.y() - fm.height() / 2, tw, fm.height());
        p.drawText(tr, (labelBelow ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, label);
    }
    p.restore();
}
void hatched(QPainter &p, const QRectF &r, const QColor &fill)
{
    p.save(); p.setPen(QPen(Qt::black, 1.1)); p.setBrush(fill); p.drawRect(r);
    p.setBrush(QBrush(fill.darker(170), Qt::BDiagPattern)); p.setPen(Qt::NoPen); p.drawRect(r);
    p.restore();
}
void ferriteRing(QPainter &p, QPointF c, double Ro, double Ri)
{
    QPainterPath ring; ring.setFillRule(Qt::OddEvenFill); ring.addEllipse(c, Ro, Ro); ring.addEllipse(c, Ri, Ri);
    QRadialGradient g(c - QPointF(Ro * 0.08, Ro * 0.10), Ro);
    double mid = (Ri + Ro) / 2 / Ro;
    g.setColorAt(0, QColor("#2c2e32")); g.setColorAt(std::max(0.0, Ri / Ro - 0.02), QColor("#33363b")); g.setColorAt(mid, QColor("#7b8088"));
    g.setColorAt(std::min(1.0, mid + 0.18), QColor("#4c5056")); g.setColorAt(1, QColor("#2e3034"));
    p.save(); p.setPen(QPen(Qt::black, 1.6)); p.setBrush(g); p.drawPath(ring); p.restore();
}
QPair<double, double> solenoid(QPainter &p, QPointF c, double formDia, double windLen, int turns, double wireW, const QColor &wire,
                                const QColor &formMid, double formLen)
{
    double R = formDia / 2, x0 = c.x() - formLen / 2, x1 = c.x() + formLen / 2, ex = std::max(6.0, R * 0.22);
    QLinearGradient g(0, c.y() - R, 0, c.y() + R);
    g.setColorAt(0, formMid.darker(150)); g.setColorAt(0.28, formMid.lighter(135)); g.setColorAt(0.55, formMid); g.setColorAt(1, formMid.darker(190));
    p.save(); p.setPen(QPen(Qt::black, 1.3)); p.setBrush(g);
    QPainterPath body; body.moveTo(x0, c.y() - R); body.lineTo(x1, c.y() - R); body.arcTo(QRectF(x1 - ex, c.y() - R, 2 * ex, 2 * R), 90, -180);
    body.lineTo(x0, c.y() + R); body.arcTo(QRectF(x0 - ex, c.y() - R, 2 * ex, 2 * R), 270, -180); body.closeSubpath(); p.drawPath(body);
    p.setBrush(formMid.darker(120)); p.drawEllipse(QRectF(x0 - ex, c.y() - R, 2 * ex, 2 * R));   // open end
    p.setBrush(formMid.darker(240)); p.drawEllipse(QRectF(x0 - ex * 0.72, c.y() - R * 0.86, 1.44 * ex, 1.72 * R));
    p.restore();
    int n = std::max(1, turns); double w0 = c.x() - windLen / 2, pitch = n > 1 ? windLen / n : windLen;
    for (int i = 0; i < n; i++) {   // front half of each turn: slanted half-pitch, Coil64 style
        double xa = w0 + i * pitch + wireW / 2, xb = xa + pitch / 2;
        QPainterPath t(QPointF(xa, c.y() - R - wireW * 0.25)); t.cubicTo(QPointF(xa + pitch * 0.05, c.y() - R * 0.35), QPointF(xb - pitch * 0.05, c.y() + R * 0.35), QPointF(xb, c.y() + R + wireW * 0.25));
        tube(p, t, wireW, wire);
    }
    return {w0, w0 + windLen};
}
void parallelLC(QPainter &p, QPointF left, QPointF right, const QString &lTxt, const QString &cTxt, bool coreL)
{
    p.save(); p.setPen(QPen(Qt::black, 2)); p.setBrush(Qt::NoBrush);
    double xl = left.x() + 40, xr = right.x() - 40, yc = left.y(), yu = yc - 46, yd = yc + 46;
    p.drawLine(left, QPointF(xl, yc)); p.drawLine(QPointF(xr, yc), right);
    p.setBrush(Qt::black); p.drawEllipse(QPointF(xl, yc), 3.2, 3.2); p.drawEllipse(QPointF(xr, yc), 3.2, 3.2); p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(xl, yu), QPointF(xl, yd)); p.drawLine(QPointF(xr, yu), QPointF(xr, yd));
    // inductor: four bumps
    double span = xr - xl, cw = std::min(120.0, span * 0.62), c0 = xl + (span - cw) / 2, bw = cw / 4;
    p.drawLine(QPointF(xl, yu), QPointF(c0, yu)); p.drawLine(QPointF(c0 + cw, yu), QPointF(xr, yu));
    QPainterPath coil(QPointF(c0, yu)); for (int i = 0; i < 4; i++) coil.arcTo(QRectF(c0 + i * bw, yu - bw / 2, bw, bw), 180, -180);
    p.drawPath(coil);
    if (coreL) { p.drawLine(QPointF(c0, yu - bw / 2 - 5), QPointF(c0 + cw, yu - bw / 2 - 5)); p.drawLine(QPointF(c0, yu - bw / 2 - 9), QPointF(c0 + cw, yu - bw / 2 - 9)); }
    // capacitor
    double cx = (xl + xr) / 2; p.drawLine(QPointF(xl, yd), QPointF(cx - 5, yd)); p.drawLine(QPointF(cx + 5, yd), QPointF(xr, yd));
    p.setPen(QPen(Qt::black, 3)); p.drawLine(QPointF(cx - 5, yd - 15), QPointF(cx - 5, yd + 15)); p.drawLine(QPointF(cx + 5, yd - 15), QPointF(cx + 5, yd + 15));
    p.setPen(Qt::black); p.setFont(serif(18, true));
    p.drawText(QRectF(c0, yu - bw / 2 - (coreL ? 34 : 26), cw, 22), Qt::AlignCenter, "L");
    p.drawText(QRectF(cx - 40, yd + 15, 80, 22), Qt::AlignCenter, "C");
    p.setFont(serif(14)); p.drawText(QRectF(xl - 60, yu - bw / 2 - (coreL ? 52 : 44), span + 120, 18), Qt::AlignCenter, lTxt);
    p.drawText(QRectF(xl - 60, yd + 36, span + 120, 18), Qt::AlignCenter, cTxt);
    p.restore();
}
} // namespace draw

using namespace draw;
static QString inch(double v, int d) { return f(v, d) + S("″"); }

/* ------------------------------------------------------------------ coax trap: 720 x 300 */
void paintCoaxTrap(QPainter &p, const QRectF &rect, const CoaxTrapResult &d)
{
    p.save(); p.setRenderHint(QPainter::Antialiasing); fit(p, rect, 720, 300);
    double D = d.input.formOdIn, Lw = std::max(d.windingLenIn, 0.1), ppi = std::min(120.0 / std::max(D, 0.1), 300.0 / Lw);
    double dpx = D * ppi, lpx = Lw * ppi, formLen = lpx + 70;
    QPointF c(235, 140);
    int n = std::max(1, int(std::ceil(d.n - 1e-6))); bool many = n > 40; if (many) n = 40;
    double ww = std::max(3.0, std::min(14.0, d.coax.odIn * ppi));
    auto span = solenoid(p, c, dpx, lpx, n, ww, coaxJacket(), QColor("#d9dce1"), formLen);
    // leads (Coil64 orange-red wire ends)
    double top = c.y() - dpx / 2;
    tubeLine(p, QPointF(span.first + ww / 2, top - 2), QPointF(span.first - 18, top - 38), ww * 0.8, coaxJacket());
    tubeLine(p, QPointF(span.second, c.y() + dpx / 2), QPointF(span.second + 24, top - 38), ww * 0.8, coaxJacket());
    p.setPen(Qt::black); p.setFont(serif(15, true));
    p.drawText(QRectF(span.first - 80, top - 62, 120, 20), Qt::AlignCenter, "1");
    p.drawText(QRectF(span.second - 36, top - 62, 120, 20), Qt::AlignCenter, "2");
    dim(p, QPointF(c.x() - formLen / 2 - 8, c.y() - dpx / 2), QPointF(c.x() - formLen / 2 - 8, c.y() + dpx / 2), 22, "D", true);
    dim(p, QPointF(span.first, c.y() + dpx / 2 + 6), QPointF(span.second, c.y() + dpx / 2 + 6), 18, "l", true);
    p.setPen(Qt::black); p.setFont(serif(14));
    QString cap = QString("D = %1 form,  l = %2,  %3 turns of %4%5").arg(inch(D, 3), inch(d.windingLenIn, 2), f(d.n, 2), d.coax.label.section(' ', 0, 0),
                    many ? S(" (40 drawn)") : QString());
    p.drawText(QRectF(10, 262, 450, 18), Qt::AlignLeft | Qt::AlignVCenter, cap);
    p.drawText(QRectF(10, 280, 450, 18), Qt::AlignLeft | Qt::AlignVCenter, S("pitch %1 · cut %2 of coax · ends 1, 2: see build steps").arg(inch(d.pitchIn, 3), inch(d.cutLenIn, 1)));
    // equivalent circuit
    p.setPen(QPen(QColor("#9aa0a8"), 1, Qt::DashLine)); p.drawLine(QPointF(478, 20), QPointF(478, 280));
    p.setPen(Qt::black); p.setFont(serif(15, true)); p.drawText(QRectF(490, 14, 220, 20), Qt::AlignCenter, "Equivalent circuit");
    parallelLC(p, QPointF(495, 150), QPointF(705, 150), S("L = %1 µH").arg(f(d.L_uH, 3)), S("C = %1 pF (coax)").arg(f(d.C_pF, 1)));
    p.setFont(serif(15)); p.drawText(QRectF(490, 250, 220, 20), Qt::AlignCenter, S("f₀ = %1 MHz,  X = %2 Ω").arg(f(d.input.fMHz, 3), rnd(d.X)));
    p.setFont(serif(12)); p.drawText(QRectF(490, 152, 50, 16), Qt::AlignLeft, "wire"); p.drawText(QRectF(660, 152, 50, 16), Qt::AlignRight, "wire");
    p.restore();
}

/* ------------------------------------------------------------------ LC trap: 720 x 300 */
void paintLcTrap(QPainter &p, const QRectF &rect, const LcTrapResult &d)
{
    p.save(); p.setRenderHint(QPainter::Antialiasing); fit(p, rect, 720, 300);
    p.setPen(Qt::black);
    if (d.hasToroid && d.toroid.core) {
        const Core &k = *d.toroid.core; QPointF c(200, 130); double Ro = 92, Ri = std::max(30.0, Ro * k.idia / std::max(k.od, 0.1));
        ferriteRing(p, c, Ro, Ri);
        int N = std::max(1, d.toroid.N), drawn = std::min(N, 48); double sw = std::max(2.5, std::min(7.0, 150.0 / drawn));
        for (int i = 0; i < drawn; i++) { double a = (-90 + 25 + i * 310.0 / std::max(1, drawn - 1)) * PI / 180;
            tubeLine(p, QPointF(c.x() + (Ri - 6) * std::cos(a), c.y() + (Ri - 6) * std::sin(a)), QPointF(c.x() + (Ro + 6) * std::cos(a), c.y() + (Ro + 6) * std::sin(a)), sw, wireYellow()); }
        for (double a0 : {-65.0, 245.0}) { double a = a0 * PI / 180; QPointF o(c.x() + (Ro + 6) * std::cos(a), c.y() + (Ro + 6) * std::sin(a));
            tubeLine(p, o, o + QPointF(std::cos(a) * 28, -26), sw, leadRed()); }
        dim(p, QPointF(c.x() - Ro, c.y()), QPointF(c.x() + Ro, c.y()), Ro + 18, "OD", true);
        dim(p, QPointF(c.x() - Ri, c.y()), QPointF(c.x() + Ri, c.y()), 0, "ID");
        p.setFont(serif(14)); p.setPen(Qt::black);
        p.drawText(QRectF(10, 262, 450, 18), Qt::AlignLeft, S("%1 turns on %2%3  (OD %4 mm, ID %5 mm, h %6 mm)").arg(N).arg(k.id, d.input.stack > 1 ? S(" ×") + jsNum(d.input.stack) : QString(), f(k.od, 1), f(k.idia, 1), f(k.h, 1)));
        if (N > drawn) p.drawText(QRectF(10, 280, 450, 18), Qt::AlignLeft, QString("(%1 of %2 turns drawn)").arg(drawn).arg(N));
    } else if (d.hasCoil) {
        double D = d.input.formDiaMM, L = std::max(d.coil.lenMM, 1.0), ppm = std::min(130.0 / std::max(D, 1.0), 320.0 / L);
        int n = std::max(1, int(std::ceil(d.coil.N - 1e-6))); bool many = n > 40; if (many) n = 40;
        double wd = 2.0; if (d.coil.wire) { double dw = wireDiaMM(*d.coil.wire); wd = std::isnan(dw) ? d.coil.wire->od : std::max(dw, std::isnan(d.coil.wire->od) ? 0.0 : d.coil.wire->od); if (!(wd > 0)) wd = 2.0; }
        double ww = std::max(2.5, std::min(10.0, wd * ppm));
        QPointF c(235, 130); double formLen = L * ppm + 60;
        auto span = solenoid(p, c, D * ppm, L * ppm, n, ww, wireYellow(), QColor("#c07a3a"), formLen);   // Coil64's copper-brown former
        double top = c.y() - D * ppm / 2;
        tubeLine(p, QPointF(span.first + ww / 2, top), QPointF(span.first - 16, top - 34), ww, leadRed());
        tubeLine(p, QPointF(span.second, c.y() + D * ppm / 2), QPointF(span.second + 22, top - 34), ww, leadRed());
        dim(p, QPointF(c.x() - formLen / 2 - 8, c.y() - D * ppm / 2), QPointF(c.x() - formLen / 2 - 8, c.y() + D * ppm / 2), 22, "D", true);
        dim(p, QPointF(span.first, c.y() + D * ppm / 2 + 6), QPointF(span.second, c.y() + D * ppm / 2 + 6), 18, "l", true);
        p.setFont(serif(14)); p.setPen(Qt::black);
        p.drawText(QRectF(10, 262, 460, 18), Qt::AlignLeft, S("D = %1 mm,  l = %2 mm,  %3 turns, pitch %4 mm%5").arg(f(D, 1), f(d.coil.lenMM, 1), f(d.coil.N, 2), f(d.coil.pitchMM, 2), many ? S(" (40 drawn)") : QString()));
    }
    p.setPen(QPen(QColor("#9aa0a8"), 1, Qt::DashLine)); p.drawLine(QPointF(478, 20), QPointF(478, 280));
    p.setPen(Qt::black); p.setFont(serif(15, true)); p.drawText(QRectF(490, 14, 220, 20), Qt::AlignCenter, "Trap circuit");
    parallelLC(p, QPointF(495, 150), QPointF(705, 150), S("L = %1 µH").arg(f(d.L_uH, 3)), S("C = %1 pF, ≥ %2 V").arg(f(d.C_pF, 1), jsNum(d.capRatingV)), d.hasToroid);
    p.setFont(serif(15)); p.drawText(QRectF(490, 250, 220, 20), Qt::AlignCenter, S("f₀ = %1 MHz,  X = %2 Ω").arg(f(d.input.fMHz, 3), rnd(d.X)));
    p.setFont(serif(12)); p.drawText(QRectF(490, 152, 50, 16), Qt::AlignLeft, "wire"); p.drawText(QRectF(660, 152, 50, 16), Qt::AlignRight, "wire");
    p.restore();
}

/* ------------------------------------------------------------------ end-fed long wire: 760 x 300 */
void paintLongWire(QPainter &p, const QRectF &rect, const LongWireInfo &d)
{
    p.save(); p.setRenderHint(QPainter::Antialiasing); fit(p, rect, 760, 300);
    const double gy = 246;
    // ground
    p.setPen(QPen(Qt::black, 1.6)); p.drawLine(QPointF(10, gy), QPointF(750, gy));
    p.setPen(QPen(Qt::black, 0.8)); for (double x = 16; x < 750; x += 12) p.drawLine(QPointF(x, gy), QPointF(x - 8, gy + 8));
    // support at the far end
    QRectF pole(712, 46, 10, gy - 46); QLinearGradient pg(pole.left(), 0, pole.right(), 0); pg.setColorAt(0, QColor("#6d5a3e")); pg.setColorAt(0.4, QColor("#b89668")); pg.setColorAt(1, QColor("#5a4a33"));
    p.setPen(QPen(Qt::black, 1)); p.setBrush(pg); p.drawRect(pole);
    // transformer box
    QRectF box(118, gy - 50, 56, 50); QLinearGradient bg(0, box.top(), 0, box.bottom()); bg.setColorAt(0, QColor("#6c7076")); bg.setColorAt(0.35, QColor("#8a8f96")); bg.setColorAt(1, QColor("#3b3e43"));
    p.setBrush(bg); p.setPen(QPen(Qt::black, 1.3)); p.drawRoundedRect(box, 4, 4);
    p.setPen(Qt::white); p.setFont(serif(17, true)); p.drawText(box, Qt::AlignCenter, d.ratio.isEmpty() ? S("9:1") : d.ratio);
    // ground rod
    p.setPen(QPen(QColor("#8a5a2a"), 4)); p.drawLine(QPointF(132, gy), QPointF(132, gy + 34));
    // coax to the station
    QPainterPath cx(QPointF(118, gy - 18)); cx.cubicTo(QPointF(80, gy - 18), QPointF(70, gy - 6), QPointF(18, gy - 6)); tube(p, cx, 6, coaxJacket());
    p.setPen(Qt::black); p.setFont(serif(13)); p.drawText(QRectF(10, gy + 10, 165, 18), Qt::AlignLeft, S("coax → choke → tuner"));
    // counterpoise along the ground
    p.setPen(QPen(leadRed().darker(130), 2, Qt::DashLine)); p.drawLine(QPointF(174, gy - 4), QPointF(420, gy - 4));
    p.setPen(Qt::black); p.drawText(QRectF(180, gy + 10, 300, 18), Qt::AlignLeft, d.counterpoiseFt > 0 ? S("counterpoise ≈ %1 ft (0.05 λ)").arg(f(d.counterpoiseFt, 0)) : S("counterpoise"));
    // radiator
    QPointF a(146, gy - 50), b(712, 52);
    tubeLine(p, a, b, 5, wireYellow());
    p.setBrush(Qt::white); p.setPen(QPen(Qt::black, 1)); p.drawRoundedRect(QRectF(b.x() - 4, b.y() - 5, 12, 10), 3, 3);   // end insulator
    dim(p, a, b, -26, S("L = %1 ft (%2 m)").arg(f(d.lengthFt, 1), f(d.lengthFt * 0.3048, 2)));
    // traps
    std::vector<TrapSuggestion> ts = d.traps; std::sort(ts.begin(), ts.end(), [](const TrapSuggestion &x, const TrapSuggestion &y) { return x.distanceFt < y.distanceFt; });
    std::vector<std::pair<double, int>> placed;   // label x and level, to avoid overlapping labels
    for (const TrapSuggestion &t : ts) {
        if (d.lengthFt <= 0 || t.distanceFt <= 0 || t.distanceFt >= d.lengthFt) continue;
        double u = t.distanceFt / d.lengthFt; QPointF q = a + (b - a) * u;
        QRectF tr(q.x() - 12, q.y() - 8, 24, 16); QLinearGradient tg(0, tr.top(), 0, tr.bottom()); tg.setColorAt(0, QColor("#2e2e2e")); tg.setColorAt(0.35, QColor("#777")); tg.setColorAt(1, QColor("#222"));
        p.save(); p.translate(q); p.rotate(std::atan2(b.y() - a.y(), b.x() - a.x()) * 180 / PI); p.translate(-q);
        p.setBrush(tg); p.setPen(QPen(Qt::black, 1)); p.drawRoundedRect(tr, 4, 4); p.restore();
        double lx = std::min(q.x(), 630.0); int level = 0;
        for (auto &pl : placed) if (std::fabs(pl.first - lx) < 140 && pl.second >= level) level = pl.second + 1;
        placed.push_back({lx, level});
        double off = 30 + level * 36;
        p.setPen(QPen(Qt::black, 0.8)); p.drawLine(q + QPointF(0, 9), QPointF(lx, q.y() + off - 13));
        p.setFont(serif(13, true)); p.drawText(QRectF(lx - 70, q.y() + off - 12, 140, 16), Qt::AlignCenter, t.band + " trap");
        p.setFont(serif(12)); p.drawText(QRectF(lx - 70, q.y() + off + 3, 140, 16), Qt::AlignCenter, S("%1 MHz @ %2 ft").arg(f(t.fMHz, 2), f(t.distanceFt, 1)));
    }
    p.setPen(Qt::black); p.setFont(serif(14, true));
    p.drawText(QRectF(10, 8, 690, 20), Qt::AlignLeft, p.fontMetrics().elidedText(d.transformer.isEmpty() ? S("End-fed long wire") : d.transformer, Qt::ElideRight, 690));
    p.restore();
}

/* ------------------------------------------------------------------ avoid-zone chart (web app zoneSVG): 900 x H */
int zoneChartHeight(int nBands) { return 10 + 22 * nBands + 40; }
void paintZoneChart(QPainter &p, const QRectF &rect, const std::vector<AvoidZone> &zones, const QStringList &bandIds, double mn, double mx, double len,
                    const std::vector<std::pair<double, double>> &good)
{
    const double W = 900, L = 60, R = 20, T = 10, rowH = 22, H = zoneChartHeight(bandIds.size()), pw = W - L - R;
    p.save(); p.setRenderHint(QPainter::Antialiasing); fit(p, rect, W, H);
    if (mx <= mn) { p.restore(); return; }
    auto X = [&](double ft) { return L + pw * (ft - mn) / (mx - mn); };
    QFont fn = p.font(); fn.setPixelSize(12); fn.setBold(false); fn.setItalic(false); p.setFont(fn);
    for (auto &g : good) p.fillRect(QRectF(X(g.first), T, X(g.second) - X(g.first), rowH * bandIds.size()), QColor(31, 122, 58, 46));
    for (int i = 0; i < bandIds.size(); i++) {
        double y = T + i * rowH; p.setPen(Qt::black); p.drawText(QRectF(0, y, L - 8, rowH), Qt::AlignRight | Qt::AlignVCenter, bandIds[i]);
        p.setPen(QPen(QColor("#d5dbe3"), 1)); p.drawLine(QPointF(L, y + rowH), QPointF(W - R, y + rowH));
        for (const AvoidZone &z : zones) { if (z.band != bandIds[i] || z.hiFt <= mn || z.loFt >= mx) continue;
            double xa = X(std::max(z.loFt, mn)), xb = X(std::min(z.hiFt, mx)); p.fillRect(QRectF(xa, y + 4, std::max(1.5, xb - xa), rowH - 8), QColor(208, 52, 44, 191)); }
    }
    double step = (mx - mn) > 200 ? 25 : 10; p.setPen(Qt::black);
    for (double t = std::ceil(mn / step) * step; t <= mx + 1e-9; t += step) p.drawText(QRectF(X(t) - 25, H - 32, 50, 14), Qt::AlignCenter, jsNum(t));
    p.drawText(QRectF(L, H - 16, pw, 14), Qt::AlignCenter, S("Wire length (ft) — red = n·λ/2 avoid zone, green = good gap for the selected bands"));
    if (len >= mn && len <= mx) { p.setPen(QPen(Qt::black, 2.5)); p.drawLine(QPointF(X(len), T - 4), QPointF(X(len), T + rowH * bandIds.size() + 2));
        QFont bf = fn; bf.setBold(true); p.setFont(bf); p.drawText(QPointF(X(len) + 4, T + 8), jsNum(len) + " ft"); }
    p.restore();
}

QImage renderPaper(const PaintFn &fn, int w, int h)
{
    QImage img(w * 2, h * 2, QImage::Format_ARGB32); img.fill(Qt::white);
    QPainter p(&img); p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::black); p.scale(2, 2); fn(p, QRectF(0, 0, w, h)); return img;
}

PaperView::PaperView(int w, int h, QWidget *parent) : QWidget(parent), m_w(w), m_h(h)
{
    setObjectName("ke9esuDiagram"); setAttribute(Qt::WA_StyledBackground, true);
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred); sp.setHeightForWidth(true); setSizePolicy(sp);
}
int PaperView::heightForWidth(int w) const { return int(std::lround(double(w) * m_h / m_w)) + 12; }
QSize PaperView::sizeHint() const { return QSize(m_w, m_h + 12); }
QSize PaperView::minimumSizeHint() const { return QSize(240, int(240.0 * m_h / m_w) + 12); }
void PaperView::paintEvent(QPaintEvent *)
{
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const ThemeColors &c = theme(); p.setPen(QPen(c.line, 1)); p.setBrush(c.paper); p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    if (!m_fn) return;
    p.setPen(c.paperInk); m_fn(p, QRectF(rect()).adjusted(6, 6, -6, -6));
}

} // namespace ke9esu
