/* combinerdialog.cpp - see combinerdialog.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "combinerdialog.h"
#include "project.h"
#include "ke9esu_util.h"
#include <QBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QThread>
#include <QMenu>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSplitter>
#include <QGridLayout>
#include <QScrollArea>
#include <QTimer>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>
#include "netdiagram.h"
#include "theme.h"
#include <cmath>
#include <functional>

namespace ke9esu {

namespace {
class FnThread : public QThread {
public:
    explicit FnThread(std::function<void()> fn) : m_fn(std::move(fn)) {}
protected:
    void run() override { m_fn(); }
private:
    std::function<void()> m_fn;
};

int fontPx(const QFont &f) { return qMax(9, QFontInfo(f).pixelSize()); }

/* the wiring diagram of row r, on paper, in the table's last column (the web app shows it under every result) */
class NetDelegate : public QStyledItemDelegate {
public:
    NetDelegate(const std::vector<cc::Result> *res, const QChar *type, QObject *parent) : QStyledItemDelegate(parent), m_res(res), m_type(type) {}
    static double scale(const QFont &f) { return 0.9 * fontPx(f) / 14.0; }
    QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        int r = idx.data(Qt::UserRole).toInt(); if (r < 0 || r >= (int)m_res->size()) return QStyledItemDelegate::sizeHint(opt, idx);
        QSizeF sz = net::size((*m_res)[r].tree, *m_type); double s = scale(opt.font), capH = std::max(120.0, 7.5 * fontPx(opt.font));
        double h = std::min(sz.height() * s, capH), k = h / (sz.height() * s);
        return QSize(int(sz.width() * s * k) + 12, int(h) + 10);
    }
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyleOptionViewItem o2(opt); initStyleOption(&o2, idx); o2.text.clear();   // selection / row background only
        const QWidget *w = opt.widget; (w ? w->style() : QApplication::style())->drawControl(QStyle::CE_ItemViewItem, &o2, p, w);
        int r = idx.data(Qt::UserRole).toInt(); if (r < 0 || r >= (int)m_res->size()) return;
        const ThemeColors &c = theme(); QRectF pr = QRectF(opt.rect).adjusted(4, 4, -4, -4);
        p->save(); p->setRenderHint(QPainter::Antialiasing); p->setPen(QPen(c.line, 1)); p->setBrush(c.paper); p->drawRoundedRect(pr, 6, 6);
        net::Options o; o.ink = c.paperInk; net::paint(*p, pr.adjusted(2, 1, -2, -1), (*m_res)[r].tree, *m_type, o, scale(opt.font)); p->restore();
    }
private:
    const std::vector<cc::Result> *m_res; const QChar *m_type;
};

/* the selected combination, larger (web-app scale 1.35), with its value, error and part-tolerance spread */
class NetView : public QWidget {
public:
    explicit NetView(QWidget *parent = nullptr) : QWidget(parent) { setObjectName("ke9esuDiagram"); setMinimumHeight(90); }
    void set(const cc::Node &n, QChar type, const QString &cap) { m_n = n; m_type = type; m_cap = cap; m_has = true; updateGeometry(); update(); }
    void clear() { m_has = false; update(); }
    QSize sizeHint() const override {
        if (!m_has) return QSize(400, 120);
        net::Options o; o.caption = m_cap; QSizeF sz = net::size(m_n, m_type, o); double s = 1.35 * fontPx(font()) / 14.0;
        return QSize(int(sz.width() * s) + 16, int(sz.height() * s) + 16);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); const ThemeColors &c = theme();
        p.setPen(QPen(c.line, 1)); p.setBrush(c.paper); p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        if (!m_has) return;
        net::Options o; o.caption = m_cap; o.ink = c.paperInk; net::paint(p, QRectF(rect()).adjusted(8, 8, -8, -8), m_n, m_type, o, 1.35 * fontPx(font()) / 14.0);
    }
private:
    cc::Node m_n; QChar m_type = 'R'; QString m_cap; bool m_has = false;
};
}

enum Col { CNet, CValue, CErr, CTol, CCount, CParts, CDiagram, NCols };

