/* trapdialogs.cpp - see trapdialogs.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "trapdialogs.h"
#include "coreui.h"
#include "project.h"
#include "serialize.h"
#include "ke9esu_util.h"
#include "drawings.h"
#include <QBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollArea>
#include <QSplitter>
#include <QMenu>
#include <QJsonDocument>
#include <QDate>
#include <cmath>

namespace ke9esu {

static QDoubleSpinBox *dsp(double lo, double hi, double v, int dec, const QString &suf = QString())
{ QDoubleSpinBox *s = new QDoubleSpinBox; s->setRange(lo, hi); s->setDecimals(dec); s->setValue(v); s->setSuffix(suf); s->setKeyboardTracking(false); return s; }
static QWidget *row(std::initializer_list<QWidget *> ws)
{ QWidget *w = new QWidget; QHBoxLayout *h = new QHBoxLayout(w); h->setContentsMargins(0, 0, 0, 0); for (QWidget *x : ws) h->addWidget(x); h->addStretch(); return w; }
static QTableWidget *tbl(const QStringList &h)
{
    QTableWidget *t = new QTableWidget(0, h.size()); t->setHorizontalHeaderLabels(h); t->verticalHeader()->hide(); t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); t->horizontalHeader()->setStretchLastSection(true); t->setMinimumHeight(160); return t;
}

/* ------------------------------------------------------------------ FreqPicker */
FreqPicker::FreqPicker(QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *h = new QHBoxLayout(this); h->setContentsMargins(0, 0, 0, 0);
    m_band = new QComboBox; m_band->addItem(tr("band…"), ""); for (const Band &b : bands()) m_band->addItem(b.id, b.id);
    m_place = new QComboBox; m_place->addItem(tr("centre"), "mid"); m_place->addItem(tr("1% below band"), "below");
    m_f = dsp(0.5, 60, 7.05, 3, " MHz"); m_mark = new FromMarker;
    h->addWidget(m_f); h->addWidget(m_band); h->addWidget(m_place); h->addWidget(m_mark); h->addStretch();
    auto apply = [this] { QString b = m_band->currentData().toString(); if (b.isEmpty()) return; double fv = bandFrequency(b, m_place->currentData().toString());
        if (!std::isnan(fv)) { m_internal = true; m_f->setValue(fv); m_internal = false; m_mark->clear(); emit changed(); } };
    connect(m_band, QOverload<int>::of(&QComboBox::activated), this, apply); connect(m_place, QOverload<int>::of(&QComboBox::activated), this, apply);
    connect(m_f, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this] { if (m_internal) return; m_band->setCurrentIndex(0); m_mark->clear(); emit changed(); });
}
double FreqPicker::mhz() const { return m_f->value(); }
void FreqPicker::setMhz(double v, const QString &from) { m_internal = true; m_f->setValue(v); m_band->setCurrentIndex(0); m_internal = false; if (from.isEmpty()) m_mark->clear(); else m_mark->setFrom(from); }

/* ------------------------------------------------------------------ TrapTables */
TrapTables::TrapTables(const QString &prefix, QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *v = new QVBoxLayout(this); v->setContentsMargins(0, 0, 0, 0);
    cards = new Cards(4); checks = new ChecksBar; drawing = new PaperView(720, 300);
    m_bands = tbl({"Band", "MHz", S("X (Ω)"), "Acts like", "Equivalent"}); m_parts = tbl({"Qty", "Item"});
    m_steps = new QLabel; m_steps->setWordWrap(true); m_steps->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_rb = new Rollup(prefix + "_bands", tr("Trap reactance on the other bands"), m_bands);
    m_rs = new Rollup(prefix + "_steps", tr("Build steps"), m_steps);
    m_rp = new Rollup(prefix + "_parts", tr("Parts"), m_parts);
    v->addWidget(cards); v->addWidget(drawing); v->addWidget(checks); v->addWidget(m_rb); v->addWidget(m_rs); v->addWidget(m_rp); v->addStretch(1);
}
void TrapTables::set(const std::vector<Check> &ck, const std::vector<TrapBand> &bs, const QStringList &steps, const std::vector<Part> &parts)
{
    checks->setChecks(ck);
    m_bands->setRowCount((int)bs.size()); QStringList blk; int ind = 0, cap = 0;
    for (int r = 0; r < (int)bs.size(); r++) {
        const TrapBand &b = bs[r];
        QString eq = b.nearRes ? S("blocks (near resonance)") : b.x > 0 ? f(b.eqUH, 2) + S(" µH") : f(b.eqPF, 1) + " pF";
        QStringList v = {b.band, f(b.fMHz, 3), b.nearRes ? S("≈ ∞") : rnd(b.x), b.kind, eq};
        for (int c = 0; c < 5; c++) { QTableWidgetItem *it = new QTableWidgetItem(v[c]); if (b.nearRes) it->setForeground(sevColor("fail")); m_bands->setItem(r, c, it); }
        if (b.nearRes) blk << b.band; else if (b.x > 0) ind++; else cap++;
    }
    m_rb->setSummary(QString("%1 bands · <span style='color:%2'>blocks %3</span> · %4 inductive · %5 capacitive").arg(bs.size()).arg(sevColor(blk.isEmpty() ? "ok" : "fail").name(), blk.isEmpty() ? "none" : blk.join(", ")).arg(ind).arg(cap));
    m_steps->setText(stepsHtml(steps)); m_rs->setSummary(QString("%1 steps").arg(steps.size()));
    m_parts->setRowCount((int)parts.size()); double q = 0;
    for (int r = 0; r < (int)parts.size(); r++) { m_parts->setItem(r, 0, new QTableWidgetItem(jsNum(parts[r].qty))); m_parts->setItem(r, 1, new QTableWidgetItem(parts[r].item)); q += parts[r].qty; }
    m_rp->setSummary(QString("%1 line items, %2 pieces").arg(parts.size()).arg(jsNum(q)));
}

