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
}

CombinerDialog::CombinerDialog(QWidget *parent) : Ke9esuDialog(Win::Combiner, parent)
{
    QVBoxLayout *v = new QVBoxLayout(body()); v->setContentsMargins(0, 0, 0, 0);
    QFormLayout *fl = new QFormLayout;
    m_type = new QComboBox; m_type->addItem(tr("Resistor (Ω)"), "R"); m_type->addItem(tr("Capacitor (F)"), "C"); m_type->addItem(tr("Inductor (H)"), "L");
    m_target = new QLineEdit("1234"); m_target->setToolTip(tr("Value with SI prefix: 4k7, 2u2, 100nF, 1meg, 4R7, 33p, 4.87uH …")); m_mark = new FromMarker;
    QWidget *tw = new QWidget; QHBoxLayout *th = new QHBoxLayout(tw); th->setContentsMargins(0, 0, 0, 0); th->addWidget(m_target, 1); th->addWidget(m_mark);
    m_series = new QComboBox; for (const char *s : {"E6", "E12", "E24", "E48", "E96", "E192"}) m_series->addItem(s, s); m_series->addItem(tr("My parts (list below)"), "custom"); m_series->setCurrentIndex(1);
    m_custom = new QPlainTextEdit; m_custom->setPlaceholderText(tr("junk-box values separated by spaces, commas or new lines: 100 220 470 1k 2.2k")); m_custom->setMaximumHeight(60);
    m_max = new QSpinBox; m_max->setRange(1, 6); m_max->setValue(3);
    m_topo = new QComboBox; m_topo->addItem(tr("Mixed series/parallel"), "mixed"); m_topo->addItem(tr("Series only"), "series"); m_topo->addItem(tr("Parallel only"), "parallel");
    m_sort = new QComboBox; m_sort->addItem(tr("Fewest parts within tolerance"), "simple"); m_sort->addItem(tr("Most accurate"), "accuracy");
    m_tol = new QDoubleSpinBox; m_tol->setRange(0.001, 20); m_tol->setDecimals(3); m_tol->setValue(1); m_tol->setSuffix(" %");
    fl->addRow(tr("Part type"), m_type); fl->addRow(tr("Needed value"), tw); fl->addRow(tr("Series"), m_series); fl->addRow(m_custom);
    fl->addRow(tr("Up to"), m_max); fl->addRow(tr("Topology"), m_topo); fl->addRow(tr("Sort"), m_sort); fl->addRow(tr("Tolerance"), m_tol);
    v->addLayout(fl);
    QHBoxLayout *rh = new QHBoxLayout; m_run = new QPushButton(tr("Find combinations")); m_prog = new QProgressBar; m_prog->setRange(0, 1000); m_status = new QLabel;
    rh->addWidget(m_run); rh->addWidget(m_prog, 1); v->addLayout(rh); v->addWidget(m_status);
    m_t = new QTableWidget(0, 5); m_t->setHorizontalHeaderLabels({tr("Network ( + = series, || = parallel )"), tr("Value"), tr("Error"), tr("Parts"), tr("Part values")});
    m_t->verticalHeader()->hide(); m_t->setEditTriggers(QAbstractItemView::NoEditTriggers); m_t->setSelectionBehavior(QAbstractItemView::SelectRows); m_t->setSelectionMode(QAbstractItemView::SingleSelection);
    m_t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); m_t->horizontalHeader()->setStretchLastSection(true);
    v->addWidget(m_t, 1);
    QLabel *hint = new QLabel(S("<span style='color:gray'>“+” = wired in series, “||” = wired in parallel (for capacitors series values add as 1/C). "
                                "Leaf ranges: R 1 Ω–10 MΩ, C 1 pF–10 mF, L 1 nH–1 H. Meet-in-the-middle search over all 1…6-part networks.</span>")); hint->setWordWrap(true); v->addWidget(hint);
    finishLayout(false);
    resize(980, 720);
    connect(m_run, &QPushButton::clicked, this, &CombinerDialog::run);
    connect(m_target, &QLineEdit::textEdited, m_mark, &FromMarker::clear);
    connect(m_target, &QLineEdit::returnPressed, this, &CombinerDialog::run);
    connect(m_sort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CombinerDialog::showResults);
    connect(m_tol, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CombinerDialog::showResults);
    connect(m_series, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { m_custom->setVisible(m_series->currentData() == "custom"); });
    m_custom->setVisible(false);
    QJsonObject st = Project::instance()->state(window()); if (st.isEmpty()) st = QJsonDocument::fromJson(ke9esuSettings()->value("KE9ESU/state/" + window()).toByteArray()).object();
    loadState(st);
}
CombinerDialog::~CombinerDialog() { m_abort = true; if (m_thread) { m_thread->wait(); } }