CombinerDialog::CombinerDialog(QWidget *parent) : Ke9esuDialog(Win::Combiner, parent)
{
    QVBoxLayout *v = new QVBoxLayout(body()); v->setContentsMargins(0, 0, 0, 0);
    m_type = new QComboBox; m_type->addItem(tr("Resistor (Ω)"), "R"); m_type->addItem(tr("Capacitor (F)"), "C"); m_type->addItem(tr("Inductor (H)"), "L");
    m_target = new QLineEdit("1234"); m_target->setToolTip(tr("Value with SI prefix: 4k7, 1234, 2.2u, 2u2, 100n, 33p, 10mH, 1M, 4R7 … (upper-case M = mega, lower-case m = milli)")); m_mark = new FromMarker;
    m_target->setPlaceholderText(tr("e.g. 4k7, 1234, 2.2u, 100n, 33p, 10mH, 1M"));
    m_parsed = new QLabel; m_parsed->setObjectName("ke9esuHint");
    QWidget *tw = new QWidget; QVBoxLayout *tv = new QVBoxLayout(tw); tv->setContentsMargins(0, 0, 0, 0); tv->setSpacing(2);
    QHBoxLayout *th = new QHBoxLayout; th->setContentsMargins(0, 0, 0, 0); th->addWidget(m_target, 1); th->addWidget(m_mark); tv->addLayout(th); tv->addWidget(m_parsed);
    m_series = new QComboBox; for (const char *sn : {"E6", "E12", "E24", "E48", "E96", "E192"}) m_series->addItem(cc::seriesLabel(sn), sn); m_series->addItem(tr("My parts (list below)"), "custom"); m_series->setCurrentIndex(1);
    m_series->setToolTip(tr("IEC 60063 value series; the % is the usual tolerance of parts sold in that series"));
    m_seriesHint = new QLabel; m_seriesHint->setObjectName("ke9esuHint"); m_seriesHint->setWordWrap(true);
    QWidget *sw = new QWidget; QVBoxLayout *sv = new QVBoxLayout(sw); sv->setContentsMargins(0, 0, 0, 0); sv->setSpacing(2); sv->addWidget(m_series); sv->addWidget(m_seriesHint);
    m_custom = new QPlainTextEdit; m_custom->setPlaceholderText(tr("Parts I actually have, separated by spaces, commas or new lines, e.g. 100 220 470 1k 2.2k 4.7k 10k 47k 100k. Each value can be used more than once.")); m_custom->setMaximumHeight(70);
    m_customInfo = new QLabel; m_customInfo->setObjectName("ke9esuHint"); m_customInfo->setWordWrap(true);
    QWidget *cw = new QWidget; QVBoxLayout *cv = new QVBoxLayout(cw); cv->setContentsMargins(0, 0, 0, 0); cv->setSpacing(2); cv->addWidget(m_custom); cv->addWidget(m_customInfo);
    m_max = new QSpinBox; m_max->setRange(1, 6); m_max->setValue(3); m_max->setSuffix(tr(" parts (fewer is fine)"));
    m_topo = new QComboBox; m_topo->addItem(tr("Series + parallel mixed"), "mixed"); m_topo->addItem(tr("Series only"), "series"); m_topo->addItem(tr("Parallel only"), "parallel");
    m_sort = new QComboBox; m_sort->addItem(tr("Fewest parts within tolerance"), "simple"); m_sort->addItem(tr("Most accurate"), "accuracy");
    m_sort->setProperty("ke9esuTamed", true); m_sort->setSizeAdjustPolicy(QComboBox::AdjustToContents);   // short list: always show the whole choice
    m_tol = new QDoubleSpinBox; m_tol->setRange(0, 20); m_tol->setDecimals(3); m_tol->setSingleStep(0.1); m_tol->setValue(1); m_tol->setPrefix(S("± ")); m_tol->setSuffix(" %");
    m_tol->setToolTip(tr("How far the combination may be from the target and still count as a match (every row shows its own error and whether it is within this)"));
    auto cell = [](const QString &label, QWidget *field) { QWidget *w = new QWidget; QVBoxLayout *l = new QVBoxLayout(w); l->setContentsMargins(0, 0, 0, 0); l->setSpacing(3);
        QLabel *lb = new QLabel(label); QFont bf = lb->font(); bf.setBold(true); lb->setFont(bf); lb->setBuddy(field); l->addWidget(lb); l->addWidget(field); l->addStretch(1); return w; };
    m_cells = {cell(tr("Part type"), m_type), cell(tr("Target value"), tw), cell(tr("Value series"), sw), cell(tr("Max number of parts"), m_max),
               cell(tr("Acceptable error ±%"), m_tol), cell(tr("Wiring"), m_topo)};
    m_customCell = cell(tr("Parts I actually have (replaces the value series; each value can be used more than once)"), cw);
    // inputs pane (scrolls when the window is short or the text is large), results table, selected diagram: one vertical splitter
    m_inputs = new QWidget; QVBoxLayout *iv = new QVBoxLayout(m_inputs); iv->setContentsMargins(0, 0, 4, 4);
    m_grid = new QGridLayout; m_grid->setHorizontalSpacing(18); m_grid->setVerticalSpacing(8); iv->addLayout(m_grid);
    relayoutForm();
    QHBoxLayout *rh = new QHBoxLayout; m_run = new QPushButton(tr("Find combinations")); m_run->setObjectName("ke9esuPrimary"); m_prog = new QProgressBar; m_prog->setRange(0, 1000); m_status = new QLabel; m_status->setWordWrap(true);
    QLabel *sl = new QLabel(tr("Show first")); QFont sbf = sl->font(); sbf.setBold(true); sl->setFont(sbf); sl->setBuddy(m_sort);
    rh->addWidget(m_run); rh->addWidget(m_prog, 1); rh->addSpacing(12); rh->addWidget(sl); rh->addWidget(m_sort); iv->addLayout(rh); iv->addWidget(m_status);
    m_legend = new QLabel; m_legend->setObjectName("ke9esuHint"); m_legend->setWordWrap(true); iv->addWidget(m_legend);
    m_inScroll = new QScrollArea; m_inScroll->setWidget(m_inputs); m_inScroll->setWidgetResizable(true); m_inScroll->setFrameShape(QFrame::NoFrame);
    m_inScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_t = new QTableWidget(0, NCols);
    m_t->setHorizontalHeaderLabels({tr("Network"), tr("Value"), tr("Error"), tr("Tolerance"), tr("Parts"), tr("Part values"), tr("Wiring diagram")});
    m_t->verticalHeader()->hide(); m_t->setEditTriggers(QAbstractItemView::NoEditTriggers); m_t->setSelectionBehavior(QAbstractItemView::SelectRows); m_t->setSelectionMode(QAbstractItemView::SingleSelection);
    m_t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); m_t->horizontalHeader()->setStretchLastSection(true);
    m_t->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); m_t->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); m_t->setWordWrap(false);
    m_t->setItemDelegateForColumn(CDiagram, new NetDelegate(&m_sorted, &m_solType, m_t));
    QWidget *dw = new QWidget; QVBoxLayout *dv = new QVBoxLayout(dw); dv->setContentsMargins(0, 4, 0, 0); dv->setSpacing(4);
    m_detailHead = new QLabel; m_detailHead->setWordWrap(true); m_detailHead->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_detail = new NetView; dv->addWidget(m_detailHead); dv->addWidget(m_detail, 1);
    m_split = new QSplitter(Qt::Vertical); m_split->addWidget(m_inScroll); m_split->addWidget(m_t); m_split->addWidget(dw);
    m_split->setStretchFactor(0, 0); m_split->setStretchFactor(1, 3); m_split->setStretchFactor(2, 2); m_detail->setMinimumHeight(90); m_split->setChildrenCollapsible(false);
    v->addWidget(m_split, 1);
    QLabel *hint = new QLabel(S("<span>IEC 60063 E-series · ranges R 1 Ω–10 MΩ, C 1 pF–10 mF, L 1 nH–1 H · inductors assume no mutual coupling "
                                "(space them apart / at right angles) · real parts have tolerance too (spread shown above the diagram) · meet-in-the-middle search, 1…6 parts</span>")); hint->setObjectName("ke9esuHint"); hint->setWordWrap(true); v->addWidget(hint);
    finishLayout(false);
    m_run->setDefault(true); fitRunButton();   // Enter in any field runs the search (as in the web app)
    resize(1100, 820);
    connect(m_run, &QPushButton::clicked, this, &CombinerDialog::run);
    connect(m_target, &QLineEdit::textEdited, m_mark, &FromMarker::clear);
    connect(m_target, &QLineEdit::textChanged, this, &CombinerDialog::showParsed);
    connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { showParsed(); showLegend(); });
    connect(m_sort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CombinerDialog::showResults);
    connect(m_tol, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CombinerDialog::showResults);
    connect(m_series, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CombinerDialog::showSeriesInfo);
    connect(m_custom, &QPlainTextEdit::textChanged, this, &CombinerDialog::showSeriesInfo);
    connect(m_t, &QTableWidget::currentCellChanged, this, [this] { showDetail(); });
    QJsonObject st = Project::instance()->state(window()); if (st.isEmpty()) st = QJsonDocument::fromJson(ke9esuSettings()->value("KE9ESU/state/" + window()).toByteArray()).object();
    loadState(st);
    showParsed(); showSeriesInfo(); showLegend(); showDetail();
}
CombinerDialog::~CombinerDialog() { m_abort = true; if (m_thread) { m_thread->wait(); } }