static QString bandsSheet(const std::vector<TrapBand> &bs)
{
    QString h = "<table class='t' cellspacing='0'><tr><th>Band</th><th>MHz</th><th>X</th><th>Acts like</th></tr>";
    for (const TrapBand &b : bs) h += "<tr><td>" + b.band + "</td><td class='n'>" + f(b.fMHz, 3) + "</td><td class='n'>" + (b.nearRes ? S("≈ ∞") : rnd(b.x)) + "</td><td>" + esc(b.kind + (b.nearRes ? QString() : b.x > 0 ? " " + f(b.eqUH, 2) + S(" µH") : " " + f(b.eqPF, 1) + " pF")) + "</td></tr>";
    return h + "</table>";
}
static QString trapSheet(const QString &title, const QList<QPair<QString, QString>> &rows, const std::vector<Check> &checks, const QStringList &steps, const std::vector<Part> &parts, const std::vector<TrapBand> &bs)
{
    QString h = "<html><head>" + sheetCss() + "</head><body><h1>" + esc(S("KE9ESU build sheet — ") + title) + "</h1><div class='hint'>" + QDate::currentDate().toString(Qt::ISODate) + S(" · Coil64 KE9ESU · estimates only — confirm resonance with a dip meter / analyser before installing</div>");
    h += "<h2>Specification</h2><table class='t' cellspacing='0'>"; for (auto &r : rows) h += "<tr><th>" + esc(r.first) + "</th><td>" + esc(r.second) + "</td></tr>"; h += "</table>";
    h += "<h2>Drawing</h2><img src='ke9esu://drawing' width='640'>";
    h += "<h2>Parts</h2>" + partsTableHtml(parts) + "<h2>Build steps</h2>" + stepsHtml(steps) + "<h2>Checks</h2>" + ChecksBar::sheetHtml(checks) + "<h2>Reactance on other bands</h2>" + bandsSheet(bs);
    return h + "</body></html>";
}

/* ------------------------------------------------------------------ Coax trap */
CoaxTrapDialog::CoaxTrapDialog(QWidget *parent) : Ke9esuDialog(Win::Coax, parent)
{
    initData(); applyCoreUnits();
    QWidget *in = new QWidget; QFormLayout *fl = new QFormLayout(in);
    m_f = new FreqPicker; m_req = new QLabel; m_req->setWordWrap(true); m_req->setObjectName("ke9esuReq"); m_req->hide();
    m_coax = new QComboBox; for (const Coax &c : coaxList()) m_coax->addItem(c.label, c.id); m_coax->addItem(tr("Custom coax"), "custom");
    m_pf = dsp(5, 60, 30.8, 1, " pF/ft"); m_vf = dsp(0.5, 1, 0.66, 2); m_cOd = dsp(0.05, 1, 0.195, 3, " in"); m_z0 = dsp(25, 150, 50, 0, S(" Ω"));
    m_custom = new QWidget; QFormLayout *cf = new QFormLayout(m_custom); cf->setContentsMargins(0, 0, 0, 0);
    cf->addRow(tr("Capacitance"), m_pf); cf->addRow(tr("Velocity factor"), m_vf); cf->addRow(tr("Outside diameter"), m_cOd); cf->addRow(tr("Z0"), m_z0);
    m_form = new QComboBox; for (const PvcForm &p : pvcForms()) m_form->addItem(p.label, p.id); m_form->addItem(tr("Custom form"), "custom");
    m_od = new LenSpin(6.35, 304.8, 60.325); m_spacing = new LenSpin(0, 50.8, 0); m_lead = new LenSpin(0, 609.6, 25.4); m_trim = new LenSpin(0, 304.8, 25.4);
    m_model = new QComboBox; m_model->addItem(tr("Lumped (Wheeler L, coax C)"), "lumped"); m_model->addItem(tr("Stub-corrected"), "stub");
    m_proj = new ProjectBox(Win::Coax, ProjectBox::Power | ProjectBox::ZEnd | ProjectBox::Duty);
    fl->addRow(tr("Trap frequency"), m_f); fl->addRow(m_req); fl->addRow(tr("Coax"), m_coax); fl->addRow(m_custom); fl->addRow(tr("Coil form"), m_form);
    fl->addRow(tr("Form OD"), m_od); fl->addRow(tr("Turn spacing"), m_spacing); fl->addRow(tr("Leads (each end)"), m_lead); fl->addRow(tr("Trim allowance"), m_trim);
    fl->addRow(tr("Model"), m_model); fl->addRow(m_proj);
    QScrollArea *sa = new QScrollArea; sa->setWidget(in); sa->setWidgetResizable(true); sa->setMinimumWidth(420);
    m_t = new TrapTables("coax"); QScrollArea *ra = new QScrollArea; ra->setWidget(m_t); ra->setWidgetResizable(true); ra->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QSplitter *sp = new QSplitter; sp->addWidget(sa); sp->addWidget(ra); sp->setStretchFactor(1, 1);
    QVBoxLayout *bl = new QVBoxLayout(body()); bl->setContentsMargins(0, 0, 0, 0); bl->addWidget(sp);
    finishLayout(true, tr("Accept → Coil64 (coil inductance)"));
    resize(1180, 760);
    sp->setSizes({440, 740});
    connect(m_f, &FreqPicker::changed, this, &CoaxTrapDialog::recalc);
    for (QComboBox *c : {m_coax, m_model}) connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CoaxTrapDialog::recalc);
    connect(m_form, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (m_internal) return; QString id = m_form->currentData().toString();
        for (const PvcForm &p : pvcForms()) if (p.id == id) { m_internal = true; m_od->setValueMM(p.odIn * 25.4); m_internal = false; }
        recalc(); });
    connect(m_od, &LenSpin::changedMM, this, [this] { if (m_internal) return; m_internal = true; m_form->setCurrentIndex(m_form->findData("custom")); m_internal = false; recalc(); });
    for (LenSpin *l : {m_spacing, m_lead, m_trim}) connect(l, &LenSpin::changedMM, this, &CoaxTrapDialog::recalc);
    for (QDoubleSpinBox *s : {m_pf, m_vf, m_cOd, m_z0}) connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CoaxTrapDialog::recalc);
    connect(m_proj, &ProjectBox::edited, this, &CoaxTrapDialog::recalc);
    connect(Project::instance(), &Project::unitsChanged, this, &CoaxTrapDialog::recalc);
    connect(Project::instance(), &Project::loaded, this, [this] { loadState(Project::instance()->state(window())); });
    QJsonObject st = Project::instance()->state(window());
    if (st.isEmpty()) st = QJsonDocument::fromJson(ke9esuSettings()->value("KE9ESU/state/" + window()).toByteArray()).object();
    setInput(st.isEmpty() ? CoaxTrapInput() : coaxInputFromJson(st.value("input").toObject()));
}

