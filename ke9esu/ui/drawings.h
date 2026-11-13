/* drawings.h - KE9ESU fork of Coil64: build drawings in Coil64's own drawing style (3-D shaded wire tubes, shaded
   ferrite, hatched sections, black dimension lines with arrowheads, italic serif labels) for the RF windows:
   coax trap, LC trap and end-fed long wire. Drawn on "paper" (white / light grey) so they stay readable in dark
   themes and print exactly as shown. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_DRAWINGS_H
#define KE9ESU_DRAWINGS_H
#include <QWidget>
#include <QImage>
#include <QPainterPath>
#include <functional>
#include "trap_core.h"
#include "eflw_core.h"

namespace ke9esu {
namespace draw {
QFont serif(double px, bool bold = false, bool italic = true);
QColor wireYellow();   // Coil64 winding wire
QColor wireOrange();   // second winding / Coil64 wire ends
QColor wireBlue();     // third winding (trifilar)
QColor leadRed();      // leads and taps
QColor coaxJacket();
/* scale a logical W x H drawing uniformly into r (centred); call p.save() before and p.restore() after */
void fit(QPainter &p, const QRectF &r, double W, double H);
void tube(QPainter &p, const QPainterPath &path, double w, const QColor &base);
void tubeLine(QPainter &p, QPointF a, QPointF b, double w, const QColor &base);
void arrowHead(QPainter &p, QPointF tip, QPointF from, double size = 9);
/* dimension a-b, offset perpendicular by off (signed); label placed beside the line (below if labelBelow) */
void dim(QPainter &p, QPointF a, QPointF b, double off, const QString &label, bool labelBelow = false);
void hatched(QPainter &p, const QRectF &r, const QColor &fill = QColor("#55595f"));
void ferriteRing(QPainter &p, QPointF c, double Ro, double Ri);
/* side view of a cylindrical former with a single-layer winding; returns the winding's x span */
QPair<double, double> solenoid(QPainter &p, QPointF c, double formDia, double windLen, int turns, double wireW, const QColor &wire,
                                const QColor &formMid, double formLen);
void parallelLC(QPainter &p, QPointF left, QPointF right, const QString &lTxt, const QString &cTxt, bool coreL = false);
} // namespace draw

void paintCoaxTrap(QPainter &p, const QRectF &r, const CoaxTrapResult &d);
void paintLcTrap(QPainter &p, const QRectF &r, const LcTrapResult &d);
struct LongWireInfo { double lengthFt = 0; QString transformer, ratio; std::vector<TrapSuggestion> traps; double counterpoiseFt = 0; };
void paintLongWire(QPainter &p, const QRectF &r, const LongWireInfo &d);
using PaintFn = std::function<void(QPainter &, const QRectF &)>;
int zoneChartHeight(int nBands);
void paintZoneChart(QPainter &p, const QRectF &r, const std::vector<AvoidZone> &zones, const QStringList &bandIds, double mn, double mx, double len,
                    const std::vector<std::pair<double, double>> &good);
QImage renderPaper(const PaintFn &fn, int w, int h);   // 2x, paper background, for build sheets

/* A drawing on paper that keeps its aspect ratio (height follows width). */
class PaperView : public QWidget
{
    Q_OBJECT
public:
    PaperView(int logicalW, int logicalH, QWidget *parent = nullptr);
    void setPainter(const PaintFn &fn) { m_fn = fn; update(); }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    int m_w, m_h; PaintFn m_fn;
};
} // namespace ke9esu
#endif
