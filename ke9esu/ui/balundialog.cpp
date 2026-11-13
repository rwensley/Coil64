/* balundialog.cpp - see balundialog.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "balundialog.h"
#include "coreui.h"
#include "diagram.h"
#include "project.h"
#include "serialize.h"
#include "ke9esu_util.h"
#include "theme.h"
#include <QBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QMenu>
#include <QDate>
#include <QJsonDocument>
#include <cmath>

namespace ke9esu {

static QDoubleSpinBox *dspin(double lo, double hi, double v, int dec, const QString &suf = QString())
{
    QDoubleSpinBox *s = new QDoubleSpinBox; s->setRange(lo, hi); s->setDecimals(dec); s->setValue(v); s->setSuffix(suf); s->setKeyboardTracking(false); return s;
}
static QWidget *hbox(std::initializer_list<QWidget *> ws)
{
    QWidget *w = new QWidget; QHBoxLayout *h = new QHBoxLayout(w); h->setContentsMargins(0, 0, 0, 0); for (QWidget *x : ws) h->addWidget(x); h->addStretch(); return w;
}
static QTableWidget *table(const QStringList &headers)
{
    QTableWidget *t = new QTableWidget(0, headers.size()); t->setHorizontalHeaderLabels(headers); t->verticalHeader()->hide();
    t->setEditTriggers(QAbstractItemView::NoEditTriggers); t->setSelectionBehavior(QAbstractItemView::SelectRows); t->setAlternatingRowColors(true);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); t->horizontalHeader()->setStretchLastSection(true); t->setMinimumHeight(180);
    return t;
}
static QTableWidgetItem *cell(const QString &s, bool num = true) { QTableWidgetItem *i = new QTableWidgetItem(s); if (num) i->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter); return i; }

BalunDialog::BalunDialog(QWidget *parent) : Ke9esuDialog(Win::Balun, parent)
{
    initData(); applyCoreUnits();
    QSplitter *split = new QSplitter(Qt::Horizontal);
    // ---- inputs
    QWidget *in = new QWidget; QFormLayout *fl = new QFormLayout(in); fl->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_preset = new QComboBox; for (const Preset &p : presets()) m_preset->addItem(p.label, p.id); m_preset->addItem(S("— custom (edited) —"), "custom");
    m_presetMark = new FromMarker;
    m_top = new QComboBox; for (const Topology &t : topologies()) m_top->addItem(t.name, t.id);
    m_core = new CoreSelector;
    m_stack = new QSpinBox; m_stack->setRange(1, 4); m_stack->setSuffix(S(" core(s) stacked"));
    m_cond = new QComboBox; m_wireMark = new FromMarker;
    m_turns = new QSpinBox; m_turns->setRange(0, 60); m_turns->setSpecialValueText(tr("auto")); m_turns->setToolTip(tr("0 = automatic minimum for the XL / |Z| target"));
    m_fMin = dspin(0.1, 500, 1.8, 3, " MHz"); m_fMax = dspin(0.1, 500, 54, 3, " MHz");
    m_bandsHint = new QLabel; m_bandsHint->setWordWrap(true); m_bandsHint->setObjectName("ke9esuHint");
    m_xl = dspin(1, 20, 5, 1, S("× Z low"));
    m_target = dspin(100, 100000, 5000, 0, S(" Ω")); m_cm = dspin(0.1, 100, 10, 1, " %");
    m_cs = new QComboBox; m_cs->addItem(S("K9YC (1.3 pF)"), "k9yc"); m_cs->addItem("G3RBJ (geometry)", "g3rbj"); m_cs->addItem(tr("Manual"), "manual"); m_cs->addItem(tr("None"), "none");
    m_csPF = dspin(0, 50, 1.3, 2, " pF");
    m_lead = new LenSpin(0, 2000, 100);
    fl->addRow(tr("Preset"), hbox({m_preset, m_presetMark}));
    fl->addRow(tr("Topology"), m_top);
    fl->addRow(tr("Core"), m_core);
    fl->addRow(QString(), m_core->marker());
    fl->addRow(tr("Stack"), m_stack);
    fl->addRow(tr("Conductor"), hbox({m_cond, m_wireMark}));
    fl->addRow(tr("Turns (primary / tap)"), m_turns);
    fl->addRow(tr("Lowest frequency"), m_fMin);
    fl->addRow(tr("Highest frequency"), m_fMax);
    fl->addRow(QString(), m_bandsHint);
    m_xfOnly = new QWidget; QFormLayout *xf = new QFormLayout(m_xfOnly); xf->setContentsMargins(0, 0, 0, 0); xf->addRow(tr("XL target at lowest f"), m_xl);
    m_chokeOnly = new QWidget; QFormLayout *ck = new QFormLayout(m_chokeOnly); ck->setContentsMargins(0, 0, 0, 0);
    ck->addRow(tr("Choke |Z| target"), m_target); ck->addRow(tr("Common-mode voltage"), m_cm); ck->addRow(tr("Stray capacitance"), hbox({m_cs, m_csPF}));
    fl->addRow(m_xfOnly); fl->addRow(m_chokeOnly);
    fl->addRow(tr("Lead length (total)"), m_lead);
    m_proj = new ProjectBox(Win::Balun, ProjectBox::Power | ProjectBox::ZSys | ProjectBox::Vswr | ProjectBox::Duty | ProjectBox::Ambient);
    fl->addRow(m_proj);
    QScrollArea *sa = new QScrollArea; sa->setWidget(in); sa->setWidgetResizable(true); sa->setMinimumWidth(430);
    split->addWidget(sa);
    // ---- results
    QWidget *res = new QWidget; QVBoxLayout *rv = new QVBoxLayout(res);
    m_title = new QLabel; m_title->setObjectName("ke9esuTitle"); m_title->setTextFormat(Qt::RichText); m_title->setWordWrap(true);
    m_hint = new QLabel; m_hint->setWordWrap(true);
    m_cards = new Cards(4); m_checks = new ChecksBar;
    m_capNote = new QLabel; m_capNote->setWordWrap(true);
    QTabWidget *tabs = new QTabWidget;
    m_diag = new DesignView(DesignView::Winding); m_plot = new DesignView(DesignView::Impedance);
    QWidget *pw = new QWidget; QVBoxLayout *pv = new QVBoxLayout(pw); pv->setContentsMargins(0, 0, 0, 0); m_plotHint = new QLabel; m_plotHint->setWordWrap(true); pv->addWidget(m_plot, 1); pv->addWidget(m_plotHint);
    tabs->addTab(m_diag, tr("Winding diagram")); tabs->addTab(pw, tr("Impedance vs frequency"));
    tabs->setMinimumHeight(360);
    m_pf = table({"MHz", "R", "X", "|Z|", "B peak (mT)", "B heat (G)", "limit (G)", "loss key-down (W)", "loss matched (W)"});
    m_parts = table({"Qty", "Item"});
    m_steps = new QLabel; m_steps->setWordWrap(true); m_steps->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_rPf = new Rollup("balun_perfreq", tr("Per-frequency results"), m_pf);
    m_rParts = new Rollup("balun_parts", tr("Parts & cut lengths"), m_parts);
    m_rSteps = new Rollup("balun_steps", tr("Connections / build steps"), m_steps);
    rv->addWidget(m_title); rv->addWidget(m_hint); rv->addWidget(m_cards); rv->addWidget(m_checks); rv->addWidget(m_capNote);
    rv->addWidget(m_rPf); rv->addWidget(m_rParts); rv->addWidget(m_rSteps); rv->addWidget(tabs, 1);
    QScrollArea *rs = new QScrollArea; rs->setWidget(res); rs->setWidgetResizable(true); rs->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    split->addWidget(rs); split->setStretchFactor(1, 1);
    QVBoxLayout *bl = new QVBoxLayout(body()); bl->setContentsMargins(0, 0, 0, 0); bl->addWidget(split);
    finishLayout(true, tr("Accept → Coil64 (inductance)"));
    resize(1280, 860);
    split->setSizes({470, 810});

    // ---- wiring
    connect(m_preset, QOverload<int>::of(&QComboBox::activated), this, [this] { QString id = m_preset->currentData().toString(); if (id != "custom") applyPreset(id); });
    connect(m_top, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { if (m_internal) return; fillConductors(m_cond->currentData().toString()); m_turns->setValue(0); edited(); });
    connect(m_core, &CoreSelector::coreChanged, this, [this] { if (m_internal) return; m_turns->setValue(0); publishCore(); edited(); });
    connect(m_core, &CoreSelector::browseRequested, this, [] { Project::instance()->requestWindow(Win::CoreDb); });
    connect(m_stack, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { if (m_internal) return; m_turns->setValue(0); publishCore(); edited(); });
    connect(m_cond, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { if (m_internal) return; m_wireMark->clear(); publishWire(); edited(); });
    for (QSpinBox *s : {m_turns}) connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, &BalunDialog::edited);
    for (QDoubleSpinBox *s : {m_fMin, m_fMax, m_xl, m_target, m_cm, m_csPF}) connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BalunDialog::edited);
    connect(m_cs, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BalunDialog::edited);
    connect(m_lead, &LenSpin::changedMM, this, &BalunDialog::edited);
    connect(m_proj, &ProjectBox::edited, this, &BalunDialog::edited);
    connect(Project::instance(), &Project::changed, this, &BalunDialog::projectChanged);
    connect(Project::instance(), &Project::unitsChanged, this, [this] { applyCoreUnits(); fillConductors(m_cond->currentData().toString()); recalc(); });
    connect(Project::instance(), &Project::loaded, this, [this] { loadState(Project::instance()->state(window())); });

    QJsonObject st = Project::instance()->state(window());
    if (st.isEmpty()) st = QJsonDocument::fromJson(ke9esuSettings()->value("KE9ESU/state/" + window()).toByteArray()).object();
    if (!st.isEmpty()) loadState(st); else applyPreset("ke9esu");
}

void BalunDialog::fillConductors(const QString &sel)
{
    bool tr = topology(m_top->currentData().toString()) && topology(m_top->currentData().toString())->kind == "transformer";
    m_internal = true; bool was = m_internal; Q_UNUSED(was)
    m_cond->clear();
    for (const Wire &c : conductors()) if (!(tr && c.kind == "coax")) m_cond->addItem(wireLabel(c), c.id);
    int i = m_cond->findData(sel); m_cond->setCurrentIndex(i >= 0 ? i : m_cond->findData("awg16t"));
    m_internal = false;
}

BalunInput BalunDialog::input() const
{
    BalunInput i;
    i.topology = m_top->currentData().toString(); i.coreId = m_core->coreId(); i.conductorId = m_cond->currentData().toString();
    i.stack = m_stack->value(); i.turns = m_turns->value() ? m_turns->value() : NaN;
    i.zLow = m_proj->zSys(); i.powerW = m_proj->power(); i.vswr = m_proj->vswr(); i.fMinMHz = m_fMin->value(); i.fMaxMHz = m_fMax->value();
    i.duty = m_proj->duty(); i.dutyPercent = m_proj->dutyPercent(); i.xlFactor = m_xl->value(); i.chokeTarget = m_target->value(); i.cmPercent = m_cm->value();
    i.leadMM = m_lead->valueMM(); i.ambientC = m_proj->ambient(); i.csMode = m_cs->currentData().toString(); i.csPF = m_csPF->value();
    return i;
}

void BalunDialog::setInput(const BalunInput &raw, bool presetValues)
{
    BalunInput i = normalize(raw);
    m_internal = true;
    m_top->setCurrentIndex(qMax(0, m_top->findData(i.topology)));
    m_core->setCoreId(i.coreId);
    m_stack->setValue((int)i.stack);
    m_internal = false; fillConductors(i.conductorId); m_internal = true;
    m_turns->setValue(std::isnan(i.turns) ? 0 : (int)i.turns);
    m_fMin->setValue(i.fMinMHz); m_fMax->setValue(i.fMaxMHz); m_xl->setValue(i.xlFactor); m_target->setValue(i.chokeTarget); m_cm->setValue(i.cmPercent);
    m_cs->setCurrentIndex(qMax(0, m_cs->findData(i.csMode))); m_csPF->setValue(i.csPF); m_lead->setValueMM(i.leadMM);
    m_internal = false;
    Project *pr = Project::instance();
    if (presetValues || !pr->has(Key::Power)) m_proj->setValues(i.powerW, i.zLow, i.vswr, i.duty, i.dutyPercent, pr->num(Key::ZEnd, 3000), i.ambientC, true);
    else m_proj->setValues(pr->num(Key::Power, i.powerW), pr->num(Key::ZSys, i.zLow), pr->num(Key::Vswr, i.vswr), pr->get(Key::Duty, i.duty).toString(),
                           pr->num(Key::DutyPct, i.dutyPercent), pr->num(Key::ZEnd, 3000), pr->num(Key::Ambient, i.ambientC), false);
    recalc();
}

void BalunDialog::applyPreset(const QString &id, const QString &from)
{
    const Preset *p = presetById(id); if (!p) return;
    m_preset->setCurrentIndex(m_preset->findData(id));
    setInput(p->input, true);
    m_preset->setCurrentIndex(m_preset->findData(id));
    if (from.isEmpty()) m_presetMark->clear(); else m_presetMark->setFrom(from);
    m_wireMark->clear(); m_core->marker()->clear();
    publishCore(); publishWire();
}

void BalunDialog::edited()
{
    if (m_internal) return;
    m_preset->setCurrentIndex(m_preset->findData("custom")); m_presetMark->clear();
    recalc();
}

void BalunDialog::publishCore()
{
    if (!isVisible()) return;   // only user-visible choices are shared (not the defaults loaded while constructing)
    Project *p = Project::instance(); p->set(Key::Stack, m_stack->value(), window()); p->set(Key::Core, m_core->coreId(), window());
}
void BalunDialog::publishWire()
{
    if (!isVisible()) return;
    if (const Wire *w = conductorById(m_cond->currentData().toString())) if (w->kind != "coax") Project::instance()->set(Key::Wire, wireInfo(*w), window());
}

void BalunDialog::projectChanged(const QString &key, const QString &source)
{
    if (source == window()) return;
    bool live = ke9esuSettings()->value("KE9ESU/link/live", true).toBool();
    if (!live) return;
    if (key == Key::Core) pullKey(key);
    else if (key == Key::Wire) pullKey(key);
}

void BalunDialog::pullKey(const QString &key)
{
    Project *p = Project::instance();
    if (key == Key::Core) {
        QString id = p->get(Key::Core).toString(); const Core *c = coreById(id); if (!c || c->id != id) return;
        m_internal = true; m_core->setCoreId(id); if (p->has(Key::Stack) && p->source(Key::Stack) == p->source(Key::Core)) m_stack->setValue(p->get(Key::Stack).toInt()); m_internal = false;
        if (m_core->coreId() == id) { m_core->marker()->setFrom(p->source(key)); m_turns->setValue(0); m_preset->setCurrentIndex(m_preset->findData("custom")); recalc(); }
    } else if (key == Key::Wire) {
        QVariantMap w = p->get(Key::Wire).toMap(); double d = w.value("diaMM").toDouble(); if (!(d > 0)) return;
        QString id = conductorById(w.value("id").toString()) && conductorById(w.value("id").toString())->id == w.value("id").toString() ? w.value("id").toString() : nearestWire(conductors(), d, w.value("insul").toString());
        int i = m_cond->findData(id); if (i < 0) return;
        m_internal = true; m_cond->setCurrentIndex(i); m_internal = false; m_wireMark->setFrom(p->source(key)); m_preset->setCurrentIndex(m_preset->findData("custom")); recalc();
    } else if (key == Key::Recommend) {
        applyPreset(p->get(key).toString(), p->source(key));
    } else if (key == Key::ProblemBands) {
        QStringList bl = p->get(key).toStringList(); double lo = 1e9, hi = 0;
        for (const Band &b : bands()) if (bl.contains(b.id)) { lo = std::min(lo, b.lo); hi = std::max(hi, b.hi); }
        if (hi > 0) { m_internal = true; m_fMin->setValue(lo); m_fMax->setValue(hi); m_internal = false; m_turns->setValue(0); edited(); }
    }
}

void BalunDialog::recalc()
{
    applyCoreUnits();
    BalunInput in = input();
    m_d = design(in);
    const DesignResult &d = m_d; const BalunInput &inp = d.input; const Topology &top = d.topology; bool isChoke = top.kind == "choke";
    m_xfOnly->setVisible(!isChoke); m_chokeOnly->setVisible(isChoke); m_csPF->setEnabled(inp.csMode == "manual");
    QStringList bl; for (const Band &b : bandsInRange(inp.fMinMHz, inp.fMaxMHz)) bl << b.id;
    m_bandsHint->setText(bl.isEmpty() ? tr("No amateur bands in this range.") : tr("Bands covered: ") + bl.join(", "));
    m_hint->setText(d.nMin ? S("Auto minimum: %1").arg(d.nMin) + (isChoke ? S(" turns for |Z| ≥ ") + fmtOhm(d.target) : S(" turns for XL ≥ ") + rnd(d.target) + S(" Ω")) + " at " + jsNum(inp.fMinMHz) + " MHz."
                           : S("Target not reachable within 60 turns on this core — choose a bigger core, more cores, or a higher-µ mix."));
    m_title->setText("<span style='font-size:large;font-weight:600'>" + esc(top.name + S(" — ") + d.core->id + (inp.stack > 1 ? S(" ×") + jsNum(inp.stack) : QString()) + (top.coreSets > 1 ? S(" (×2 lines)") : QString())) + "</span>");
    QList<QPair<QString, QString>> cards;
    QString turnsTxt = top.turnsRatio ? QString("%1 : %2").arg(d.n1).arg(d.n2) : QString::number(d.n1) + (top.wires > 1 && !(isChoke && d.conductor->kind == "coax") ? (top.wires == 3 ? " trifilar" : " bifilar") : "");
    cards << qMakePair(QString("Turns"), turnsTxt);
    cards << qMakePair(QString(top.turnsRatio ? "Inductance (tap section)" : "Inductance (primary)"), f(d.lNom, d.lNom < 10 ? 2 : 1) + S(" µH"));
    if (!std::isnan(d.lFullUH)) cards << qMakePair(QString("Inductance (full winding)"), f(d.lFullUH, d.lFullUH < 10 ? 2 : 1) + S(" µH"));
    if (top.turnsRatio) cards << qMakePair(QString("High-side voltage"), rnd(d.vHighRms) + " V RMS (" + rnd(d.vHighRms * SQRT2) + " pk)");
    if (top.id == "auto91" && !d.wires.empty()) cards << qMakePair(QString("Wire (one piece)"), lenDual(d.wires[0].lengthMM));
    cards << qMakePair(isChoke ? "|Z| at " + jsNum(inp.fMinMHz) + " MHz" : "XL at " + jsNum(inp.fMinMHz) + " MHz", isChoke ? fmtOhm(d.zAtFmin.z) : rnd(d.zAtFmin.x) + S(" Ω (") + f(d.zAtFmin.x / inp.zLow) + S("×)"));
    cards << qMakePair(QString("Worst core loss (avg)"), f(d.lossAvgW, 2) + " W");
    cards << qMakePair(QString("Loss density"), rnd(d.lossDensity_mWcm3) + S(" mW/cm³"));
    cards << qMakePair(QString("Core temperature"), S("≈ ") + rnd(d.coreTempC) + S(" °C (+") + rnd(d.tempRiseC) + ")");
    if (!isChoke) cards << qMakePair(QString("Core efficiency (matched)"), f(d.efficiencyPct) + " %");
    if (d.hasCompCap) cards << qMakePair(QString("Compensation cap"), jsNum(d.compCap.pF) + " pF / " + jsNum(d.compCap.ratingV) + " V");
    m_cards->setCards(cards);
    m_checks->setChecks(d.checks);
    m_capNote->setText(d.hasCompCap ? S("<b>Compensation capacitor:</b> ") + jsNum(d.compCap.pF) + " pF (calc " + f(d.compCap.pFcalc, 0) + S(" pF; adjust ") + jsNum(d.compCap.rangePF[0]) + S("–") + jsNum(d.compCap.rangePF[1]) +
                                         S(" pF on the analyser), rated ≥ ") + jsNum(d.compCap.ratingV) + S(" V (input peak ≈ ") + rnd(d.compCap.vPeak) + S(" V at worst-case SWR, ×5 margin). Place it across the SO-239 (centre to shell).") : QString());
    m_capNote->setVisible(d.hasCompCap);
    m_diag->setDesign(d); m_plot->setDesign(d);
    double srf = selfResonanceMHz(*d.core, d.n1, inp.stack, d.stray_pF);
    m_plotHint->setText(isChoke ? S("Solid = R (resistive part, what really chokes), dashed = |X|, bold = |Z|. Self-resonance ≈ ") + (std::isnan(srf) ? QString("none in 0.1–500 MHz") : f(srf, 1) + " MHz") + " with " + f(d.stray_pF, 2) + " pF stray C."
                                : S("Magnetizing impedance of the primary (no load). XL should be ≥ ") + jsNum(inp.xlFactor) + S("× ") + jsNum(inp.zLow) + S(" Ω at the lowest band (green line). R is the core-loss resistance."));
    // per-frequency
    m_pf->setColumnHidden(8, isChoke); m_pf->setRowCount((int)d.perFreq.size());
    int hot = 0, vhot = 0; double zmin = 1e12, zmax = 0, lmax = 0, lf = 0;
    for (int r = 0; r < (int)d.perFreq.size(); r++) {
        const PerFreq &p = d.perFreq[r];
        QStringList v = {f(p.fMHz, 3), fmtOhm(p.r), (p.x < 0 ? S("−") : QString()) + fmtOhm(std::fabs(p.x)), fmtOhm(p.z), f(p.bPk_mT, 2), f(p.bHeat_G, 1), f(p.heatLimit_G, 0), f(p.lossPeakW, 2), f(p.lossMatchedW, 2)};
        for (int c = 0; c < v.size(); c++) { QTableWidgetItem *it = cell(v[c]); if (p.bHeat_G > p.heatLimit_G) it->setBackground(QColor(255, 210, 200)); m_pf->setItem(r, c, it); }
        if (p.bHeat_G > 2 * p.heatLimit_G) vhot++; else if (p.bHeat_G > p.heatLimit_G) hot++;
        zmin = std::min(zmin, p.z); zmax = std::max(zmax, p.z); if (p.lossPeakW > lmax) { lmax = p.lossPeakW; lf = p.fMHz; }
    }
    int ok = (int)d.perFreq.size() - hot - vhot, nb = (int)bandsInRange(inp.fMinMHz, inp.fMaxMHz).size();
    auto pill = [](const QString &sev, const QString &t, int n) { return QString(" <span style='color:%1;%2'>[%3]</span>").arg(sevColor(sev).name(), n ? "font-weight:bold" : "color:" + theme().muted.name(), esc(t)); };
    m_rPf->setSummary(QString("%1 frequencies, %2 bands · |Z| %3–%4 · worst key-down loss %5 W @ %6 MHz").arg(d.perFreq.size()).arg(nb).arg(esc(fmtOhm(zmin)), esc(fmtOhm(zmax)), f(lmax, 2), f(lf, 3))
                      + pill("fail", S("FAIL %1 (B > 2× limit)").arg(vhot), vhot) + pill("warn", QString("CHECK %1 (B > heating limit)").arg(hot), hot) + pill("ok", QString("OK %1").arg(ok), ok));
    m_parts->setRowCount((int)d.parts.size()); double q = 0;
    for (int r = 0; r < (int)d.parts.size(); r++) { m_parts->setItem(r, 0, cell(jsNum(d.parts[r].qty))); m_parts->setItem(r, 1, cell(d.parts[r].item, false)); q += d.parts[r].qty; }
    m_rParts->setSummary(QString("%1 line items, %2 pieces — %3").arg(d.parts.size()).arg(jsNum(q), esc(d.parts.empty() ? QString() : d.parts[0].item.split(QRegExp("[:(]"))[0])));
    m_steps->setText(stepsHtml(d.steps)); m_rSteps->setSummary(QString("%1 steps").arg(d.steps.size()));
    Project::instance()->setState(window(), saveState());
    ke9esuSettings()->setValue("KE9ESU/state/" + window(), QJsonDocument(saveState()).toJson(QJsonDocument::Compact));
}

QJsonObject BalunDialog::saveState() const
{
    QJsonObject o; o.insert("input", toJson(input())); o.insert("preset", m_preset->currentData().toString()); o.insert("db", m_core->db());
    return o;
}
void BalunDialog::loadState(const QJsonObject &o)
{
    if (o.isEmpty()) return;
    m_core->setDb(o.value("db").toString("fr"));
    setInput(balunInputFromJson(o.value("input").toObject()));
    QString p = o.value("preset").toString("custom"); m_preset->setCurrentIndex(qMax(0, m_preset->findData(p)));
}

void BalunDialog::fillPull(QMenu *m)
{
    addPull(m, Key::Core, tr("Core") + ": " + Project::instance()->get(Key::Core).toString());
    addPull(m, Key::Wire, tr("Wire") + ": " + Project::instance()->get(Key::Wire).toMap().value("label").toString());
    addPull(m, Key::Recommend, tr("Recommended transformer preset") + ": " + Project::instance()->get(Key::Recommend).toString());
    addPull(m, Key::ProblemBands, tr("Frequency range of the long-wire bands") + ": " + Project::instance()->get(Key::ProblemBands).toStringList().join(" "));
}

void BalunDialog::fillUseIn(QMenu *m)
{
    const DesignResult &d = m_d; Project *p = Project::instance(); QString w = window();
    QString what = d.topology.turnsRatio ? tr("tap-section") : tr("primary");
    m->addAction(S("Inductance %1 = %2 µH → Coil64 ferrite toroid (core preloaded)").arg(what, f(d.lNom, 2)), this, [this, p, w] {
        p->set(Key::L, m_d.lNom, w); p->requestCoil64("inductance", {{"L", m_d.lNom}, {"kind", "ferrite"}, {"coreId", m_d.core->id}, {"stack", m_d.input.stack}, {"N", m_d.n1}, {"wireDiaMM", wireDiaMM(*m_d.conductor)}}); });
    if (!std::isnan(d.lFullUH)) m->addAction(S("Full-winding inductance %1 µH → Coil64 ferrite toroid").arg(f(d.lFullUH, 1)), this, [this, p, w] {
        p->set(Key::L, m_d.lFullUH, w); p->requestCoil64("inductance", {{"L", m_d.lFullUH}, {"kind", "ferrite"}, {"coreId", m_d.core->id}, {"stack", m_d.input.stack}, {"N", m_d.n2}, {"wireDiaMM", wireDiaMM(*m_d.conductor)}}); });
    m->addAction(S("Inductance %1 µH → Find standard-part combination").arg(f(d.lNom, 2)), this, [this, p, w] {
        p->set(Key::CombReq, QVariantMap{{"type", "L"}, {"value", m_d.lNom * 1e-6}, {"label", "Balun inductance"}}, w); p->requestWindow(Win::Combiner, Key::CombReq); });
    m->addSeparator();
    m->addAction(tr("Core %1 → Core database").arg(d.core->id), this, [this, p] { publishCore(); p->requestWindow(Win::CoreDb, Key::Core); });
    m->addAction(tr("Core %1 → LC trap (toroid)").arg(d.core->id), this, [this, p] { publishCore(); p->requestWindow(Win::Lc, Key::Core); });
    m->addAction(tr("Core %1 → Coil64 ferrite toroid calculator").arg(d.core->id), this, [this, p] { publishCore(); p->requestCoil64("ferriteCore", {{"coreId", m_d.core->id}, {"stack", m_d.input.stack}}); });
    if (d.conductor->kind != "coax") m->addAction(tr("Wire %1 → Coil64 coil calculators and RF windows").arg(wireLabel(*d.conductor)), this, [this, p] {
        publishWire(); p->requestCoil64("wire", p->get(Key::Wire).toMap()); });
    if (d.hasCompCap) {
        m->addSeparator();
        m->addAction(S("Compensation cap %1 pF → Coil64 LC circuit (with L full)").arg(jsNum(d.compCap.pF)), this, [this, p, w] {
            p->set(Key::C, m_d.compCap.pF, w); p->requestCoil64("lc", {{"C", m_d.compCap.pF}, {"L", std::isnan(m_d.lFullUH) ? m_d.lNom : m_d.lFullUH}}); });
        m->addAction(S("Compensation cap %1 pF → Find standard-part combination").arg(jsNum(d.compCap.pF)), this, [this, p, w] {
            p->set(Key::CombReq, QVariantMap{{"type", "C"}, {"value", m_d.compCap.pF * 1e-12}, {"label", "Compensation capacitor"}}, w); p->requestWindow(Win::Combiner, Key::CombReq); });
    }
}

void BalunDialog::acceptToCoil64()
{
    Project *p = Project::instance(); p->set(Key::L, m_d.lNom, window());
    p->requestCoil64("inductance", {{"L", m_d.lNom}, {"kind", "ferrite"}, {"coreId", m_d.core->id}, {"stack", m_d.input.stack}, {"N", m_d.n1}, {"wireDiaMM", wireDiaMM(*m_d.conductor)}});
}

QString BalunDialog::sheetTitle() const { return "KE9ESU " + m_d.topology.name + " " + m_d.core->id; }
QMap<QString, QImage> BalunDialog::sheetImages() const { return {{"diagram", renderWinding(m_d)}, {"plot", renderImpedance(m_d, 900, 330)}}; }
QString BalunDialog::sheetHtml() const
{
    const DesignResult &d = m_d; const BalunInput &inp = d.input; const Core &c = *d.core;
    QStringList bl; for (const Band &b : bandsInRange(inp.fMinMHz, inp.fMaxMHz)) bl << b.id;
    QList<QPair<QString, QString>> rows = {
        {"Device", d.topology.name},
        {"Core", c.id + S(" ×") + jsNum(inp.stack * d.topology.coreSets) + S(" — ") + c.db + " " + QString(c.pn).remove(QRegExp("^Amidon ")) + ", " + dimsTxt(c) + ", AL " + jsNum(c.al) + S(" nH/T²") +
                     (c.hasAlAmidon && c.alAmidonUnit.contains("uH") ? S(" (= ") + jsNum(c.alAmidonValue) + S(" µH/100T)") : QString()) + " (+" + jsNum(c.alTol[0]) + S("/−") + jsNum(c.alTol[1]) + "%)"},
        {"Turns", d.topology.turnsRatio ? QString("%1 primary : %2 total (tap at turn %1)").arg(d.n1).arg(d.n2) : QString("%1 turns").arg(d.n1)},
        {"Conductor", wireLabel(*d.conductor)},
        {"Power / mode", jsNum(inp.powerW) + " W, " + inp.duty + " (" + jsNum(inp.dutyPercent) + "% duty), worst SWR " + jsNum(inp.vswr) + ":1"},
        {"Frequency", jsNum(inp.fMinMHz) + S("–") + jsNum(inp.fMaxMHz) + " MHz (" + bl.join(" ") + ")"},
        {"Inductance", f(d.lNom, 2) + S(" µH (") + f(d.lMin, 2) + S("–") + f(d.lMax, 2) + " with AL tolerance)" + (!std::isnan(d.lFullUH) ? "; full winding " + f(d.lFullUH, 1) + S(" µH; high side ≈ ") + rnd(d.vHighRms) + " V RMS" : QString())},
        {"Loss / temp", f(d.lossAvgW, 2) + " W avg, " + rnd(d.lossDensity_mWcm3) + S(" mW/cm³, core ≈ ") + rnd(d.coreTempC) + S(" °C")}};
    if (d.hasCompCap) rows << qMakePair(QString("Comp. capacitor"), jsNum(d.compCap.pF) + S(" pF, ≥ ") + jsNum(d.compCap.ratingV) + " V, across SO-239 (trim " + jsNum(d.compCap.rangePF[0]) + S("–") + jsNum(d.compCap.rangePF[1]) + " pF)");
    QString h = "<html><head>" + sheetCss() + "</head><body><h1>" + esc(S("KE9ESU build sheet — ") + d.topology.name) + "</h1><div class='hint'>" + esc(QDate::currentDate().toString(Qt::ISODate)) +
                S(" · Coil64 KE9ESU — RF transformers &amp; traps · estimates only — confirm with an antenna analyser</div>");
    h += "<h2>Specification</h2><table class='t' cellspacing='0'>"; for (auto &r : rows) h += "<tr><th>" + esc(r.first) + "</th><td>" + esc(r.second) + "</td></tr>"; h += "</table>";
    h += "<h2>Parts &amp; cut lengths</h2>" + partsTableHtml(d.parts);
    h += "<h2>Winding</h2><img src='ke9esu://diagram' width='420'>";
    h += "<h2>Connections</h2>" + stepsHtml(d.steps);
    h += "<h2>Checks</h2>" + ChecksBar::sheetHtml(d.checks);
    h += "<h2>Impedance</h2><img src='ke9esu://plot' width='640'>";
    h += "<h2>Per-frequency detail</h2><table class='t' cellspacing='0'><tr><th>MHz</th><th>R</th><th>X</th><th>|Z|</th><th>B peak (mT)</th><th>B heat (G)</th><th>limit (G)</th><th>loss key-down (W)</th>" + QString(d.topology.kind == "choke" ? "" : "<th>loss matched (W)</th>") + "</tr>";
    for (const PerFreq &p : d.perFreq) h += "<tr><td class='n'>" + f(p.fMHz, 3) + "</td><td class='n'>" + esc(fmtOhm(p.r)) + "</td><td class='n'>" + esc((p.x < 0 ? S("−") : QString()) + fmtOhm(std::fabs(p.x))) + "</td><td class='n'>" + esc(fmtOhm(p.z)) +
                                            "</td><td class='n'>" + f(p.bPk_mT, 2) + "</td><td class='n'>" + f(p.bHeat_G, 1) + "</td><td class='n'>" + f(p.heatLimit_G, 0) + "</td><td class='n'>" + f(p.lossPeakW, 2) + "</td>" + (d.topology.kind == "choke" ? QString() : "<td class='n'>" + f(p.lossMatchedW, 2) + "</td>") + "</tr>";
    h += "</table><p class='hint'>Reminders: PTFE/FEP insulation on high-voltage turns; keep start and finish apart; weatherproof enclosure; test on an analyser into a dummy load (transformers) before full power. "
         "Fair-Rite, Amidon and Micrometals are trademarks of their owners.</p></body></html>";
    return h;
}

} // namespace ke9esu