void CombinerDialog::relayoutForm()
{
    int px = fontPx(font()), w = body()->width() > 100 ? body()->width() : width(); w -= 24;
    int cols = qBound(1, w / (22 * px), 3);
    if (cols == m_cols) return;
    m_cols = cols;
    for (QWidget *c : m_cells) m_grid->removeWidget(c); m_grid->removeWidget(m_customCell);
    for (int i = 0; i < m_cells.size(); i++) m_grid->addWidget(m_cells[i], i / cols, i % cols);
    for (int c = 0; c < 3; c++) m_grid->setColumnStretch(c, c < cols ? 1 : 0);
    m_grid->addWidget(m_customCell, (m_cells.size() + cols - 1) / cols, 0, 1, cols);
}
void CombinerDialog::fitRunButton()
{
    QFont bf = m_run->font(); bf.setBold(true); QFontMetrics fm(bf);
    m_run->setMinimumWidth(fm.horizontalAdvance(m_run->text()) + fm.height() * 2 + 12);
}
void CombinerDialog::resizeEvent(QResizeEvent *e) { Ke9esuDialog::resizeEvent(e); int c = m_cols; relayoutForm(); if (c != m_cols || !m_sized) { m_sized = isVisible(); QTimer::singleShot(0, this, [this] { sizeDetail(); }); } }
void CombinerDialog::showEvent(QShowEvent *e) { Ke9esuDialog::showEvent(e); QTimer::singleShot(0, this, [this] { sizeDetail(); }); }
void CombinerDialog::showParsed()
{
    QString ty = m_type->currentData().toString(); double v = cc::parseValue(m_target->text());
    if (std::isnan(v)) { m_parsed->setText("<span style='color:" + sevColor("fail").name() + "'>" + esc(tr("Can't read that value")) + "</span>"); return; }
    QString msg = "= " + esc(cc::fmt(v, 6, cc::partType(ty[0]).unit));
    if (ty == "R" && v < 1 && m_target->text().contains('m')) msg += " <span style='color:" + sevColor("fail").name() + "'>" + esc(tr("(lower-case m = milli. For meg use M)")) + "</span>";
    m_parsed->setText(msg);
}
void CombinerDialog::showSeriesInfo()
{
    bool custom = m_series->currentData() == "custom";
    m_customCell->setVisible(custom);
    if (custom) {
        cc::ParsedList pl = cc::parseList(m_custom->toPlainText()); QString fail = sevColor("fail").name();
        QString t = pl.values.empty() ? QString() : esc(tr("Using %n value(s) instead of the E-series.", "", int(pl.values.size())));
        if (!pl.bad.isEmpty()) t += (t.isEmpty() ? QString() : " ") + "<span style='color:" + fail + "'>" + esc((pl.values.empty() ? tr("Couldn't read: ") : tr("Ignored: ")) + pl.bad.join(", ")) + "</span>";
        m_customInfo->setText(t);
        m_seriesHint->setText(tr("Not used while a custom parts list is filled in: the tolerance is whatever your parts are."));
    } else m_seriesHint->setText(esc(cc::seriesHint(m_series->currentData().toString())));
}
void CombinerDialog::showLegend()
{
    QString t = tr("In the formulas, <b>+</b> means wired in <b>series</b> and <b>||</b> means wired in <b>parallel</b>.");
    if (m_type->currentData() == "C") t += " " + tr("Remember: capacitors in <b>parallel</b> add up; in <b>series</b> they combine like resistors in parallel.");
    m_legend->setText(t);
}