CoaxTrapInput CoaxTrapDialog::input() const
{
    CoaxTrapInput i; i.fMHz = m_f->mhz(); i.coaxId = m_coax->currentData().toString(); i.formId = m_form->currentData().toString(); i.formOdIn = m_od->valueMM() / 25.4;
    i.spacingIn = m_spacing->valueMM() / 25.4; i.leadIn = m_lead->valueMM() / 25.4; i.trimIn = m_trim->valueMM() / 25.4; i.model = m_model->currentData().toString();
    i.powerW = m_proj->power(); i.zEnd = m_proj->zEnd(); i.duty = m_proj->duty(); i.dutyPercent = m_proj->dutyPercent();
    i.customPfFt = m_pf->value(); i.customVf = m_vf->value(); i.customOdIn = m_cOd->value(); i.customZ0 = m_z0->value();
    return i;
}
void CoaxTrapDialog::setInput(const CoaxTrapInput &raw)
{
    CoaxTrapInput i = normalizeCoaxTrap(raw); m_internal = true;
    m_f->setMhz(i.fMHz); m_coax->setCurrentIndex(qMax(0, m_coax->findData(i.coaxId))); m_form->setCurrentIndex(qMax(0, m_form->findData(i.formId)));
    m_od->setValueMM(i.formOdIn * 25.4); m_spacing->setValueMM(i.spacingIn * 25.4); m_lead->setValueMM(i.leadIn * 25.4); m_trim->setValueMM(i.trimIn * 25.4);
    m_model->setCurrentIndex(qMax(0, m_model->findData(i.model))); m_pf->setValue(i.customPfFt); m_vf->setValue(i.customVf); m_cOd->setValue(i.customOdIn); m_z0->setValue(i.customZ0);
    Project *p = Project::instance();
    if (!p->has(Key::Power)) m_proj->setValues(i.powerW, p->num(Key::ZSys, 50), p->num(Key::Vswr, 3), i.duty, i.dutyPercent, i.zEnd, p->num(Key::Ambient, 30), true);
    else m_proj->setValues(p->num(Key::Power), p->num(Key::ZSys, 50), p->num(Key::Vswr, 3), p->get(Key::Duty, i.duty).toString(), p->num(Key::DutyPct, i.dutyPercent), p->num(Key::ZEnd, i.zEnd), p->num(Key::Ambient, 30), false);
    m_internal = false; recalc();
}

void CoaxTrapDialog::recalc()
{
    if (m_internal) return;
    applyCoreUnits();
    m_custom->setVisible(m_coax->currentData() == "custom");
    m_r = coaxTrap(input()); const CoaxTrapResult &r = m_r;
    bool mixed = false; Q_UNUSED(mixed)
    QList<QPair<QString, QString>> c = {
        {"Turns", f(r.n, 2)}, {"Cut coax", inchTxt(r.cutLenIn, 1, S("″"))}, {"On the coil", inchTxt(r.coaxLenIn, 1, S("″"))},
        {"Winding length", inchTxt(r.windingLenIn, 2, S("″")) + " (pitch " + inchTxt(r.pitchIn, 3, S("″")) + ")"}, {"L (Wheeler)", f(r.L_uH, 3) + S(" µH")}, {"C (coax)", f(r.C_pF, 1) + " pF"},
        {"X at resonance", rnd(r.X) + S(" Ω")}, {"Resonance check", f(r.fCheckMHz, 3) + " MHz"}, {"Stub-model estimate", f(r.fStubMHz, 3) + " MHz"},
        {"Q (est.) / Rp", rnd(r.q) + " / " + fmtOhm(r.rpOhm)}, {"Trap voltage", rnd(r.vTrap.rms) + " V RMS"}};
    m_t->cards->setCards(c);
    m_t->drawing->setPainter([r](QPainter &p, const QRectF &rc) { paintCoaxTrap(p, rc, r); });
    m_t->set(r.checks, r.bands, r.steps, r.parts);
    Project::instance()->setState(window(), saveState());
    ke9esuSettings()->setValue("KE9ESU/state/" + window(), QJsonDocument(saveState()).toJson(QJsonDocument::Compact));
}