cc::Params CombinerDialog::params(QString *err) const
{
    cc::Params p; p.type = m_type->currentData().toString()[0]; p.target = cc::parseValue(m_target->text());
    if (std::isnan(p.target)) *err = tr("Cannot read the value \"%1\"").arg(m_target->text());
    p.maxParts = m_max->value(); p.topo = m_topo->currentData().toString(); p.series = m_series->currentData().toString();
    if (p.series == "custom") { cc::ParsedList pl = cc::parseList(m_custom->toPlainText()); p.custom = pl.values; p.series = "E12";
        if (!pl.bad.isEmpty()) *err = tr("Not understood: %1").arg(pl.bad.join(" ")); if (pl.values.empty() && err->isEmpty()) *err = tr("Enter your parts list"); }
    return p;
}

void CombinerDialog::run()
{
    if (m_thread) { m_abort = true; return; }
    QString err; cc::Params p = params(&err);
    if (!err.isEmpty()) { m_status->setText("<span style='color:#c62828'>" + esc(err) + "</span>"); return; }
    m_abort = false; m_run->setText(tr("Cancel")); m_prog->setValue(0); m_status->setText(tr("Searching…"));
    m_solType = p.type;
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
    QString err; cc::Params p = params(&err); if (!err.isEmpty()) return; m_solType = p.type; m_sol = cc::solve(p); m_prog->setValue(1000); showResults();
}

void CombinerDialog::showResults()
{
    QString unit = cc::partType(m_solType).unit;
    if (!m_sol.error.isEmpty()) { m_status->setText(m_sol.error == "aborted" ? tr("Cancelled.") : "<span style='color:#c62828'>" + esc(m_sol.error) + "</span>"); m_t->setRowCount(0); return; }
    m_sorted = cc::sortResults(m_sol.results, m_sort->currentData().toString(), m_tol->value());
    m_t->setRowCount((int)m_sorted.size());
    for (int r = 0; r < (int)m_sorted.size(); r++) {
        const cc::Result &x = m_sorted[r]; QStringList pv; for (double v : x.parts) pv << cc::fmt(v, 4, unit);
        double e = x.err * 100;
        QStringList c = {x.expr, cc::fmt(x.value, 6, unit), (e >= 0 ? "+" : "") + jsNum(toPrecisionNum(e, 3)) + " %", QString::number(x.count), pv.join(", ")};
        for (int k = 0; k < 5; k++) { QTableWidgetItem *it = new QTableWidgetItem(c[k]); if (k >= 1 && k <= 3) it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (std::fabs(x.err) <= m_tol->value() / 100 + 1e-12) it->setForeground(sevColor("ok")); m_t->setItem(r, k, it); }
    }
    QStringList ts; for (int n : m_sol.tableSizes) ts << QString::number(n);
    m_status->setText(QString("%1 results · %2 standard values in range · tables %3 · %4 ms").arg(m_sorted.size()).arg(m_sol.leaves).arg(ts.join("/")).arg(m_sol.ms));
    if (!m_sorted.empty()) m_t->selectRow(0);
    Project::instance()->setState(window(), saveState());
    ke9esuSettings()->setValue("KE9ESU/state/" + window(), QJsonDocument(saveState()).toJson(QJsonDocument::Compact));
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
QString CombinerDialog::sheetHtml() const
{
    QString unit = cc::partType(m_solType).unit;
    QString h = "<html><head>" + sheetCss() + "</head><body><h1>" + esc(tr("Standard-part combinations for %1").arg(m_target->text())) + "</h1><table class='t' cellspacing='0'><tr><th>Network</th><th>Value</th><th>Error %</th><th>Parts</th></tr>";
    for (size_t r = 0; r < m_sorted.size() && r < 40; r++) h += "<tr><td>" + esc(m_sorted[r].expr) + "</td><td class='n'>" + esc(cc::fmt(m_sorted[r].value, 6, unit)) + "</td><td class='n'>" + jsNum(toPrecisionNum(m_sorted[r].err * 100, 3)) + "</td><td class='n'>" + QString::number(m_sorted[r].count) + "</td></tr>";
    return h + "</table></body></html>";
}

} // namespace ke9esu