cc::Params CombinerDialog::params(QString *err) const
{
    cc::Params p; p.type = m_type->currentData().toString()[0]; p.target = cc::parseValue(m_target->text());
    if (std::isnan(p.target)) *err = tr("Cannot read the value \"%1\"").arg(m_target->text());
    p.maxParts = m_max->value(); p.topo = m_topo->currentData().toString(); p.series = m_series->currentData().toString();
    if (p.series == "custom") { cc::ParsedList pl = cc::parseList(m_custom->toPlainText()); p.custom = pl.values; p.series = "E12";
        if (pl.values.empty() && err->isEmpty()) *err = pl.bad.isEmpty() ? tr("Enter your parts list") : tr("Couldn't read: %1").arg(pl.bad.join(", ")); }
    return p;
}

void CombinerDialog::run()
{
    if (m_thread) { m_abort = true; return; }
    QString err; cc::Params p = params(&err);
    if (!err.isEmpty()) { m_status->setText("<span style='color:" + sevColor("fail").name() + "'>" + esc(err) + "</span>"); return; }
    m_abort = false; m_run->setText(tr("Cancel")); fitRunButton(); m_prog->setValue(0); m_status->setText(tr("Searching…"));
    m_solType = p.type; m_solSeries = p.series; m_solCustom = (int)p.custom.size(); m_solMax = p.maxParts; m_solTarget = p.target;
    m_thread = new FnThread([this, p] {
        cc::Solution s = cc::solve(p, [this](const QString &ph, double fr) {
            QMetaObject::invokeMethod(this, [this, ph, fr] { m_prog->setValue(int(fr * 1000)); m_status->setText(ph); }, Qt::QueuedConnection);
            return !m_abort.load(); });
        QMetaObject::invokeMethod(this, [this, s] { m_sol = s; m_thread->wait(); m_thread->deleteLater(); m_thread = nullptr; m_run->setText(tr("Find combinations")); m_prog->setValue(1000); showResults(); }, Qt::QueuedConnection);
    });
    m_thread->start();
}
void CombinerDialog::runSync()
{
    QString err; cc::Params p = params(&err); if (!err.isEmpty()) return; m_solType = p.type; m_solSeries = p.series; m_solCustom = (int)p.custom.size(); m_solMax = p.maxParts; m_solTarget = p.target; m_sol = cc::solve(p); m_prog->setValue(1000); showResults();
}

