/* netdiagram.h - KE9ESU fork of Coil64: wiring (schematic) diagram of a standard-part combination - the drawing the
   KE9ESU Component Combiner shows under every result, redrawn in the Milestone 2 Coil64 drawing style (black ink on
   paper, italic serif values, bold designators, junction dots, open terminals). Same layout rules as the web app:
   series parts side by side, parallel branches stacked between two rails, nested to any depth.
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_NETDIAGRAM_H
#define KE9ESU_NETDIAGRAM_H
#include <QSizeF>
#include <QRectF>
#include <QImage>
#include <QColor>
#include "combiner_core.h"
class QPainter;
namespace ke9esu {
namespace net {
struct Options {
    bool designators = true;      // R1, R2 … numbered left to right / top to bottom (the order of the parts list)
    QString caption;              // optional line under the drawing ("A–B = 1.2335 kΩ  (−0.04 %)")
    QColor ink = Qt::black;       // drawings are on paper: black ink in every theme, like Coil64's pictures
};
/* logical size (font 14 px units) of the drawing of tree */
QSizeF size(const cc::Node &tree, QChar type, const Options &o = Options());
/* draw the network scaled to fit r (never above maxScale), centred; returns the scale used */
double paint(QPainter &p, const QRectF &r, const cc::Node &tree, QChar type, const Options &o = Options(), double maxScale = 1e9);
/* the drawing on white paper at scale s (for build sheets, screenshots and tests) */
QImage render(const cc::Node &tree, QChar type, double s = 2, const Options &o = Options());
/* designator prefix for a part type: R, C, L */
inline QString prefix(QChar type) { return QString(type == 'C' ? 'C' : type == 'L' ? 'L' : 'R'); }
}
}
#endif