void CoaxTrapDialog::pullKey(const QString &key)
{
    Project *p = Project::instance();
    if (key == Key::F) m_f->setMhz(p->num(Key::F), p->source(Key::F));
    else if (key == Key::TrapReq) {
        QVariantMap t = p->get(key).toMap(); m_f->setMhz(t.value("fMHz").toDouble(), p->source(key));
        m_req->setText(S("◂ from ") + p->source(key) + ": " + esc(t.value("text").toString())); m_req->show();
    }
    recalc();
}
void CoaxTrapDialog::fillPull(QMenu *m)
{
    Project *p = Project::instance();
    addPull(m, Key::TrapReq, tr("Trap suggestion") + ": " + p->get(Key::TrapReq).toMap().value("band").toString() + " " + f(p->get(Key::TrapReq).toMap().value("fMHz").toDouble(), 3) + " MHz");
    addPull(m, Key::F, tr("Frequency") + ": " + f(p->num(Key::F), 3) + " MHz");
}
void CoaxTrapDialog::fillUseIn(QMenu *m)
{
    Project *p = Project::instance(); QString w = window();
    m->addAction(S("Coil L %1 µH → Coil64 one-layer coil (form, coax OD, pitch preloaded)").arg(f(m_r.L_uH, 3)), this, [this] { acceptToCoil64(); });
    m->addAction(S("L %1 µH + C %2 pF → Coil64 LC circuit").arg(f(m_r.L_uH, 3), f(m_r.C_pF, 1)), this, [this, p, w] {
        p->set(Key::L, m_r.L_uH, w); p->set(Key::C, m_r.C_pF, w); p->requestCoil64("lc", {{"L", m_r.L_uH}, {"C", m_r.C_pF}}); });
    m->addAction(S("Trap frequency %1 MHz → LC trap").arg(f(m_r.input.fMHz, 3)), this, [this, p, w] { p->set(Key::F, m_r.input.fMHz, w); p->requestWindow(Win::Lc, Key::F); });
}
void CoaxTrapDialog::acceptToCoil64()
{
    Project *p = Project::instance(); p->set(Key::L, m_r.L_uH, window());
    double od = m_r.coax.odIn * 25.4;
    p->requestCoil64("inductance", {{"L", m_r.L_uH}, {"kind", "air"}, {"D", m_r.input.formOdIn * 25.4}, {"wireDiaMM", od}, {"wireOdMM", od}, {"pitchMM", m_r.pitchIn * 25.4}, {"f", m_r.input.fMHz}});
}
QJsonObject CoaxTrapDialog::saveState() const { QJsonObject o; o.insert("input", toJson(input())); return o; }
void CoaxTrapDialog::loadState(const QJsonObject &o) { if (!o.isEmpty()) setInput(coaxInputFromJson(o.value("input").toObject())); }
QMap<QString, QImage> CoaxTrapDialog::sheetImages() const { CoaxTrapResult r = m_r; return {{"drawing", renderPaper([r](QPainter &p, const QRectF &rc) { paintCoaxTrap(p, rc, r); }, 720, 300)}}; }
QMap<QString, QImage> LcTrapDialog::sheetImages() const { LcTrapResult r = m_r; return {{"drawing", renderPaper([r](QPainter &p, const QRectF &rc) { paintLcTrap(p, rc, r); }, 720, 300)}}; }
QString CoaxTrapDialog::sheetTitle() const { return "Coax trap " + f(m_r.input.fMHz, 3) + " MHz"; }
QString CoaxTrapDialog::sheetHtml() const
{
    const CoaxTrapResult &r = m_r; const CoaxTrapInput &i = r.input; QString formLabel = i.formId == "custom" ? QString("custom") : QString();
    for (const PvcForm &p : pvcForms()) if (p.id == i.formId) formLabel = p.label;
    QList<QPair<QString, QString>> rows = {
        {"Trap frequency", f(i.fMHz, 3) + " MHz (" + (i.model == "stub" ? "stub-corrected" : "lumped") + " model)"},
        {"Coax", r.coax.label + S(" — ") + jsNum(r.coax.pfFt) + " pF/ft, OD " + inchTxt(r.coax.odIn, 3, S("″"))},
        {"Form", inchTxt(i.formOdIn, 3, S("″")) + " OD (" + formLabel + ")"},
        {"Turns / pitch", f(r.n, 2) + " turns, pitch " + inchTxt(r.pitchIn, 3, S("″")) + ", winding " + inchTxt(r.windingLenIn, 2, S("″"))},
        {"Coax length", inchTxt(r.coaxLenIn, 1, S("″")) + " on the coil; cut " + inchTxt(r.cutLenIn, 1, S("″"))},
        {"L / C / X", f(r.L_uH, 3) + S(" µH / ") + f(r.C_pF, 1) + " pF / " + rnd(r.X) + S(" Ω")},
        {"Check", f(r.fCheckMHz, 3) + S(" MHz lumped; ≈ ") + f(r.fStubMHz, 3) + " MHz with stub effect"},
        {"Power", jsNum(i.powerW) + " W " + i.duty + S("; ≈ ") + rnd(r.vTrap.rms) + " V RMS across the trap"}};
    return trapSheet(sheetTitle(), rows, r.checks, r.steps, r.parts, r.bands);
}

