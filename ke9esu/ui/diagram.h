/* diagram.h - KE9ESU fork of Coil64: winding diagram and impedance plot of a balun/unun/choke design (redrawn in
   Coil64's drawing style in Milestone 2; drawn on paper so they stay readable in dark themes). Copyright (C) 2026 KE9ESU. GPL-3.0-or-later. */
#ifndef KE9ESU_DIAGRAM_H
#define KE9ESU_DIAGRAM_H
#include <QWidget>
#include <QImage>
#include "balun_core.h"
namespace ke9esu {
void paintWinding(QPainter &p, const QRectF &r, const DesignResult &d);
void paintImpedance(QPainter &p, const QRectF &r, const DesignResult &d);
const int kWindW = 460, kWindH = 500;   // logical size of the winding drawing (without the shunt-capacitor strip)
const int kCapStripH = 80;              // v1.3.2: low-side shunt capacitor strip under the winding (SO-239 centre -> shell)
inline int windH(const DesignResult &d) { return kWindH + (d.hasCompCap ? kCapStripH : 0); }
QImage renderWinding(const DesignResult &d, int w = kWindW, int h = 0);   // h 0 = windH(d)
QImage renderImpedance(const DesignResult &d, int w = 900, int h = 360);
/* v1.3.2 transformer frequency sweep: SWR (and the load-range family), R / X / |Z|, return loss and efficiency, ham bands
   shaded; cursorMHz draws a cursor line. sweepAtX maps a widget x back to the nearest sweep point (-1 = none). */
void paintSweep(QPainter &p, const QRectF &r, const DesignResult &d, double cursorMHz = NaN);
QImage renderSweep(const DesignResult &d, int w = 900, int h = 560);
int sweepAtX(const QRectF &r, const DesignResult &d, double x);
QString sweepPointText(const SweepPoint &p);
QString sweepCsv(const DesignResult &d);

class DesignView : public QWidget
{
    Q_OBJECT
public:
    enum Kind { Winding, Impedance, Sweep };
    DesignView(Kind k, QWidget *parent = nullptr);
    void setDesign(const DesignResult &d) { m_d = d; m_has = true; if (m_kind == Winding) updateGeometry(); update(); }
    QSize sizeHint() const override;
    bool hasHeightForWidth() const override { return m_kind == Winding; }
    int heightForWidth(int w) const override;
signals:
    void hovered(const QString &text);   // Sweep: readout of the point under the mouse
protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override { mouseMoveEvent(e); }
private:
    QRectF plotRect() const;
    Kind m_kind; DesignResult m_d; bool m_has = false; double m_cursor = NaN;
};
}
#endif