void CombinerDialog::showResults()
{
    QString unit = cc::partType(m_solType).unit;
    if (m_solTarget == 0 && m_sol.error.isEmpty()) return;   // nothing searched yet
    if (!m_sol.error.isEmpty()) { m_status->setText(m_sol.error == "aborted" ? tr("Cancelled.") : "<span style='color:" + sevColor("fail").name() + "'>" + esc(m_sol.error) + "</span>"); m_sorted.clear(); m_t->setRowCount(0); showDetail(); return; }
    m_sorted = cc::sortResults(m_sol.results, m_sort->currentData().toString(), m_tol->value());
    m_t->setRowCount(0); m_t->setRowCount((int)m_sorted.size());
    double tol = m_tol->value();
    for (int r = 0; r < (int)m_sorted.size(); r++) {
        const cc::Result &x = m_sorted[r]; bool in = cc::withinTol(x.err, tol);
        QStringList c = {x.expr, cc::fmt(x.value, 6, unit), cc::pctText(x.err), (in ? S("✓ ") : S("✗ ")) + cc::tolBadge(x.err, tol), QString::number(x.count), cc::partsSummary(x.parts, unit), QString()};
        QString tip = caption(x) + "\n" + spreadText(x);
        for (int k = 0; k < NCols; k++) { QTableWidgetItem *it = new QTableWidgetItem(c[k]); it->setToolTip(tip);
            if (k == CValue || k == CErr || k == CCount) it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter); else it->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            if (k == CTol) { it->setForeground(sevColor(in ? "ok" : "warn")); QFont bf = it->font(); bf.setBold(true); it->setFont(bf); }
            else if (in) it->setForeground(sevColor("ok"));
            if (k == CDiagram) it->setData(Qt::UserRole, r);
            m_t->setItem(r, k, it); }
    }
    QStringList ts; for (int n : m_sol.tableSizes) ts << QString::number(n);
    QString src = m_solCustom ? tr("%n custom value(s)", "", m_solCustom) : m_solSeries + " (" + cc::seriesTolerance(m_solSeries) + " parts)";
    m_status->setText(esc(tr("Target %1 · %2 · up to %3 parts · %4 results · %5 values in range · tables %6 · searched in %7 s")
                          .arg(cc::fmt(m_solTarget, 6, unit), src).arg(m_solMax).arg(m_sorted.size()).arg(m_sol.leaves).arg(ts.join("/")).arg(toFixed(m_sol.ms / 1000.0, 2))));
    m_t->resizeRowsToContents();
    if (!m_sorted.empty()) m_t->selectRow(0);
    showDetail();
    sizeDetail();
    Project::instance()->setState(window(), saveState());
    ke9esuSettings()->setValue("KE9ESU/state/" + window(), QJsonDocument(saveState()).toJson(QJsonDocument::Compact));
}
QString CombinerDialog::caption(const cc::Result &x) const
{
    return S("A–B = ") + cc::fmt(x.value, 6, cc::partType(m_solType).unit) + "   " + S("(error ") + cc::pctText(x.err) + ", " + cc::tolBadge(x.err, m_tol->value()) + ")";
}
QString CombinerDialog::spreadText(const cc::Result &x) const
{
    double t = m_solCustom ? NaN : cc::seriesTolFrac(m_solSeries);
    if (std::isnan(t)) return tr("Part tolerance: depends on your parts (worst case the network is off by the same % as its parts).");
    cc::Spread sp = cc::partSpread(x.tree, m_solType, t); QString unit = cc::partType(m_solType).unit;
    QString s = tr("With %1 %2 parts: worst case ±%3 % (%4 … %5)").arg(cc::seriesTolerance(m_solSeries).section(',', 0, 0), m_solSeries, toFixed(sp.worst * 100, 2),
                                                                   cc::fmt(x.value * (1 - sp.worst), 4, unit), cc::fmt(x.value * (1 + sp.worst), 4, unit));
    if (x.count > 1) s += tr(", typically ±%1 % (independent part errors)").arg(toFixed(sp.typical * 100, 2));
    return s;
}
void CombinerDialog::showDetail()
{
    NetView *nv = static_cast<NetView *>(m_detail);
    int r = m_t->currentRow();
    if (r < 0 || r >= (int)m_sorted.size()) { nv->clear(); m_detailHead->setText(esc(tr("Select a combination to see its wiring diagram."))); return; }
    const cc::Result &x = m_sorted[r]; QString unit = cc::partType(m_solType).unit;
    QString d; int n = 0; QStringList des; for (double v : x.parts) des << net::prefix(m_solType) + QString::number(++n) + " " + cc::fmt(v, 4, unit);
    m_detailHead->setText("<b>#" + QString::number(r + 1) + "  " + esc(x.expr) + "</b> &nbsp; · &nbsp;" + esc(des.join(", ")) + "<br><span style='color:" + theme().muted.name() + "'>" + esc(spreadText(x)) + "</span>");
    nv->set(x.tree, m_solType, caption(x));
}
void CombinerDialog::selectResult(int row) { if (row >= 0 && row < m_t->rowCount()) { m_t->selectRow(row); m_t->scrollToItem(m_t->item(row, 0)); showDetail(); sizeDetail(); } }
/* inputs get what they need (at most 45 %), the selected diagram the height it needs at web-app scale (at most half of
   the rest), the results table the remainder */