/* ------------------------------------------------------------------ LC trap */
LcTrapDialog::LcTrapDialog(QWidget *parent) : Ke9esuDialog(Win::Lc, parent)
{
    initData(); applyCoreUnits();
    QWidget *in = new QWidget; QFormLayout *fl = new QFormLayout(in);
    m_f = new FreqPicker; m_req = new QLabel; m_req->setWordWrap(true); m_req->setObjectName("ke9esuReq"); m_req->hide();
    m_mode = new QComboBox; m_mode->addItem(tr("Choose reactance X"), "X"); m_mode->addItem(tr("Choose inductance L"), "L"); m_mode->addItem(tr("Choose capacitance C"), "C");
    m_x = dsp(10, 2000, 200, 0, S(" Ω")); m_l = dsp(0.05, 500, 5, 3, S(" µH")); m_c = dsp(1, 20000, 100, 1, " pF");
    m_lMark = new FromMarker; m_cMark = new FromMarker;
    m_snap = new QComboBox; m_snap->addItem(tr("exact C"), "none"); m_snap->addItem("E6", "E6"); m_snap->addItem("E12", "E12"); m_snap->addItem("E24", "E24");
    m_coreType = new QComboBox; m_coreType->addItem(tr("Air-core coil"), "air"); m_coreType->addItem(tr("Toroid"), "toroid");
    m_form = new QComboBox; for (const PvcForm &p : pvcForms()) m_form->addItem(p.label, p.id); m_form->addItem(tr("Custom form"), "custom");
    m_dia = new LenSpin(3, 300, 48.26); m_wire = new QComboBox; m_wireMark = new FromMarker; m_pitch = new LenSpin(0, 50, 0); m_pitch->setSpecialValueText(tr("auto"));
    m_core = new CoreSelector; m_stack = new QSpinBox; m_stack->setRange(1, 4);
    m_cap = new QComboBox; for (const CapType &c : capTypes()) m_cap->addItem(c.label, c.id);
    m_lead = new LenSpin(0, 1000, 50);
    m_coilNote = new QLabel; m_coilNote->setWordWrap(true); m_coilNote->setObjectName("ke9esuReq"); m_coilNote->hide();
    m_proj = new ProjectBox(Win::Lc, ProjectBox::Power | ProjectBox::ZEnd | ProjectBox::Duty | ProjectBox::Ambient);
    fl->addRow(tr("Trap frequency"), m_f); fl->addRow(m_req);
    fl->addRow(tr("Design from"), m_mode); fl->addRow(tr("Reactance X"), m_x); fl->addRow(tr("Inductance L"), row({m_l, m_lMark})); fl->addRow(tr("Capacitance C"), row({m_c, m_cMark}));
    fl->addRow(tr("Snap C to"), m_snap); fl->addRow(tr("Inductor"), m_coreType);
    m_air = new QWidget; QFormLayout *af = new QFormLayout(m_air); af->setContentsMargins(0, 0, 0, 0);
    af->addRow(tr("Coil form"), m_form); af->addRow(tr("Form diameter"), m_dia); af->addRow(tr("Wire"), row({m_wire, m_wireMark})); af->addRow(tr("Pitch"), m_pitch);
    m_tor = new QWidget; QFormLayout *tf = new QFormLayout(m_tor); tf->setContentsMargins(0, 0, 0, 0); tf->addRow(tr("Core"), m_core); tf->addRow(QString(), m_core->marker()); tf->addRow(tr("Stack"), m_stack);
    fl->addRow(m_air); fl->addRow(m_tor); fl->addRow(m_coilNote);
    fl->addRow(tr("Capacitor"), m_cap); fl->addRow(tr("Lead length"), m_lead); fl->addRow(m_proj);
    QScrollArea *sa = new QScrollArea; sa->setWidget(in); sa->setWidgetResizable(true); sa->setMinimumWidth(440);
    m_t = new TrapTables("lc"); QScrollArea *ra = new QScrollArea; ra->setWidget(m_t); ra->setWidgetResizable(true); ra->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QSplitter *sp = new QSplitter; sp->addWidget(sa); sp->addWidget(ra); sp->setStretchFactor(1, 1);
    QVBoxLayout *bl = new QVBoxLayout(body()); bl->setContentsMargins(0, 0, 0, 0); bl->addWidget(sp);
    finishLayout(true, tr("Accept → Coil64 (inductance)"));
    resize(1200, 820);
    sp->setSizes({450, 750});
    auto rc = [this] { if (!m_internal) recalc(); };
    connect(m_f, &FreqPicker::changed, this, rc);
    for (QComboBox *c : {m_mode, m_snap, m_coreType, m_cap}) connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this, rc);
    connect(m_wire, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { if (m_internal) return; m_wireMark->clear();
        if (const Wire *w = coilWireById(m_wire->currentData().toString())) if (isVisible()) Project::instance()->set(Key::Wire, wireInfo(*w), window()); recalc(); });
    connect(m_x, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, rc);
    connect(m_l, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this] { if (m_internal) return; m_lMark->clear(); m_coilNote->hide(); recalc(); });
    connect(m_c, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this] { if (m_internal) return; m_cMark->clear(); recalc(); });
    connect(m_stack, QOverload<int>::of(&QSpinBox::valueChanged), this, rc);
    connect(m_form, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (m_internal) return; QString id = m_form->currentData().toString();
        for (const PvcForm &p : pvcForms()) if (p.id == id) { m_internal = true; m_dia->setValueMM(p.odIn * 25.4); m_internal = false; }
        recalc(); });
    connect(m_dia, &LenSpin::changedMM, this, [this] { if (m_internal) return; m_internal = true; m_form->setCurrentIndex(m_form->findData("custom")); m_internal = false; recalc(); });
    connect(m_pitch, &LenSpin::changedMM, this, rc); connect(m_lead, &LenSpin::changedMM, this, rc);
    connect(m_core, &CoreSelector::coreChanged, this, [this] { if (m_internal) return; if (isVisible()) Project::instance()->set(Key::Core, m_core->coreId(), window()); recalc(); });
    connect(m_core, &CoreSelector::browseRequested, this, [] { Project::instance()->requestWindow(Win::CoreDb); });
    connect(m_proj, &ProjectBox::edited, this, rc);
    connect(Project::instance(), &Project::unitsChanged, this, [this] {
        applyCoreUnits(); QString cur = m_wire->currentData().toString(); m_internal = true; m_wire->clear();
        for (const Wire &w : coilWires()) m_wire->addItem(wireLabel(w), w.id); m_wire->setCurrentIndex(qMax(0, m_wire->findData(cur))); m_internal = false; recalc(); });
    connect(Project::instance(), &Project::changed, this, [this](const QString &key, const QString &src) {
        if (src == window() || !ke9esuSettings()->value("KE9ESU/link/live", true).toBool()) return;
        if (key == Key::Core && m_coreType->currentData() == "toroid") pullKey(key);
        if (key == Key::Wire && m_coreType->currentData() == "air") pullKey(key);
    });
    connect(Project::instance(), &Project::loaded, this, [this] { loadState(Project::instance()->state(window())); });
    for (const Wire &w : coilWires()) m_wire->addItem(wireLabel(w), w.id);
    QJsonObject st = Project::instance()->state(window());
    if (st.isEmpty()) st = QJsonDocument::fromJson(ke9esuSettings()->value("KE9ESU/state/" + window()).toByteArray()).object();
    if (!st.isEmpty()) loadState(st); else { m_core->setDb("amidon"); setInput(LcTrapInput()); }
}

