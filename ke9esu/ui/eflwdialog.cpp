/* eflwdialog.cpp - see eflwdialog.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "eflwdialog.h"
#include "project.h"
#include "trap_core.h"
#include "balun_core.h"
#include "ke9esu_util.h"
#include <QBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollArea>
#include <QMenu>
#include <QJsonDocument>
#include <cmath>

namespace ke9esu {

static QTableWidget *mkTable(const QStringList &h)
{
    QTableWidget *t = new QTableWidget(0, h.size()); t->setHorizontalHeaderLabels(h); t->verticalHeader()->hide(); t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows); t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); t->horizontalHeader()->setStretchLastSection(true);
    t->setMinimumHeight(200); t->setWordWrap(true); return t;
}

EflwDialog::EflwDialog(QWidget *parent) : Ke9esuDialog(Win::Eflw, parent)
{
    initData();
    QWidget *in = new QWidget; QFormLayout *fl = new QFormLayout(in);
    m_unit = new QComboBox; m_unit->addItem("ft", "ft"); m_unit->addItem("m", "m");
    const Project::Units &u = Project::instance()->units(); m_unit->setCurrentIndex(u.len == "in" || u.len == "mil" ? 0 : 1);
    m_len = new QDoubleSpinBox; m_len->setRange(3, 3000); m_len->setDecimals(2); m_len->setKeyboardTracking(false);
    QWidget *lw = new QWidget; QHBoxLayout *lh = new QHBoxLayout(lw); lh->setContentsMargins(0, 0, 0, 0); lh->addWidget(m_len); lh->addWidget(m_unit); lh->addStretch();
    m_k = new QDoubleSpinBox; m_k->setRange(0.80, 1.0); m_k->setDecimals(3); m_k->setSingleStep(0.005); m_k->setValue(0.95); m_k->setKeyboardTracking(false);
    m_k->setToolTip(tr("Wire velocity / end-effect factor (0.95 bare wire; ~0.97 thin insulated wire in the clear)."));
    m_tol = new QDoubleSpinBox; m_tol->setRange(0.01, 0.2); m_tol->setDecimals(2); m_tol->setSingleStep(0.01); m_tol->setValue(0.05); m_tol->setKeyboardTracking(false);
    m_min = new QDoubleSpinBox; m_min->setRange(10, 1000); m_min->setValue(30); m_min->setSuffix(" ft"); m_max = new QDoubleSpinBox; m_max->setRange(30, 2000); m_max->setValue(300); m_max->setSuffix(" ft");
    m_6m = new QCheckBox(tr("include 6 m")); m_60m = new QCheckBox(tr("include 60 m")); m_60m->setChecked(true);
    fl->addRow(tr("Wire length"), lw); fl->addRow(tr("Velocity factor k"), m_k); fl->addRow(tr("Avoid-zone tolerance (± λ/2 fraction)"), m_tol);
    QWidget *sw = new QWidget; QHBoxLayout *sh = new QHBoxLayout(sw); sh->setContentsMargins(0, 0, 0, 0); sh->addWidget(m_min); sh->addWidget(new QLabel(S("–"))); sh->addWidget(m_max); sh->addWidget(m_6m); sh->addWidget(m_60m); sh->addStretch();
    fl->addRow(tr("Good-length search"), sw);
    m_proj = new ProjectBox(Win::Eflw, ProjectBox::Power | ProjectBox::ZEnd); fl->addRow(m_proj);

    QWidget *res = new QWidget; QVBoxLayout *rv = new QVBoxLayout(res);
    m_summary = new QLabel; m_summary->setWordWrap(true);
    QGroupBox *rg = new QGroupBox(tr("Recommended transformer")); QVBoxLayout *rgl = new QVBoxLayout(rg);
    m_reco = new QLabel; m_reco->setWordWrap(true); m_recoBtn = new QPushButton; rgl->addWidget(m_reco); QHBoxLayout *rbh = new QHBoxLayout; rbh->addWidget(m_recoBtn); rbh->addStretch(); rgl->addLayout(rbh);
    m_table = mkTable({"Band", S("Length in λ"), "Impedance tendency", "9:1 + tuner", "49:1"});
    m_traps = mkTable({"Band", "Trap f (MHz)", "From feed point", "Suggestion", "", ""});
    m_zonesLbl = new QLabel; m_zonesLbl->setWordWrap(true); m_good = new QLabel; m_good->setWordWrap(true);
    QWidget *zw = new QWidget; QVBoxLayout *zv = new QVBoxLayout(zw); zv->setContentsMargins(0, 0, 0, 0); zv->addWidget(m_good); zv->addWidget(m_zonesLbl);
    m_rTable = new Rollup("eflw_table", tr("Band-by-band"), m_table);
    m_rTraps = new Rollup("eflw_traps", tr("Trap suggestions (problem bands)"), m_traps);
    m_rZones = new Rollup("eflw_zones", tr("Avoid zones and good lengths"), zw);
    QLabel *rem = new QLabel(S("<i>Reminders: run a counterpoise (≈0.05 λ at the lowest band) or a feedline choke at the transformer; keep the wire clear of metal; "
                               "classification uses ±8% around n·λ/2 (high Z) and odd λ/4 (low Z) at the band edges.</i>")); rem->setWordWrap(true);
    rv->addWidget(m_summary); rv->addWidget(rg); rv->addWidget(m_rTable); rv->addWidget(m_rTraps); rv->addWidget(m_rZones); rv->addWidget(rem); rv->addStretch();
    QScrollArea *sa = new QScrollArea; sa->setWidget(res); sa->setWidgetResizable(true); sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QVBoxLayout *bl = new QVBoxLayout(body()); bl->setContentsMargins(0, 0, 0, 0); bl->addWidget(in); bl->addWidget(sa, 1);
    finishLayout(false);
    resize(1100, 860);

    for (QDoubleSpinBox *s : {m_len, m_k, m_tol, m_min, m_max}) connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EflwDialog::recalc);
    connect(m_6m, &QCheckBox::toggled, this, &EflwDialog::recalc); connect(m_60m, &QCheckBox::toggled, this, &EflwDialog::recalc);
    connect(m_unit, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        double ft = Project::instance()->num(Key::EflwLen, 124); m_internal = true; m_len->setValue(m_unit->currentData() == "ft" ? ft : ft * 0.3048); m_internal = false; recalc(); });
    connect(m_proj, &ProjectBox::edited, this, &EflwDialog::recalc);
    connect(m_recoBtn, &QPushButton::clicked, this, [this] { Project *p = Project::instance(); p->set(Key::Recommend, m_adv.presetId, window()); p->requestWindow(Win::Balun, Key::Recommend); });
    connect(Project::instance(), &Project::loaded, this, [this] { loadState(Project::instance()->state(window())); });
    QJsonObject st = Project::instance()->state(window());
    if (st.isEmpty()) st = QJsonDocument::fromJson(ke9esuSettings()->value("KE9ESU/state/" + window()).toByteArray()).object();
    m_internal = true; m_len->setValue(m_unit->currentData() == "ft" ? 124 : 124 * 0.3048); m_internal = false;
    if (!st.isEmpty()) loadState(st); else recalc();
}

double EflwDialog::lengthFt() const { return m_unit->currentData() == "ft" ? m_len->value() : m_len->value() / 0.3048; }

void EflwDialog::recalc()
{
    if (m_internal) return;
    double len = toFixedNum(lengthFt(), 2), k = m_k->value();
    Project::instance()->set(Key::EflwLen, len, window());
    EflwOpts o; o.k = k; m_rows = eflwAnalysis(len, o);
    auto cls = [](const QString &s) { return s.contains("good") ? QString("ok") : (s.contains("fair") ? QString("warn") : QString("fail")); };
    m_table->setRowCount((int)m_rows.size()); int cnt[3] = {0, 0, 0};
    for (int r = 0; r < (int)m_rows.size(); r++) {
        const EflwRow &e = m_rows[r];
        QStringList v = {e.band, f(e.lambdaLo[0], 2) + S("–") + f(e.lambdaLo[1], 2), e.verdict, e.unun9, e.unun49};
        for (int c = 0; c < 5; c++) { QTableWidgetItem *it = new QTableWidgetItem(v[c]); if (c >= 3) it->setForeground(sevColor(cls(v[c]))); if (c == 0) { QFont fb = it->font(); fb.setBold(true); it->setFont(fb); } m_table->setItem(r, c, it); }
        QString c9 = cls(e.unun9); cnt[c9 == "ok" ? 0 : c9 == "warn" ? 1 : 2]++;
    }
    m_rTable->setSummary(QString("%1 bands · 9:1 + tuner: <span style='color:%2'>good %3</span> · <span style='color:%4'>fair %5</span> · <span style='color:%6'>poor %7</span>")
                             .arg(m_rows.size()).arg(sevColor("ok").name()).arg(cnt[0]).arg(sevColor("warn").name()).arg(cnt[1]).arg(sevColor("fail").name()).arg(cnt[2]));
    m_adv = recommendTransformer(len, m_rows);
    QStringList hi = m_adv.highBands, lo = m_adv.lowBands, med = m_adv.moderateBands;
    m_summary->setText(QString("<b>%1 ft (%2 m):</b> high-Z (near a half-wave multiple) on <b>%3</b>; low-Z (near an odd quarter-wave) on <b>%4</b>; moderate (9:1 sweet spot) on <b>%5</b>.")
                           .arg(jsNum(len), f(len * 0.3048, 2), hi.isEmpty() ? "none" : hi.join(", "), lo.isEmpty() ? "none" : lo.join(", "), med.isEmpty() ? "none" : med.join(", ")));
    Project::instance()->set(Key::ProblemBands, hi, window());
    m_reco->setText(esc(m_adv.reason));
    const Preset *pr = presetById(m_adv.presetId);
    m_recoBtn->setText(tr("Open the recommended transformer in the Balun/Unun designer")); m_recoBtn->setToolTip(pr ? pr->label : m_adv.presetId);
    // trap suggestions
    m_sugg = trapSuggestions(len, m_rows, k);
    m_traps->setRowCount((int)m_sugg.size());
    for (int r = 0; r < (int)m_sugg.size(); r++) {
        const TrapSuggestion &t = m_sugg[r];
        m_traps->setItem(r, 0, new QTableWidgetItem(t.band)); m_traps->setItem(r, 1, new QTableWidgetItem(f(t.fMHz, 3)));
        m_traps->setItem(r, 2, new QTableWidgetItem(f(t.distanceFt, 1) + " ft (" + f(t.distanceFt * 0.3048, 2) + " m)")); m_traps->setItem(r, 3, new QTableWidgetItem(t.text));
        QPushButton *bc = new QPushButton(tr("→ Coax trap")), *bl = new QPushButton(tr("→ LC trap"));
        connect(bc, &QPushButton::clicked, this, [this, r] { sendTrap(r, Win::Coax); }); connect(bl, &QPushButton::clicked, this, [this, r] { sendTrap(r, Win::Lc); });
        m_traps->setCellWidget(r, 4, bc); m_traps->setCellWidget(r, 5, bl);
    }
    m_traps->resizeRowsToContents();
    m_rTraps->setSummary(m_sugg.empty() ? tr("no high-Z band needs a trap") : QString("%1 suggestion(s): ").arg(m_sugg.size()) + [&] { QStringList b; for (auto &t : m_sugg) b << t.band + " @ " + f(t.distanceFt, 1) + " ft"; return b.join(", "); }());
    if (!m_sugg.empty()) { const TrapSuggestion &t = m_sugg[0]; Project::instance()->set(Key::TrapReq, QVariantMap{{"band", t.band}, {"fMHz", t.fMHz}, {"distanceFt", t.distanceFt}, {"text", t.text}}, window()); }
    // zones / good lengths
    std::vector<Band> bs; for (const Band &b : bands()) if ((m_6m->isChecked() || b.id != "6m") && (m_60m->isChecked() || b.id != "60m")) bs.push_back(b);
    EflwOpts zo; zo.k = k; zo.tol = m_tol->value(); zo.bands = bs;
    double mn = m_min->value(), mx = std::max(m_max->value(), mn + 20);
    auto zones = avoidZones(mx, zo); auto good = goodLengths(mn, mx, zo);
    QStringList inZone; for (auto &z : zones) if (len >= z.loFt && len <= z.hiFt) inZone << z.band + " (" + QString::number(z.n) + S("·λ/2)");
    bool inGood = false; std::pair<double, double> below{NaN, NaN}, above{NaN, NaN};
    for (auto &g : good) { if (len >= g.first && len <= g.second) inGood = true; if (g.first < len) below = g; if (g.second > len && std::isnan(above.first)) above = g; }
    QString nearest = inGood ? jsNum(len) + " ft is inside a good gap for the selected bands."
        : jsNum(len) + " ft falls in the avoid zone for " + inZone.join(", ") + ". Nearest good gaps: " +
          (!std::isnan(below.first) ? "shorten to about " + f(std::min(below.second - 1, (below.first + below.second) / 2 + 1), 0) + " ft (" + f(below.first, 1) + S("–") + f(below.second, 1) + ")" : QString("none shorter")) +
          (!std::isnan(above.first) ? "; or lengthen to about " + f((above.first + above.second) / 2, 0) + " ft (" + f(above.first, 1) + S("–") + f(above.second, 1) + ")" : "; no longer gap up to " + jsNum(mx) + " ft with these bands") + ".";
    QStringList gl; for (auto &g : good) gl << f(g.first, 1) + S("–") + f(g.second, 1);
    QStringList bn; for (auto &b : bs) bn << b.id;
    m_good->setText("<p>" + esc(nearest) + "</p><p><b>Good-length gaps " + jsNum(mn) + S("–") + jsNum(mx) + " ft</b> (" + bn.join(" ") + S(", ±") + rnd(m_tol->value() * 100) + "%): " + (good.empty() ? QString("none — relax the window or drop a band") : gl.join(", ") + " ft") + "</p>");
    QStringList zl; for (auto &z : zones) zl << QString("%1 %2·λ/2: %3–%4").arg(z.band).arg(z.n).arg(f(z.loFt, 1), f(z.hiFt, 1));
    m_zonesLbl->setText("<span style='color:palette(dark)'>" + esc(S("Avoid zones (ft): ") + zl.join("; ")) + "</span>");
    m_rZones->setSummary(inGood ? QString("<span style='color:%1'>%2 ft is in a good gap</span>").arg(sevColor("ok").name(), jsNum(len)) : QString("<span style='color:%1'>%2 ft is in an avoid zone (%3)</span>").arg(sevColor("warn").name(), jsNum(len), inZone.join(", ")));
    setWindowTitle(S("Long-wire analyzer (%1 ft) - KE9ESU").arg(jsNum(len)));
    Project::instance()->setState(window(), saveState());
    ke9esuSettings()->setValue("KE9ESU/state/" + window(), QJsonDocument(saveState()).toJson(QJsonDocument::Compact));
}

void EflwDialog::sendTrap(int row, const QString &target)
{
    const TrapSuggestion &t = m_sugg[row]; Project *p = Project::instance();
    p->set(Key::F, t.fMHz, window());
    p->set(Key::TrapReq, QVariantMap{{"band", t.band}, {"fMHz", t.fMHz}, {"distanceFt", t.distanceFt}, {"text", t.text}}, window());
    p->requestWindow(target, Key::TrapReq);
}

void EflwDialog::fillUseIn(QMenu *m)
{
    Project *p = Project::instance(); QString w = window();
    m->addAction(tr("Recommended transformer (%1) → Balun/Unun designer").arg(m_adv.presetId), this, [this, p, w] { p->set(Key::Recommend, m_adv.presetId, w); p->requestWindow(Win::Balun, Key::Recommend); });
    if (!m_adv.highBands.isEmpty()) m->addAction(tr("Problem bands (%1) → Balun/Unun designer frequency range").arg(m_adv.highBands.join(", ")), this, [this, p, w] { p->set(Key::ProblemBands, m_adv.highBands, w); p->requestWindow(Win::Balun, Key::ProblemBands); });
    if (!m_sugg.empty()) m->addSeparator();
    for (int r = 0; r < (int)m_sugg.size(); r++) {
        m->addAction(tr("%1 trap %2 MHz → Coax trap").arg(m_sugg[r].band, f(m_sugg[r].fMHz, 3)), this, [this, r] { sendTrap(r, Win::Coax); });
        m->addAction(tr("%1 trap %2 MHz → LC trap").arg(m_sugg[r].band, f(m_sugg[r].fMHz, 3)), this, [this, r] { sendTrap(r, Win::Lc); });
    }
}

QJsonObject EflwDialog::saveState() const
{
    QJsonObject o; o.insert("lengthFt", lengthFt()); o.insert("unit", m_unit->currentData().toString()); o.insert("k", m_k->value()); o.insert("tol", m_tol->value());
    o.insert("min", m_min->value()); o.insert("max", m_max->value()); o.insert("inc6", m_6m->isChecked()); o.insert("inc60", m_60m->isChecked());
    return o;
}
void EflwDialog::loadState(const QJsonObject &o)
{
    if (o.isEmpty()) return;
    m_internal = true;
    m_unit->setCurrentIndex(qMax(0, m_unit->findData(o.value("unit").toString("ft"))));
    double ft = o.value("lengthFt").toDouble(124); m_len->setValue(m_unit->currentData() == "ft" ? ft : ft * 0.3048);
    m_k->setValue(o.value("k").toDouble(0.95)); m_tol->setValue(o.value("tol").toDouble(0.05)); m_min->setValue(o.value("min").toDouble(30)); m_max->setValue(o.value("max").toDouble(300));
    m_6m->setChecked(o.value("inc6").toBool(false)); m_60m->setChecked(o.value("inc60").toBool(true));
    m_internal = false; recalc();
}

QString EflwDialog::sheetTitle() const { return QString("KE9ESU long wire %1 ft").arg(jsNum(toFixedNum(lengthFt(), 2))); }
QString EflwDialog::sheetHtml() const
{
    QString h = "<html><head>" + sheetCss() + "</head><body><h1>" + esc(sheetTitle()) + "</h1>";
    h += "<p>" + m_summary->text() + "</p><h2>Recommended transformer</h2><p>" + esc(m_adv.reason) + "</p>";
    h += "<h2>Band-by-band</h2><table class='t' cellspacing='0'><tr><th>Band</th><th>Length in λ</th><th>Impedance tendency</th><th>9:1 + tuner</th><th>49:1</th></tr>";
    for (const EflwRow &e : m_rows) h += "<tr><td><b>" + e.band + "</b></td><td class='n'>" + f(e.lambdaLo[0], 2) + S("–") + f(e.lambdaLo[1], 2) + "</td><td>" + esc(e.verdict) + "</td><td>" + esc(e.unun9) + "</td><td>" + esc(e.unun49) + "</td></tr>";
    h += "</table><h2>Trap suggestions</h2>";
    if (m_sugg.empty()) h += "<p>None needed.</p>"; else { h += "<ul>"; for (auto &t : m_sugg) h += "<li>" + esc(t.text) + "</li>"; h += "</ul>"; }
    h += "<h2>Good lengths</h2>" + m_good->text() + "</body></html>";
    return h;
}

} // namespace ke9esu
