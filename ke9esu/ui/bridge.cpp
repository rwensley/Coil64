/* bridge.cpp - see bridge.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "bridge.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "project.h"
#include "widgets.h"
#include "balundialog.h"
#include "eflwdialog.h"
#include "trapdialogs.h"
#include "coredbdialog.h"
#include "combinerdialog.h"
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
    qApp->quit();
}

} // namespace ke9esu