LcTrapInput LcTrapDialog::input() const
{
    LcTrapInput i; i.fMHz = m_f->mhz(); i.mode = m_mode->currentData().toString(); i.xOhm = m_x->value(); i.lUH = m_l->value(); i.cPF = m_c->value();
    i.snap = m_snap->currentData().toString(); i.coreType = m_coreType->currentData().toString(); i.formId = m_form->currentData().toString(); i.formDiaMM = m_dia->valueMM();
    i.wireId = m_wire->currentData().toString(); i.pitchMM = m_pitch->valueMM(); i.coreId = m_core->coreId(); i.stack = m_stack->value();
    i.powerW = m_proj->power(); i.zEnd = m_proj->zEnd(); i.duty = m_proj->duty(); i.dutyPercent = m_proj->dutyPercent(); i.capType = m_cap->currentData().toString();
    i.leadMM = m_lead->valueMM(); i.ambientC = m_proj->ambient();
    return i;
}
void LcTrapDialog::setInput(const LcTrapInput &raw)
{
    LcTrapInput i = normalizeLcTrap(raw); m_internal = true;
    m_f->setMhz(i.fMHz); m_mode->setCurrentIndex(qMax(0, m_mode->findData(i.mode))); m_x->setValue(i.xOhm); m_l->setValue(i.lUH); m_c->setValue(i.cPF);
    m_snap->setCurrentIndex(qMax(0, m_snap->findData(i.snap))); m_coreType->setCurrentIndex(qMax(0, m_coreType->findData(i.coreType)));
    m_form->setCurrentIndex(qMax(0, m_form->findData(i.formId))); m_dia->setValueMM(i.formDiaMM); m_wire->setCurrentIndex(qMax(0, m_wire->findData(i.wireId)));
    m_pitch->setValueMM(i.pitchMM); m_core->setCoreId(i.coreId); m_stack->setValue((int)i.stack); m_cap->setCurrentIndex(qMax(0, m_cap->findData(i.capType))); m_lead->setValueMM(i.leadMM);
    Project *p = Project::instance();
    if (!p->has(Key::Power)) m_proj->setValues(i.powerW, p->num(Key::ZSys, 50), p->num(Key::Vswr, 3), i.duty, i.dutyPercent, i.zEnd, i.ambientC, true);
    else m_proj->setValues(p->num(Key::Power), p->num(Key::ZSys, 50), p->num(Key::Vswr, 3), p->get(Key::Duty, i.duty).toString(), p->num(Key::DutyPct, i.dutyPercent), p->num(Key::ZEnd, i.zEnd), p->num(Key::Ambient, i.ambientC), false);
    m_internal = false; recalc();
}

