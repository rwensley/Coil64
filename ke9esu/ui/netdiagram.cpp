/* netdiagram.cpp - see netdiagram.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "netdiagram.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include <algorithm>
#include <cmath>

namespace ke9esu {
namespace net {
namespace {
// Coil64 drawing fonts (see drawings.cpp): italic serif values, bold upright serif designators
QFont valFont() { QFont f(QStringLiteral("Times New Roman")); f.setStyleHint(QFont::Serif); f.setFamilies({"Times New Roman", "Liberation Serif", "DejaVu Serif", "serif"}); f.setPixelSize(14); f.setItalic(true); return f; }
QFont desFont() { QFont f = valFont(); f.setItalic(false); f.setBold(true); return f; }
const double LEAF_H = 52, LEAF_MID = 34, GAP = 12, PAD = 18, VGAP = 10, LEAD = 22, EDGE = 6, CAP_H = 26;

struct Box {
    double w = 0, h = 0, mid = 0;
    bool leaf = true; QString des, val; int op = 0;
    std::vector<Box> kids; std::vector<double> ys;
};
Box layout(const cc::Node &n, QChar type, const Options &o, int &num)
{
    Box b; b.leaf = n.leaf; b.op = n.op;
    if (n.leaf) {
        b.val = cc::fmt(n.v, 4, cc::partType(type).unit); if (o.designators) b.des = prefix(type) + QString::number(++num);
        double tw = QFontMetricsF(valFont()).horizontalAdvance(b.val) + (b.des.isEmpty() ? 0 : QFontMetricsF(desFont()).horizontalAdvance(b.des + " "));
        b.w = std::max(84.0, tw + 16); b.h = LEAF_H; b.mid = LEAF_MID; return b;
    }
    for (const cc::Node &k : n.kids) b.kids.push_back(layout(k, type, o, num));
    if (n.op == 1) {   // series: side by side on one through-wire
        double above = 0, below = 0, W = 0;
        for (size_t i = 0; i < b.kids.size(); i++) { above = std::max(above, b.kids[i].mid); below = std::max(below, b.kids[i].h - b.kids[i].mid); W += b.kids[i].w + (i ? GAP : 0); }
        b.w = W; b.h = above + below; b.mid = above; return b;
    }
    double mw = 0, H = 0;   // parallel: branches stacked between two rails
    for (size_t i = 0; i < b.kids.size(); i++) { mw = std::max(mw, b.kids[i].w); b.ys.push_back(H); H += b.kids[i].h + VGAP; }
    H -= VGAP;
    double top = b.ys.front() + b.kids.front().mid, bot = b.ys.back() + b.kids.back().mid;
    b.w = mw + 2 * PAD; b.h = H; b.mid = (top + bot) / 2; return b;
}
void dot(QPainter &p, QPointF c) { p.save(); p.setBrush(p.pen().color()); p.setPen(Qt::NoPen); p.drawEllipse(c, 3.0, 3.0); p.restore(); }
void symbol(QPainter &p, QChar type, double x, double y, double w)
{
    double c = x + w / 2;
    if (type == 'C') {
        p.drawLine(QPointF(x, y), QPointF(c - 5, y)); p.drawLine(QPointF(c + 5, y), QPointF(x + w, y));
        QPen pp = p.pen(); QPen thick = pp; thick.setWidthF(3); thick.setCapStyle(Qt::FlatCap); p.setPen(thick);
        p.drawLine(QPointF(c - 5, y - 13), QPointF(c - 5, y + 13)); p.drawLine(QPointF(c + 5, y - 13), QPointF(c + 5, y + 13)); p.setPen(pp);
    } else if (type == 'L') {   // four bumps, as in Coil64's LC drawings
        double L = 40, x0 = c - L / 2, bw = L / 4; p.drawLine(QPointF(x, y), QPointF(x0, y)); p.drawLine(QPointF(x0 + L, y), QPointF(x + w, y));
        QPainterPath coil(QPointF(x0, y)); for (int i = 0; i < 4; i++) coil.arcTo(QRectF(x0 + i * bw, y - bw / 2, bw, bw), 180, -180);
        p.setBrush(Qt::NoBrush); p.drawPath(coil);
    } else {                    // resistor zig-zag (ANSI, as in the web app)
        double L = 40, x0 = c - L / 2; QPainterPath z(QPointF(x0, y));
        for (int i = 0; i < 6; i++) z.lineTo(x0 + L * (i + 0.5) / 6, y + (i % 2 ? 7 : -7)); z.lineTo(x0 + L, y);
        p.drawLine(QPointF(x, y), QPointF(x0, y)); p.setBrush(Qt::NoBrush); p.drawPath(z); p.drawLine(QPointF(x0 + L, y), QPointF(x + w, y));
    }
}
void draw(QPainter &p, const Box &b, QChar type, double x, double y)
{
    if (b.leaf) {
        symbol(p, type, x, y + b.mid, b.w);
        QFontMetricsF fd(desFont()), fv(valFont());
        double dw = b.des.isEmpty() ? 0 : fd.horizontalAdvance(b.des + " "), tw = dw + fv.horizontalAdvance(b.val), tx = x + (b.w - tw) / 2, base = y + 13;
        if (!b.des.isEmpty()) { p.setFont(desFont()); p.drawText(QPointF(tx, base), b.des); }
        p.setFont(valFont()); p.drawText(QPointF(tx + dw, base), b.val);
        return;
    }
    if (b.op == 1) {
        double cx = x, my = y + b.mid;
        for (size_t i = 0; i < b.kids.size(); i++) { if (i) { p.drawLine(QPointF(cx, my), QPointF(cx + GAP, my)); cx += GAP; } draw(p, b.kids[i], type, cx, my - b.kids[i].mid); cx += b.kids[i].w; }
        return;
    }
    double mw = b.w - 2 * PAD, xl = x + PAD / 2, xr = x + b.w - PAD / 2;
    double top = y + b.ys.front() + b.kids.front().mid, bot = y + b.ys.back() + b.kids.back().mid, my = y + b.mid;
    p.drawLine(QPointF(x, my), QPointF(xl, my)); p.drawLine(QPointF(xr, my), QPointF(x + b.w, my));
    p.drawLine(QPointF(xl, top), QPointF(xl, bot)); p.drawLine(QPointF(xr, top), QPointF(xr, bot));
    auto junction = [&](double yy) { return yy > top + 0.5 && yy < bot - 0.5; };
    if (junction(my)) { dot(p, QPointF(xl, my)); dot(p, QPointF(xr, my)); }
    for (size_t i = 0; i < b.kids.size(); i++) {
        const Box &k = b.kids[i]; double kx = x + PAD + (mw - k.w) / 2, ky = y + b.ys[i], wy = ky + k.mid;
        p.drawLine(QPointF(xl, wy), QPointF(kx, wy)); p.drawLine(QPointF(kx + k.w, wy), QPointF(xr, wy));
        if (junction(wy) && std::fabs(wy - my) > 0.5) { dot(p, QPointF(xl, wy)); dot(p, QPointF(xr, wy)); }
        draw(p, k, type, kx, ky);
    }
}
}

QSizeF size(const cc::Node &tree, QChar type, const Options &o)
{
    int n = 0; Box b = layout(tree, type, o, n);
    double w = b.w + 2 * LEAD + 2 * EDGE, h = b.h + 2 * EDGE + (o.caption.isEmpty() ? 0 : CAP_H);
    if (!o.caption.isEmpty()) w = std::max(w, QFontMetricsF(valFont()).horizontalAdvance(o.caption) + 2 * EDGE + 8);
    return QSizeF(w, h);
}

double paint(QPainter &p, const QRectF &r, const cc::Node &tree, QChar type, const Options &o, double maxScale)
{
    int n = 0; Box b = layout(tree, type, o, n); QSizeF sz = size(tree, type, o);
    double s = std::min({r.width() / sz.width(), r.height() / sz.height(), maxScale}); if (!(s > 0)) return 0;
    p.save(); p.setRenderHint(QPainter::Antialiasing);
    p.translate(r.x() + (r.width() - sz.width() * s) / 2, r.y() + (r.height() - sz.height() * s) / 2); p.scale(s, s);
    p.setPen(QPen(o.ink, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
    double x0 = (sz.width() - (b.w + 2 * LEAD)) / 2, my = EDGE + b.mid;
    // terminals A and B (open circles) and their leads
    p.drawLine(QPointF(x0 + 4, my), QPointF(x0 + LEAD, my)); p.drawLine(QPointF(x0 + LEAD + b.w, my), QPointF(x0 + 2 * LEAD + b.w - 4, my));
    p.drawEllipse(QPointF(x0, my), 4, 4); p.drawEllipse(QPointF(x0 + 2 * LEAD + b.w, my), 4, 4);
    draw(p, b, type, x0 + LEAD, EDGE);
    if (!o.caption.isEmpty()) { p.setFont(valFont()); p.drawText(QRectF(0, EDGE + b.h + 4, sz.width(), CAP_H - 4), Qt::AlignCenter, o.caption); }
    p.restore();
    return s;
}

QImage render(const cc::Node &tree, QChar type, double s, const Options &o)
{
    QSizeF sz = size(tree, type, o);
    QImage img(int(std::ceil(sz.width() * s)), int(std::ceil(sz.height() * s)), QImage::Format_ARGB32); img.fill(Qt::white);
    QPainter p(&img); paint(p, QRectF(0, 0, img.width(), img.height()), tree, type, o, s); return img;
}
}
}