void CombinerDialog::sizeDetail()
{
    QList<int> sz = m_split->sizes(); int total = 0; for (int x : sz) total += x; if (total <= 0 || sz.size() < 3) return;
    int in = qMin(m_inputs->sizeHint().height() + 2, int(total * 0.45));
    int rest = total - in, want = m_detail->sizeHint().height() + m_detailHead->sizeHint().height() + 12;
    int det = qBound(100, want, qMax(100, rest / 2));
    m_split->setSizes({in, qMax(80, rest - det), det});
}
void CombinerDialog::setInputs(QChar type, const QString &target, const QString &topo, int maxParts)
{
    m_type->setCurrentIndex(qMax(0, m_type->findData(QString(type)))); m_target->setText(target); m_mark->clear();
    m_topo->setCurrentIndex(qMax(0, m_topo->findData(topo))); m_max->setValue(maxParts);
}

void CombinerDialog::pullKey(const QString &key)
{
    Project *p = Project::instance();
    if (key == Key::CombReq) {
        QVariantMap m = p->get(key).toMap(); QString t = m.value("type").toString(); double v = m.value("value").toDouble();
        m_type->setCurrentIndex(qMax(0, m_type->findData(t))); m_target->setText(cc::fmt(v, 6, QString()).remove(' ').replace(S("µ"), "u"));
        m_mark->setFrom(p->source(key) + (m.value("label").toString().isEmpty() ? QString() : " (" + m.value("label").toString() + ")"));
        run();
    } else if (key == Key::C || key == Key::L) {
        bool c = key == Key::C; double v = p->num(key) * (c ? 1e-12 : 1e-6);
        m_type->setCurrentIndex(m_type->findData(c ? "C" : "L")); m_target->setText(cc::fmt(v, 6, QString()).remove(' ').replace(S("µ"), "u")); m_mark->setFrom(p->source(key)); run();
    }
}
void CombinerDialog::fillPull(QMenu *m)
{
    Project *p = Project::instance();
    addPull(m, Key::CombReq, tr("Requested value") + ": " + p->get(Key::CombReq).toMap().value("label").toString());
    addPull(m, Key::C, tr("Capacitance") + ": " + f(p->num(Key::C), 1) + " pF");
    addPull(m, Key::L, tr("Inductance") + ": " + f(p->num(Key::L), 3) + S(" µH"));
}
void CombinerDialog::fillUseIn(QMenu *m)
{
    int r = m_t->currentRow(); if (r < 0 || r >= (int)m_sorted.size()) { m->addAction(tr("(select a result first)"))->setEnabled(false); return; }
    const cc::Result x = m_sorted[r]; Project *p = Project::instance(); QString w = window();
    if (m_solType == 'C') {
        m->addAction(tr("Achieved %1 → LC trap capacitor").arg(cc::fmt(x.value, 5, "F")), this, [p, w, x] { p->set(Key::C, x.value * 1e12, w); p->requestWindow(Win::Lc, Key::C); });
        m->addAction(tr("Achieved %1 → Coil64 LC circuit").arg(cc::fmt(x.value, 5, "F")), this, [p, w, x] { p->set(Key::C, x.value * 1e12, w); p->requestCoil64("lc", {{"C", x.value * 1e12}}); });
    } else if (m_solType == 'L') {
        m->addAction(tr("Achieved %1 → LC trap inductor").arg(cc::fmt(x.value, 5, "H")), this, [p, w, x] { p->set(Key::L, x.value * 1e6, w); p->requestWindow(Win::Lc, Key::L); });
        m->addAction(tr("Achieved %1 → Coil64 inductance").arg(cc::fmt(x.value, 5, "H")), this, [p, w, x] { p->set(Key::L, x.value * 1e6, w); p->requestCoil64("inductance", {{"L", x.value * 1e6}, {"kind", "keep"}}); });
    } else m->addAction(tr("(resistor results are not used by other windows)"))->setEnabled(false);
}
QJsonObject CombinerDialog::saveState() const
{
    QJsonObject o; o.insert("type", m_type->currentData().toString()); o.insert("target", m_target->text()); o.insert("series", m_series->currentData().toString());
    o.insert("custom", m_custom->toPlainText()); o.insert("max", m_max->value()); o.insert("topo", m_topo->currentData().toString()); o.insert("sort", m_sort->currentData().toString()); o.insert("tol", m_tol->value());
    return o;
}
void CombinerDialog::loadState(const QJsonObject &o)
{
    if (o.isEmpty()) return;
    m_type->setCurrentIndex(qMax(0, m_type->findData(o.value("type").toString("R")))); m_target->setText(o.value("target").toString("1234"));
    m_series->setCurrentIndex(qMax(0, m_series->findData(o.value("series").toString("E12")))); m_custom->setPlainText(o.value("custom").toString());
    m_max->setValue(o.value("max").toInt(3)); m_topo->setCurrentIndex(qMax(0, m_topo->findData(o.value("topo").toString("mixed"))));
    m_sort->setCurrentIndex(qMax(0, m_sort->findData(o.value("sort").toString("simple")))); m_tol->setValue(o.value("tol").toDouble(1));
}
static const int SHEET_DIAGRAMS = 10;
QMap<QString, QImage> CombinerDialog::sheetImages() const
{
    QMap<QString, QImage> m;
    for (int r = 0; r < (int)m_sorted.size() && r < SHEET_DIAGRAMS; r++) m.insert(QString("net%1").arg(r + 1, 2, 10, QChar('0')), net::render(m_sorted[r].tree, m_solType, 2));
    return m;
}
QString CombinerDialog::sheetHtml() const
{
    QString unit = cc::partType(m_solType).unit; double tol = m_tol->value();
    QString src = m_solCustom ? tr("%n custom value(s)", "", m_solCustom) : m_solSeries + " (" + cc::seriesTolerance(m_solSeries) + " parts)";
    QString h = "<html><head>" + sheetCss() + "</head><body><h1>" + esc(tr("Standard-part combinations for %1").arg(cc::fmt(m_solTarget, 6, unit))) + "</h1>"
              + "<p>" + esc(tr("%1 · up to %2 parts · %3 · acceptable error ±%4 % · sorted: %5").arg(src).arg(m_solMax).arg(m_topo->currentText()).arg(jsNum(tol)).arg(m_sort->currentText())) + "</p>"
              + "<table class='t' cellspacing='0'><tr><th>#</th><th>Network</th><th>Value</th><th>Error</th><th>Tolerance</th><th>Parts</th><th>Part values</th></tr>";
    for (size_t r = 0; r < m_sorted.size() && r < 40; r++) { const cc::Result &x = m_sorted[r];
        h += "<tr><td class='n'>" + QString::number(r + 1) + "</td><td>" + esc(x.expr) + "</td><td class='n'>" + esc(cc::fmt(x.value, 6, unit)) + "</td><td class='n'>" + esc(cc::pctText(x.err))
           + "</td><td>" + esc(cc::tolBadge(x.err, tol)) + "</td><td class='n'>" + QString::number(x.count) + "</td><td>" + esc(cc::partsSummary(x.parts, unit)) + "</td></tr>"; }
    h += "</table><h2>" + esc(tr("Wiring diagrams")) + "</h2>";
    for (int r = 0; r < (int)m_sorted.size() && r < SHEET_DIAGRAMS; r++) { const cc::Result &x = m_sorted[r]; QSizeF sz = net::size(x.tree, m_solType);
        h += "<p><b>#" + QString::number(r + 1) + "  " + esc(x.expr) + "</b> — " + esc(caption(x)) + "<br>" + esc(spreadText(x)) + "</p>"
           + QString("<p><img src='ke9esu://net%1' width='%2'></p>").arg(r + 1, 2, 10, QChar('0')).arg(int(std::min(640.0, sz.width() * 1.1))); }
    h += "<p>" + esc(tr("+ = wired in series, || = wired in parallel. Inductors assume no mutual coupling. Real parts have tolerance too.")) + "</p>";
    return h + "</body></html>";
}

} // namespace ke9esu