void LcTrapDialog::recalc()
{
    if (m_internal) return;
    applyCoreUnits();
    QString mode = m_mode->currentData().toString(); bool tor = m_coreType->currentData() == "toroid";
    m_x->setEnabled(mode == "X"); m_l->setEnabled(mode == "L"); m_c->setEnabled(mode == "C"); m_air->setVisible(!tor); m_tor->setVisible(tor);
    m_r = lcTrap(input()); const LcTrapResult &r = m_r; const LcTrapInput &i = r.input;
    QString cLab = "C" + (std::fabs(r.C_pF - r.cIdealPF) > 0.05 ? (tor && i.snap == "none" ? QString(" (re-solved for whole turns)") : " (" + i.snap + " value)") : QString());
    QList<QPair<QString, QString>> c = {{"L", f(r.L_uH, 3) + S(" µH")}, {cLab, f(r.C_pF, 1) + " pF" + (std::fabs(r.C_pF - r.cIdealPF) > 0.05 ? " (calc " + f(r.cIdealPF, 1) + ")" : QString())}, {"X at resonance", rnd(r.X) + S(" Ω")}};
    if (tor) { const LcToroid &t = r.toroid; c << qMakePair(QString("Turns"), QString::number(t.N) + " on " + t.core->id + (i.stack > 1 ? S(" ×") + jsNum(i.stack) : QString()))
               << qMakePair(QString("L with %1 T").arg(t.N), f(t.uH, 3) + S(" µH")) << qMakePair(QString("Wire"), mmTxt(t.wireLenMM)) << qMakePair(QString("Core temp (est.)"), rnd(t.coreTempC) + S(" °C")); }
    else { const LcCoil &k = r.coil; c << qMakePair(QString("Turns"), f(k.N, 2)) << qMakePair(QString("Coil length"), mmTxt(k.lenMM, 1) + " (pitch " + f(k.pitchMM, 2) + ")")
               << qMakePair(QString("Wire"), mmTxt(k.wireLenMM)) << qMakePair(QString("Coil SRF / Cs"), f(k.srfMHz, 1) + " MHz / " + f(k.csPF, 2) + " pF"); }
    c << qMakePair(QString("Q (est.) / Rp"), rnd(r.q) + " / " + fmtOhm(r.rpOhm)) << qMakePair(QString("Cap rating"), S("≥ ") + jsNum(r.capRatingV) + " V")
      << qMakePair(QString("Cap RF current"), f(r.capCurrentA, 2) + " A RMS") << qMakePair(QString("Trap voltage"), rnd(r.vTrap.rms) + " V RMS");
    m_t->cards->setCards(c);
    m_t->drawing->setPainter([r = m_r](QPainter &p, const QRectF &rc) { paintLcTrap(p, rc, r); });
    m_t->set(r.checks, r.bands, r.steps, r.parts);
    Project::instance()->setState(window(), saveState());
    ke9esuSettings()->setValue("KE9ESU/state/" + window(), QJsonDocument(saveState()).toJson(QJsonDocument::Compact));
}

