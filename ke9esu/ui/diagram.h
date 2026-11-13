/* diagram.h - KE9ESU fork of Coil64: winding diagram and impedance plot of a balun/unun/choke design (QPainter port of
   the web app's SVG drawings; the Milestone 2 restyle will redraw them). Copyright (C) 2026 KE9ESU. GPL-3.0-or-later. */
#ifndef KE9ESU_DIAGRAM_H
#define KE9ESU_DIAGRAM_H
#include <QWidget>
#include <QImage>
#include "balun_core.h"
namespace ke9esu {
void paintWinding(QPainter &p, const QRectF &r, const DesignResult &d);
void paintImpedance(QPainter &p, const QRectF &r, const DesignResult &d);
QImage renderWinding(const DesignResult &d, int w = 460, int h = 470);
QImage renderImpedance(const DesignResult &d, int w = 900, int h = 360);

class DesignView : public QWidget
{
    Q_OBJECT
public:
    enum Kind { Winding, Impedance };
    DesignView(Kind k, QWidget *parent = nullptr);
    void setDesign(const DesignResult &d) { m_d = d; m_has = true; update(); }
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    Kind m_kind; DesignResult m_d; bool m_has = false;
};
}
#endif
