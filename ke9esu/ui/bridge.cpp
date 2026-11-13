/* bridge.cpp - see bridge.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "bridge.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "project.h"
#include "widgets.h"
#include "balundialog.h"
#include "diagram.h"
#include "eflwdialog.h"
#include "trapdialogs.h"
#include "coredbdialog.h"
#include "combinerdialog.h"
#include "theme.h"
#include <QShortcut>
#include <memory>
#include <QSet>
#include "options.h"
#include "rf_toroid.h"
#include "viewprefs.h"
#include <QActionGroup>
#include <QFontDatabase>
#include <QFontInfo>
#include "balun_core.h"
#include "trap_core.h"
#include <QMenuBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QStatusBar>
#include <QApplication>
#include <QJsonDocument>
#include <QDir>
#include <QTimer>
#include <QScreen>
#include <QRegularExpression>
#include <cmath>

namespace ke9esu {

static const char *const kSession = "KE9ESU/session";

Bridge::Bridge(MainWindow *mw) : QObject(mw), m(mw)
{
    QString err; initData(&err);
    Project *p = Project::instance();
    // ---- menu "RF transformers & traps" (before Settings)
    QMenu *rf = new QMenu(tr("RF &transformers && traps"), m);
    struct It { const char *win; QString text; QString tip; };
    const QList<It> items = {
        {Win::Balun, tr("&Balun / Unun designer…"), tr("1:1, 4:1 Guanella/Ruthroff, 9:1 Ruthroff, 9:1 autotransformer, 49:1 / 64:1 with compensation capacitor, common-mode choke")},
        {Win::Eflw, tr("&Long-wire analyzer…"), tr("End-fed long wire: band-by-band impedance tendency, avoid zones, recommended transformer and trap suggestions")},
        {Win::Coax, tr("&Coax trap…"), tr("Coax-wound trap: turns, form, resonance, voltage and power checks")},
        {Win::Lc, tr("L&C trap…"), tr("Discrete LC trap: coil + capacitor, Q, SRF, voltage and current checks")},
        {Win::CoreDb, tr("Core &database…"), tr("Fair-Rite, Micrometals and Amidon cores with AL cross-checks")},
        {Win::Combiner, tr("Find &standard-part combination…"), tr("Series / parallel networks of E6…E192 or your own parts for a needed R, C or L")}};
    for (int i = 0; i < items.size(); i++) {
        if (i == 5) rf->addSeparator();
        QString w = items[i].win; QAction *a = rf->addAction(items[i].text, this, [this, w] { openWindow(w); }); a->setStatusTip(items[i].tip);
    }
    rf->addSeparator();
    QMenu *pre = rf->addMenu(tr("KE9ESU &presets"));
    for (const Preset &pr : presets()) if (pr.id.startsWith("ke9esu")) { QString id = pr.id;
        pre->addAction(pr.label, this, [this, id] { BalunDialog *b = qobject_cast<BalunDialog *>(openWindow(Win::Balun)); if (b) b->applyPreset(id); }); }
    rf->addSeparator();
    m_live = rf->addAction(tr("Link core && wire selection live between windows")); m_live->setCheckable(true);
    m_live->setChecked(ke9esuSettings()->value("KE9ESU/link/live", true).toBool());
    connect(m_live, &QAction::toggled, this, [](bool on) { ke9esuSettings()->setValue("KE9ESU/link/live", on); });
    m_capture = rf->addAction(tr("Share Coil64 results with the RF windows")); m_capture->setCheckable(true);
    m_capture->setChecked(ke9esuSettings()->value("KE9ESU/link/capture", true).toBool());
    m_capture->setStatusTip(tr("LC circuit L/C/f, one-layer coil and ferrite toroid results become available in the RF windows (Pull value…)"));
    connect(m_capture, &QAction::toggled, this, [](bool on) { ke9esuSettings()->setValue("KE9ESU/link/capture", on); });
    rf->addSeparator();
    rf->addAction(tr("About these tools…"), this, [this] {
        QMessageBox::about(m, tr("RF transformers & traps"), tr(
            "<b>KE9ESU fork of Coil64</b><br>Coil64 © Valery Kustarev (GPL-3.0).<br><br>"
            "Balun / unun, trap, long-wire and core database: C++ port of the KE9ESU Balun, Unun &amp; Trap Designer, "
            "which derives from <i>balun-unun-designer</i> by Volodymyr Kopytsia (hermes98761234, MIT licence; see docs/THIRD_PARTY-balun-unun-designer-MIT.txt).<br>"
            "Standard-part combination: port of the KE9ESU Component Combiner.<br><br>"
            "Results are engineering estimates. Verify with an analyzer before full power."));
    });
    m->mui->menuBar->insertMenu(m->mui->menuCustomize->menuAction(), rf);
    buildViewMenu(rf);

    // ---- File menu: RF project
    QAction *before = m->mui->actionPrint;
    QAction *op = new QAction(tr("Open RF project…"), m); QAction *sv = new QAction(tr("Save RF project"), m); QAction *sa = new QAction(tr("Save RF project as…"), m);
    QAction *sep = new QAction(m); sep->setSeparator(true); QAction *sep2 = new QAction(m); sep2->setSeparator(true);
    m->mui->menuFile->insertActions(before, {sep, op, sv, sa, sep2});
    connect(op, &QAction::triggered, this, &Bridge::openProject);
    connect(sv, &QAction::triggered, this, [this] { saveProject(false); });
    connect(sa, &QAction::triggered, this, [this] { saveProject(true); });

    connect(p, &Project::windowRequested, this, [this](const QString &w, const QString &k) { openWindow(w, k); });
    connect(p, &Project::coil64Requested, this, &Bridge::coil64Request);
    connect(p, &Project::changed, this, &Bridge::projectChanged);
    // restore the previous session's shared values (dialog inputs restore themselves)
    QJsonObject s = QJsonDocument::fromJson(ke9esuSettings()->value(kSession).toByteArray()).object();
    if (!s.isEmpty()) p->fromJson(s);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [] { Project *p = Project::instance(); emit p->aboutToSave();
        ke9esuSettings()->setValue(kSession, QJsonDocument(p->toJson()).toJson(QJsonDocument::Compact)); ke9esuSettings()->sync(); });
    connect(m, &MainWindow::sendOpt, this, [this](_OptionStruct) { optionsChanged(); });
    optionsChanged();
    QString shots = qEnvironmentVariable("KE9ESU_SHOTS");
    if (!shots.isEmpty()) QTimer::singleShot(300, this, [this, shots] { screenshots(shots); });
}

QStringList Bridge::windowNames() { return {Win::Balun, Win::Eflw, Win::Coax, Win::Lc, Win::CoreDb, Win::Combiner}; }

void Bridge::buildViewMenu(QMenu *before)
{
    // Theme: Coil64's three actions + KE9ESU Light / Dark / System, one radio group, shared by View and Customize > Theme
    QMenu *tm = m->mui->menuTheme; QActionGroup *g = new QActionGroup(this); g->setExclusive(true);
    for (QAction *a : {m->mui->actionThemeDefault, m->mui->actionThemeSunny, m->mui->actionThemeDark}) { a->setCheckable(true); g->addAction(a); }
    tm->addSeparator();
    for (int s : {StyleKe9esuLight, StyleKe9esuDark, StyleKe9esuSystem}) {
        QAction *a = tm->addAction(themeName(s)); a->setCheckable(true); a->setData(s); g->addAction(a); m_themeActs << a;
        connect(a, &QAction::triggered, this, [this, s] { m->applyGuiStyle(s); });
    }
    QMenu *view = new QMenu(tr("&View"), m);
    view->addMenu(tm);
    view->addSeparator();
    // shortcuts are separate QShortcuts and shown in the item text: with the KE9ESU style sheet Qt's shortcut column can
    // overlap the text of menus created at run time
    view->addAction(tr("A−  Smaller text      Ctrl −"), this, [this] { stepFont(-1); });
    view->addAction(tr("A+  Larger text       Ctrl +"), this, [this] { stepFont(+1); });
    view->addAction(tr("Reset text size       Ctrl 0"), this, [this] { stepFont(0); });
    // one QShortcut per distinct sequence (ZoomOut is Ctrl+- on most platforms; duplicates would be ambiguous and never fire)
    auto seen = std::make_shared<QSet<QString>>();
    auto sc = [this, seen](const QKeySequence &k, int dir) { if (k.isEmpty() || seen->contains(k.toString())) return; seen->insert(k.toString());
        QShortcut *s = new QShortcut(k, m); s->setContext(Qt::ApplicationShortcut);
        connect(s, &QShortcut::activated, this, [this, dir] { stepFont(dir); }); };
    sc(QKeySequence::ZoomOut, -1); sc(QKeySequence("Ctrl+-"), -1); sc(QKeySequence::ZoomIn, +1); sc(QKeySequence("Ctrl+="), +1); sc(QKeySequence("Ctrl++"), +1); sc(QKeySequence("Ctrl+0"), 0);
    m->mui->menuBar->insertMenu(before->menuAction(), view);
    // KE9ESU System follows the OS light/dark switch while running
    m_sysDark = systemPrefersDark();
    m_sysPoll = new QTimer(this); m_sysPoll->setInterval(4000);
    connect(m_sysPoll, &QTimer::timeout, this, [this] { bool d = systemPrefersDark(); if (d != m_sysDark) { m_sysDark = d; if (m->myOpt->styleGUI == StyleKe9esuSystem) m->applyGuiStyle(StyleKe9esuSystem); } });
    m_sysPoll->start();
    connect(ThemeNotifier::instance(), &ThemeNotifier::requested, this, [this](int code) {
        if (code == 2) m->applyGuiStyle(themeIsDark() ? StyleKe9esuLight : StyleKe9esuDark); else stepFont(code); });
    themeApplied();
}

void Bridge::themeApplied()
{
    for (QAction *a : m_themeActs) a->setChecked(a->data().toInt() == m->myOpt->styleGUI);
    QSettings *s = ke9esuSettings(); s->beginGroup("GUI");
    ViewPrefs vp; vp.style = m->myOpt->styleGUI; vp.mainPx = m->myOpt->mainFontSize; vp.textPx = m->myOpt->textFontSize; saveViewPrefs(*s, vp);
    s->endGroup();
}

void Bridge::stepFont(int dir)
{
    _OptionStruct *o = m->myOpt;
    if (dir == 0) { int def = clampFontPx(QFontInfo(QFontDatabase::systemFont(QFontDatabase::GeneralFont)).pixelSize()); o->mainFontSize = def; o->textFontSize = def; }
    else { o->mainFontSize = stepFontPx(o->mainFontSize, dir); o->textFontSize = stepFontPx(o->textFontSize, dir); }
    m->resetUiFont();
    emit m->sendOpt(*o);
    themeApplied();
    ThemeNotifier::instance()->notify();
    status(tr("Text size %1 px (A−  Ctrl+−,  A+  Ctrl++,  reset Ctrl+0)").arg(o->mainFontSize));
}

Ke9esuDialog *Bridge::openWindow(const QString &name, const QString &pullKey)
{
    if (name == Win::Coil64 || name.startsWith("Coil64 ")) { m->show(); m->raise(); m->activateWindow(); return nullptr; }
    Ke9esuDialog *d = m_wins.value(name);
    if (!d) {
        if (name == Win::Balun) d = new BalunDialog(m);
        else if (name == Win::Eflw) d = new EflwDialog(m);
        else if (name == Win::Coax) d = new CoaxTrapDialog(m);
        else if (name == Win::Lc) d = new LcTrapDialog(m);
        else if (name == Win::CoreDb) d = new CoreDbDialog(m);
        else if (name == Win::Combiner) d = new CombinerDialog(m);
        else return nullptr;
        d->setAttribute(Qt::WA_DeleteOnClose, false);
        m_wins.insert(name, d);
    }
    d->show(); d->raise(); d->activateWindow();
    if (!pullKey.isEmpty()) d->pullKey(pullKey);
    return d;
}

void Bridge::optionsChanged()
{
    const _OptionStruct *o = m->myOpt; Project::Units u;
    u.awg = o->isAWG; u.len = o->ssLengthMeasureUnit; u.lenMul = o->dwLengthMultiplier > 0 ? o->dwLengthMultiplier : 1;
    u.indUnit = qApp->translate("Context", o->ssInductanceMeasureUnit.toUtf8()); u.indMul = o->dwInductanceMultiplier > 0 ? o->dwInductanceMultiplier : 1;
    u.capUnit = qApp->translate("Context", o->ssCapacityMeasureUnit.toUtf8()); u.capMul = o->dwCapacityMultiplier > 0 ? o->dwCapacityMultiplier : 1;
    Project::instance()->setUnits(u);
}

void Bridge::status(const QString &s) { m->statusBar()->showMessage(s, 8000); }

void Bridge::refreshView(int tab, int row)
{
    m_applying = true;
    if (tab >= 0 && m->mui->tabWidget->currentIndex() != tab) m->mui->tabWidget->setCurrentIndex(tab);   // fires on_tabWidget_currentChanged
    if (row >= 0) m->FormCoil = (_FormCoil)row;
    m->on_tabWidget_currentChanged(m->mui->tabWidget->currentIndex());
    m_applying = false;
}

void Bridge::applyFerrite(const QString &coreId, int stack)
{
    const Core *c = coreById(coreId); if (!c || c->od <= 0) return;
    stack = qMax(1, stack); Geometry g = geometry(c->od, c->idia, c->h);
    m->data->Do = c->od; m->data->Di = c->idia; m->data->h = c->h * stack;
    if (g.l0 > 0 && c->al > 0) m->data->mu = c->al / g.l0;    // AL of the stack / l0 of the stack = AL / l0 of one core
    m->data->Ch = 0;
}

void Bridge::applyWire(const QVariantMap &w)
{
    double d = w.value("diaMM").toDouble(), od = w.value("odMM").toDouble();
    if (d > 0) m->data->d = d;
    if (od > 0) m->data->k = od;
}

bool Bridge::ferriteShowing(int tab) const { return (tab == 0 || tab == 1) && m->FormCoil == _FerrToroid; }

void Bridge::coil64Request(const QString &what, const QVariantMap &a)
{
    Project *p = Project::instance(); QString from = a.value("from").toString();
    // commit whatever is being typed in Coil64 first (editingFinished on focus loss), so it cannot overwrite the new values
    if (QWidget *fw = QApplication::focusWidget()) if (m->isAncestorOf(fw)) fw->clearFocus();
    if (what == "inductance") {
        double L = a.value("L").toDouble(); if (L > 0) m->data->inductance = L;
        QString kind = a.value("kind").toString();
        if (kind == "ferrite") {
            applyFerrite(a.value("coreId", p->get(Key::Core)).toString(), a.value("stack", 1).toInt());
            if (a.value("wireDiaMM").toDouble() > 0) m->data->d = a.value("wireDiaMM").toDouble();
            refreshView(0, _FerrToroid);
        } else if (kind == "air") {
            if (a.value("D").toDouble() > 0) m->data->D = a.value("D").toDouble();
            if (a.value("wireDiaMM").toDouble() > 0) m->data->d = a.value("wireDiaMM").toDouble();
            if (a.value("wireOdMM").toDouble() > 0) m->data->k = a.value("wireOdMM").toDouble();
            if (a.value("pitchMM").toDouble() > 0) m->data->p = a.value("pitchMM").toDouble();
            if (a.value("f").toDouble() > 0) m->data->frequency = a.value("f").toDouble();
            bool cw = m->data->p > 0 && m->data->k > 0 && m->data->p <= m->data->k * 1.0001;
            refreshView(0, cw ? _Onelayer_cw : _Onelayer);
        } else {
            int tab = m->mui->tabWidget->currentIndex();
            if (tab == 2) { m->mui->radioButton_LC->setChecked(true); refreshView(2, -1); } else refreshView(0, -1);
        }
        status(tr("Inductance %1 µH%2 loaded into Coil64").arg(f(L, 3), from.isEmpty() ? QString() : tr(" from %1").arg(from)));
    } else if (what == "ferriteCore") {
        applyFerrite(a.value("coreId").toString(), a.value("stack", 1).toInt());
        int tab = m->mui->tabWidget->currentIndex();
        if (a.value("switch").toBool()) refreshView(0, _FerrToroid); else if (ferriteShowing(tab)) refreshView(tab, -1);
        status(tr("Core %1 loaded into the Coil64 ferrite toroid calculator (µ = AL / l0, Ch = 0)").arg(a.value("coreId").toString()));
    } else if (what == "lc") {
        if (a.value("L").toDouble() > 0) m->data->inductance = a.value("L").toDouble();
        if (a.value("C").toDouble() > 0) m->data->capacitance = a.value("C").toDouble();
        m->mui->radioButton_LC->setChecked(true);
        refreshView(2, -1); m->on_radioButton_LC_clicked();
        if (m->data->inductance > 0 && m->data->capacitance > 0) m->on_pushButton_Calculate_clicked();
        status(tr("LC values loaded into the Coil64 LC circuit tab"));
    } else if (what == "wire") {
        applyWire(a);
        int tab = m->mui->tabWidget->currentIndex(); if (tab == 0 || tab == 1) refreshView(tab, -1);
        status(tr("Wire %1 loaded into Coil64").arg(a.value("label").toString()));
    }
    m->show(); m->raise(); m->activateWindow();
}

void Bridge::projectChanged(const QString &key, const QString &source)
{
    if (m_applying || source.startsWith("Coil64") || !m_live->isChecked()) return;
    Project *p = Project::instance(); int tab = m->mui->tabWidget->currentIndex();
    if (key == Key::Core) { applyFerrite(p->get(Key::Core).toString(), int(p->num(Key::Stack, 1)));
        if (ferriteShowing(tab) && !m->mui->tabWidget->hasFocus()) refreshView(tab, -1); }
    else if (key == Key::Wire) applyWire(p->get(Key::Wire).toMap());
}

static const Core *matchCore(double Do, double Di, double H, double mu, int *stackOut)
{
    const Core *best = nullptr; double bestE = 1e9; int bestS = 1;
    for (const Core &c : cores()) {
        if (c.od <= 0 || c.idia <= 0 || c.h <= 0) continue;
        double eo = std::fabs(std::log(c.od / Do)), ei = std::fabs(std::log(c.idia / Di)); if (eo > 0.03 || ei > 0.03) continue;
        int s = qMax(1, int(std::lround(H / c.h))); if (s > 4) continue; double eh = std::fabs(std::log(c.h * s / H)); if (eh > 0.04) continue;
        Geometry g = geometry(c.od, c.idia, c.h); double em = (mu > 0 && g.l0 > 0 && c.al > 0) ? std::fabs(std::log((c.al / g.l0) / mu)) : 0.5;
        double e = eo + ei + eh + 0.2 * em + (c.db == "amidon" ? 0.001 : 0);
        if (e < bestE) { bestE = e; best = &c; bestS = s; }
    }
    if (stackOut) *stackOut = bestS;
    return best;
}

void Bridge::captureOutput(const QString &resultHtml)
{
    if (!m_capture || !m_capture->isChecked() || resultHtml.contains("color:red")) return;
    Project *p = Project::instance(); const _Data *d = m->data; int tab = m->mui->tabWidget->currentIndex();
    m_applying = true;
    if (tab == 2) {
        if (d->inductance > 0) p->set(Key::L, d->inductance, Win::Coil64Lc);
        if (d->capacitance > 0) p->set(Key::C, d->capacitance, Win::Coil64Lc);
        if (d->frequency > 0) p->set(Key::F, d->frequency, Win::Coil64Lc);
    } else if ((tab == 0 || tab == 1) && (m->FormCoil == _Onelayer_cw || m->FormCoil == _Onelayer)) {
        bool cw = m->FormCoil == _Onelayer_cw;
        if (!(tab == 0 && cw && m->myOpt->isWindingLengthOneLayerInit) && d->D > 0 && d->d > 0) {
            QVariantMap c; c["N"] = d->N; c["D"] = d->D; c["d"] = d->d; c["k"] = d->k > 0 ? d->k : d->d; c["p"] = cw ? (d->k > 0 ? d->k : d->d) : d->p;
            c["L"] = d->inductance; c["f"] = d->frequency; c["cw"] = cw;
            p->set(Key::Coil, c, Win::Coil64Coil);
            if (d->inductance > 0) p->set(Key::L, d->inductance, Win::Coil64Coil);
        }
    } else if ((tab == 0 || tab == 1) && m->FormCoil == _FerrToroid) {
        int s = 1; const Core *c = matchCore(d->Do, d->Di, d->h, d->mu, &s);
        if (c) { p->set(Key::Stack, s, Win::Coil64Ferrite); p->set(Key::Core, c->id, Win::Coil64Ferrite); }
        if (d->inductance > 0) p->set(Key::L, d->inductance, Win::Coil64Ferrite);
    }
    m_applying = false;
}

void Bridge::openProject()
{
    QString dir = ke9esuSettings()->value("KE9ESU/projectDir", QDir::homePath()).toString();
    QString path = QFileDialog::getOpenFileName(m, tr("Open RF project"), dir, tr("RF project (*.rfproj.json *.json)"));
    if (path.isEmpty()) return;
    QString err; if (!Project::instance()->openFile(path, &err)) { QMessageBox::warning(m, tr("Open RF project"), err); return; }
    ke9esuSettings()->setValue("KE9ESU/projectDir", QFileInfo(path).absolutePath());
    for (const QString &w : Project::instance()->toJson().value("windows").toObject().keys()) openWindow(w);
    status(tr("Opened %1").arg(QDir::toNativeSeparators(path)));
}

void Bridge::saveProject(bool as)
{
    QString path = Project::instance()->filePath();
    if (as || path.isEmpty()) {
        QString dir = ke9esuSettings()->value("KE9ESU/projectDir", QDir::homePath()).toString();
        path = QFileDialog::getSaveFileName(m, tr("Save RF project"), dir + "/KE9ESU-project.rfproj.json", tr("RF project (*.rfproj.json *.json)"));
        if (path.isEmpty()) return;
    }
    QString err; if (!Project::instance()->saveFile(path, &err)) { QMessageBox::warning(m, tr("Save RF project"), err); return; }
    ke9esuSettings()->setValue("KE9ESU/projectDir", QFileInfo(path).absolutePath());
    status(tr("Saved %1").arg(QDir::toNativeSeparators(path)));
}

void Bridge::screenshots(const QString &dir)
{
    QDir().mkpath(dir);
    auto grab = [&](QWidget *w, const QString &name) { qApp->processEvents(); QApplication::processEvents(); w->grab().save(dir + "/" + name + ".png"); };
    m->resize(1280, 760); QApplication::processEvents();
    grab(m, "00_coil64_main");
    int i = 1;
    for (const QString &w : windowNames()) {
        Ke9esuDialog *d = openWindow(w);
        if (CombinerDialog *c = qobject_cast<CombinerDialog *>(d)) c->runSync();
        for (int k = 0; k < 5; k++) { QApplication::processEvents(); }
        QString fn = QString("%1_%2").arg(i++, 2, 10, QChar('0')).arg(QString(w).replace(QRegularExpression("[^A-Za-z0-9]+"), "_"));
        grab(d, fn);
        if (!d->exportSheetPdf(dir + "/" + fn + "_sheet.pdf")) qWarning("KE9ESU shots: build sheet PDF failed for %s", qPrintable(w));
        { const auto imgs = d->drawings(); if (!imgs.isEmpty()) QDir().mkpath(dir + "/drawings");
          for (auto it = imgs.constBegin(); it != imgs.constEnd(); ++it) it.value().save(dir + "/drawings/" + fn + "_" + it.key() + ".png"); }
        d->close();
    }
    // linked flows
    BalunDialog *b = qobject_cast<BalunDialog *>(openWindow(Win::Balun)); if (b) { b->applyPreset("ke9esu-auto91"); grab(b, "20_balun_auto91"); b->applyPreset("ke9esu-choke"); grab(b, "21_balun_choke"); b->applyPreset("ke9esu"); b->close(); }
    Project::instance()->requestCoil64("inductance", {{"L", 34.4}, {"kind", "ferrite"}, {"coreId", "FT240-43"}, {"stack", 2}, {"wireDiaMM", 1.29}, {"from", Win::Balun}});
    grab(m, "30_coil64_ferrite_from_balun");
    Project::instance()->requestCoil64("lc", {{"L", 2.0}, {"C", 100.0}, {"from", Win::Lc}});
    grab(m, "31_coil64_lc_from_trap");
    qInfo("KE9ESU shots: LC fields L=%s C=%s", qPrintable(m->mui->lineEdit_1_3->text()), qPrintable(m->mui->lineEdit_2_3->text()));
    { Project *p = Project::instance(); qInfo("KE9ESU shots: project f=%g MHz from '%s', L=%g from '%s'", p->num(Key::F), qPrintable(p->source(Key::F)), p->num(Key::L), qPrintable(p->source(Key::L)));
      Ke9esuDialog *lc = openWindow(Win::Lc, Key::F); grab(lc, "33_lc_trap_f_from_coil64"); lc->close();
      p->set(Key::CombReq, QVariantMap{{"type", "C"}, {"value", 4.7e-11}, {"label", "compensation cap"}}, Win::Balun);
      Ke9esuDialog *cb = openWindow(Win::Combiner); if (CombinerDialog *c = qobject_cast<CombinerDialog *>(cb)) { c->pullKey(Key::CombReq); c->runSync(); }
      grab(cb, "34_combiner_from_balun"); cb->close();
      QString tmp = dir + "/project_roundtrip.rfproj.json"; QString e; bool ok = p->saveFile(tmp, &e) && p->openFile(tmp, &e);
      qInfo("KE9ESU shots: project save/open %s %s", ok ? "OK" : "FAILED", qPrintable(e)); }
    QMenu *rf = nullptr; for (QAction *a : m->mui->menuBar->actions()) if (a->menu() && a->text().contains("RF")) rf = a->menu();
    if (rf) { rf->popup(m->mapToGlobal(QPoint(40, 40))); QApplication::processEvents(); grab(rf, "32_menu"); rf->hide(); }
    // ---- Milestone 2: every window in KE9ESU Light and KE9ESU Dark, a large-text pass and a 1024 px pass
    int orig = m->myOpt->styleGUI;
    for (int style : {int(StyleKe9esuLight), int(StyleKe9esuDark)}) {
        QString sub = dir + (style == StyleKe9esuLight ? "/light" : "/dark"); QDir().mkpath(sub);
        m->applyGuiStyle(style); QApplication::processEvents();
        auto g2 = [&](QWidget *w, const QString &name) { for (int k = 0; k < 4; k++) QApplication::processEvents(); w->grab().save(sub + "/" + name + ".png"); };
        m->mui->tabWidget->setCurrentIndex(0); refreshView(0, _Onelayer); g2(m, "00_main_one_layer");
        refreshView(0, _FerrToroid); g2(m, "01_main_ferrite_toroid");
        refreshView(2, -1); m->on_pushButton_Calculate_clicked(); g2(m, "02_main_lc_result");
        int i2 = 10;
        for (const QString &w : windowNames()) {
            Ke9esuDialog *d = openWindow(w); d->resize(1280, 820);
            if (CombinerDialog *c = qobject_cast<CombinerDialog *>(d)) c->runSync();
            g2(d, QString("%1_%2").arg(i2++).arg(QString(w).replace(QRegularExpression("[^A-Za-z0-9]+"), "_")));
            if (CombinerDialog *c = qobject_cast<CombinerDialog *>(d)) {   // M2b: % tolerance on every row and the wiring diagrams
                QJsonObject keep = c->saveState();
                auto pick = [&](int minParts, bool wantParallelInside) { const auto &rs = c->results();
                    for (int k = 0; k < (int)rs.size(); k++) if (rs[k].count >= minParts && (!wantParallelInside || rs[k].expr.contains('('))) return k; return 0; };
                c->setInputs('R', "1234", "series", 3); c->runSync(); c->selectResult(pick(2, false)); g2(c, "16_combiner_series_R");
                c->setInputs('R', "1234", "mixed", 4); c->runSync(); c->selectResult(pick(3, true)); g2(c, "17_combiner_mixed_R");
                c->setInputs('C', "47p", "parallel", 3); c->runSync(); c->selectResult(pick(2, false)); g2(c, "18_combiner_parallel_C");
                c->setInputs('L', "4.87u", "mixed", 4); c->runSync(); c->selectResult(pick(3, true)); g2(c, "19_combiner_mixed_L");
                for (int k = 0; k < 4; k++) stepFont(+1);
                c->setInputs('R', "1234", "mixed", 4); c->runSync(); c->selectResult(pick(3, true)); g2(c, "24_combiner_large_text"); stepFont(0);
                if (style == StyleKe9esuLight) { QString dd = dir + "/drawings"; QDir().mkpath(dd); const auto imgs = c->drawings();
                    for (auto it = imgs.constBegin(); it != imgs.constEnd(); ++it) it.value().save(dd + "/combiner_" + it.key() + ".png");
                    c->exportSheetPdf(sub + "/24_combiner_sheet.pdf"); }
                c->loadState(keep);
            }
            if (BalunDialog *bd = qobject_cast<BalunDialog *>(d)) {
                bd->applyPreset("ke9esu-auto91"); g2(bd, "20_balun_auto91"); bd->applyPreset("ke9esu-choke"); g2(bd, "21_balun_choke");
                // M2c: low-side shunt capacitor drawn across the SO-239, and the frequency sweep tab
                bd->applyPreset("ke9esu-auto91"); bd->showSweepTab(false); g2(bd, "20c_balun_auto91_cap_diagram"); g2(bd->tabsWidget(), "20d_balun_auto91_winding_tab");
                bd->applyPreset("efhw49-40"); bd->showSweepTab(false); g2(bd, "25_balun_efhw49_cap"); g2(bd->tabsWidget(), "25d_balun_efhw49_winding_tab");
                bd->showSweepTab(); g2(bd, "26_balun_sweep_efhw49"); g2(bd->tabsWidget(), "26d_balun_sweep_tab_efhw49");
                bd->applyPreset("ke9esu-auto91"); bd->showSweepTab(); g2(bd, "27_balun_sweep_auto91");
                { QJsonObject st = bd->saveState(); QJsonObject in = st.value("input").toObject(); in.insert("sweepLoadMode", "range"); in.insert("topology", "efhw49"); in.insert("coreId", "FT240-43"); in.insert("stack", 1);
                  in.insert("conductorId", "awg18e"); in.insert("turns", 2); in.insert("fMinMHz", 7); in.insert("fMaxMHz", 29.7); in.insert("capMode", "suggested"); st.insert("input", in); bd->loadState(st);
                  bd->showSweepTab(); g2(bd, "28_balun_sweep_load_range");
                  bd->optimizeCap(); g2(bd, "29_balun_sweep_optimized"); g2(bd->tabsWidget(), "29d_balun_sweep_tab_optimized");
                  const DesignResult &r = bd->result(); qInfo("KE9ESU shots: sweep %s opt %g pF worst %.2f (off %.2f), cap %g pF", qPrintable(r.topology.id), r.sweep.opt.pF, r.sweep.opt.maxSwr, r.sweep.opt.offMaxSwr, r.hasCompCap ? r.compCap.pF : 0.0);
                  if (style == StyleKe9esuLight) { QString dd = dir + "/drawings"; QDir().mkpath(dd); bd->exportSweepCsv(dd + "/balun_sweep_efhw49.csv"); bd->exportSweepPng(dd + "/balun_sweep_efhw49.png"); } }
                for (int k = 0; k < 4; k++) stepFont(+1);
                bd->resize(1280, 820); bd->applyPreset("efhw49-40"); bd->showSweepTab(); g2(bd, "26b_balun_sweep_large_text"); bd->showSweepTab(false); g2(bd, "25b_balun_efhw49_cap_large_text"); stepFont(0);
                if (style == StyleKe9esuLight) { QString dd = dir + "/drawings"; for (const char *id : {"ke9esu-auto91", "efhw49-40"}) { bd->applyPreset(id); renderWinding(bd->result()).save(dd + "/balun_winding_" + QString(id) + ".png"); } }
                bd->showSweepTab(false); bd->applyPreset("ke9esu");
                bd->resize(1024, 700); g2(bd, "22_balun_1024px");
                for (int k = 0; k < 4; k++) stepFont(+1);
                bd->resize(1280, 820); g2(bd, "23_balun_large_text"); stepFont(0); bd->resize(1280, 820);
            }
            d->close();
        }
        // original Coil64 dialogs under the same theme
        { Options *o = new Options(m); o->setAttribute(Qt::WA_DeleteOnClose, true);
          connect(m, SIGNAL(sendOpt(_OptionStruct)), o, SLOT(getOpt(_OptionStruct))); emit m->sendOpt(*m->myOpt); o->show(); g2(o, "40_coil64_options"); o->close(); }
        { RF_Toroid *t = new RF_Toroid(); t->setAttribute(Qt::WA_DeleteOnClose, true);
          connect(m, SIGNAL(sendOpt(_OptionStruct)), t, SLOT(getOpt(_OptionStruct))); connect(m, SIGNAL(sendLocale(QLocale)), t, SLOT(getCurrentLocale(QLocale)));
          emit m->sendLocale(m->loc); emit m->sendOpt(*m->myOpt); t->show(); g2(t, "41_coil64_rf_toroid"); t->close(); }
        QMenu *view = nullptr; for (QAction *a : m->mui->menuBar->actions()) if (a->menu() && a->text().contains("View")) view = a->menu();
        if (view) { view->popup(m->mapToGlobal(QPoint(60, 30))); QApplication::processEvents(); g2(view, "30_view_menu"); view->hide(); }
        QMenu *file = m->mui->menuBar->actions().isEmpty() ? nullptr : m->mui->menuBar->actions().first()->menu();
        if (file) { file->popup(m->mapToGlobal(QPoint(10, 30))); QApplication::processEvents(); g2(file, "31_file_menu"); file->hide(); }
    }
    m->applyGuiStyle(orig);
    qApp->quit();
}

} // namespace ke9esu