void LcTrapDialog::pullKey(const QString &key)
{
    Project *p = Project::instance(); QString src = p->source(key);
    if (key == Key::F) m_f->setMhz(p->num(Key::F), src);
    else if (key == Key::TrapReq) { QVariantMap t = p->get(key).toMap(); m_f->setMhz(t.value("fMHz").toDouble(), src); m_req->setText(S("◂ from ") + src + ": " + esc(t.value("text").toString())); m_req->show(); }
    else if (key == Key::L) { m_internal = true; m_mode->setCurrentIndex(m_mode->findData("L")); m_l->setValue(p->num(key)); m_internal = false; m_lMark->setFrom(src); }
    else if (key == Key::C) { m_internal = true; m_mode->setCurrentIndex(m_mode->findData("C")); m_c->setValue(p->num(key)); m_internal = false; m_cMark->setFrom(src); }
    else if (key == Key::Core) {
        QString id = p->get(key).toString(); m_internal = true; m_coreType->setCurrentIndex(m_coreType->findData("toroid")); m_core->setCoreId(id);
        if (p->has(Key::Stack)) m_stack->setValue(p->get(Key::Stack).toInt()); m_internal = false; if (m_core->coreId() == id) m_core->marker()->setFrom(src);
    } else if (key == Key::Wire) {
        QVariantMap w = p->get(key).toMap(); double d = w.value("diaMM").toDouble(); if (d > 0) { QString id = nearestWire(coilWires(), d, QString(), false);
            m_internal = true; m_wire->setCurrentIndex(qMax(0, m_wire->findData(id))); m_internal = false; m_wireMark->setFrom(src); }
    } else if (key == Key::Coil) {
        // A coil designed in Coil64: evaluate its Q and SRF as this trap's inductor (form diameter, wire, pitch and L taken over).
        QVariantMap k = p->get(key).toMap(); m_internal = true;
        m_coreType->setCurrentIndex(m_coreType->findData("air")); m_mode->setCurrentIndex(m_mode->findData("L")); m_l->setValue(k.value("L").toDouble());
        m_form->setCurrentIndex(m_form->findData("custom")); m_dia->setValueMM(k.value("D").toDouble());
        QString id = nearestWire(coilWires(), k.value("d").toDouble(), QString(), false); m_wire->setCurrentIndex(qMax(0, m_wire->findData(id)));
        m_pitch->setValueMM(k.value("p").toDouble()); if (k.value("f").toDouble() >= 0.5 && k.value("f").toDouble() <= 60) m_f->setMhz(k.value("f").toDouble(), src);
        m_internal = false; m_lMark->setFrom(src); m_wireMark->setFrom(src);
        const Wire *wv = coilWireById(id);
        m_coilNote->setText(S("◂ from %1: N = %2, D = %3, d = %4 (nearest listed wire: %5), p = %6, L = %7 µH. Q and SRF below are this coil evaluated as the trap inductor.")
                                .arg(src, f(k.value("N").toDouble(), 2), mmTxt(k.value("D").toDouble(), 2), mmTxt(k.value("d").toDouble(), 3), wv ? wireLabel(*wv) : id, mmTxt(k.value("p").toDouble(), 2), f(k.value("L").toDouble(), 3)));
        m_coilNote->show();
    }
    recalc();
}
void LcTrapDialog::fillPull(QMenu *m)
{
    Project *p = Project::instance();
    addPull(m, Key::TrapReq, tr("Trap suggestion") + ": " + p->get(Key::TrapReq).toMap().value("band").toString() + " " + f(p->get(Key::TrapReq).toMap().value("fMHz").toDouble(), 3) + " MHz");
    addPull(m, Key::F, tr("Frequency") + ": " + f(p->num(Key::F), 3) + " MHz");
    addPull(m, Key::Coil, tr("Coil designed in Coil64 (evaluate Q and SRF)") + ": L " + f(p->get(Key::Coil).toMap().value("L").toDouble(), 3) + S(" µH"));
    addPull(m, Key::L, tr("Inductance") + ": " + f(p->num(Key::L), 3) + S(" µH"));
    addPull(m, Key::C, tr("Capacitance") + ": " + f(p->num(Key::C), 1) + " pF");
    addPull(m, Key::Core, tr("Core (toroid)") + ": " + p->get(Key::Core).toString());
    addPull(m, Key::Wire, tr("Wire") + ": " + p->get(Key::Wire).toMap().value("label").toString());
}
void LcTrapDialog::fillUseIn(QMenu *m)
{
    Project *p = Project::instance(); QString w = window();
    m->addAction(S("L %1 µH → Coil64 (%2)").arg(f(m_r.L_uH, 3), m_r.hasToroid ? QString("ferrite/iron toroid, core preloaded") : QString("one-layer coil, form/wire/pitch preloaded")), this, [this] { acceptToCoil64(); });
    m->addAction(S("L %1 µH + C %2 pF → Coil64 LC circuit").arg(f(m_r.L_uH, 3), f(m_r.C_pF, 1)), this, [this, p, w] {
        p->set(Key::L, m_r.L_uH, w); p->set(Key::C, m_r.C_pF, w); p->requestCoil64("lc", {{"L", m_r.L_uH}, {"C", m_r.C_pF}}); });
    m->addAction(S("C %1 pF → Find standard-part combination").arg(f(m_r.cIdealPF, 1)), this, [this, p, w] {
        p->set(Key::CombReq, QVariantMap{{"type", "C"}, {"value", m_r.cIdealPF * 1e-12}, {"label", "LC trap capacitor"}}, w); p->requestWindow(Win::Combiner, Key::CombReq); });
    m->addAction(S("L %1 µH → Find standard-part combination").arg(f(m_r.L_uH, 3)), this, [this, p, w] {
        p->set(Key::CombReq, QVariantMap{{"type", "L"}, {"value", m_r.L_uH * 1e-6}, {"label", "LC trap inductor"}}, w); p->requestWindow(Win::Combiner, Key::CombReq); });
    m->addAction(S("Trap frequency %1 MHz → Coax trap").arg(f(m_r.input.fMHz, 3)), this, [this, p, w] { p->set(Key::F, m_r.input.fMHz, w); p->requestWindow(Win::Coax, Key::F); });
    if (m_r.hasToroid) m->addAction(tr("Core %1 → Core database").arg(m_r.toroid.core->id), this, [this, p, w] { p->set(Key::Core, m_r.toroid.core->id, w); p->requestWindow(Win::CoreDb, Key::Core); });
}
void LcTrapDialog::acceptToCoil64()
{
    Project *p = Project::instance(); p->set(Key::L, m_r.L_uH, window()); p->set(Key::C, m_r.C_pF, window());
    if (m_r.hasToroid) p->requestCoil64("inductance", {{"L", m_r.L_uH}, {"kind", "ferrite"}, {"coreId", m_r.toroid.core->id}, {"stack", m_r.input.stack}, {"N", m_r.toroid.N}, {"wireDiaMM", wireDiaMM(*m_r.toroid.wire)}});
    else p->requestCoil64("inductance", {{"L", m_r.L_uH}, {"kind", "air"}, {"D", m_r.input.formDiaMM}, {"wireDiaMM", wireDiaMM(*m_r.coil.wire)}, {"wireOdMM", m_r.coil.wire->od}, {"pitchMM", m_r.coil.pitchMM}, {"f", m_r.input.fMHz}});
}
QJsonObject LcTrapDialog::saveState() const { QJsonObject o; o.insert("input", toJson(input())); o.insert("db", m_core->db()); return o; }
void LcTrapDialog::loadState(const QJsonObject &o) { if (o.isEmpty()) return; m_core->setDb(o.value("db").toString("amidon")); setInput(lcInputFromJson(o.value("input").toObject())); }
QString LcTrapDialog::sheetTitle() const { return "LC trap " + f(m_r.input.fMHz, 3) + " MHz"; }
QString LcTrapDialog::sheetHtml() const
{
    const LcTrapResult &q = m_r; const LcTrapInput &li = q.input; bool tor = q.hasToroid;
    QString lTxt = f(q.L_uH, 3) + S(" µH — ") + (tor ? QString::number(q.toroid.N) + " turns " + wireLabel(*q.toroid.wire) + " on " + q.toroid.core->id + " (" + q.toroid.core->db + ", AL " + jsNum(q.toroid.core->al) + S(" nH/T²)")
                                               : f(q.coil.N, 2) + " turns " + wireLabel(*q.coil.wire) + S(" on Ø") + mmTxt(li.formDiaMM, 1) + ", pitch " + f(q.coil.pitchMM, 2) + " mm, length " + mmTxt(q.coil.lenMM, 1));
    QList<QPair<QString, QString>> rows = {{"Trap frequency", f(li.fMHz, 3) + " MHz"}, {"L", lTxt},
        {"C", f(q.C_pF, 1) + S(" pF, ≥ ") + jsNum(q.capRatingV) + " V, " + (capType(li.capType) ? capType(li.capType)->label : li.capType) + ", RF current " + f(q.capCurrentA, 2) + " A"},
        {"X / Q / Rp", rnd(q.X) + S(" Ω / ") + rnd(q.q) + " / " + fmtOhm(q.rpOhm)}, {"Power", jsNum(li.powerW) + " W " + li.duty + S("; ≈ ") + rnd(q.vTrap.rms) + " V RMS across the trap"}};
    return trapSheet(sheetTitle(), rows, q.checks, q.steps, q.parts, q.bands);
}

} // namespace ke9esu
