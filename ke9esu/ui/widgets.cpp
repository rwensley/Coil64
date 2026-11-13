/* widgets.cpp - see widgets.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "widgets.h"
#include "project.h"
#include "ke9esu_util.h"
#include "system_functions.h"
#include <QBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QSettings>
#include <QMenu>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextDocument>
#include <QPrinter>
#include <QPrintDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QFrame>
#include <QUrl>
#include <QApplication>
#include <cmath>

namespace ke9esu {

QString f(double v, int d) { return toFixed(v, d); }
QString fmtOhm(double v) { return v >= 10000 ? f(v / 1000, 1) + S(" kΩ") : v >= 1000 ? f(v / 1000, 2) + S(" kΩ") : rnd(v) + S(" Ω"); }
QString esc(const QString &s) { return s.toHtmlEscaped(); }
QString sevTag(const QString &s) { return s == "fail" ? "FAIL" : s == "warn" ? "CHECK" : s == "info" ? "NOTE" : "OK"; }
QColor sevColor(const QString &s) { return s == "fail" ? QColor("#c62828") : s == "warn" ? QColor("#b26a00") : s == "info" ? QColor("#1f5fbf") : QColor("#2e7d32"); }

QSettings *ke9esuSettings()
{
    static QSettings *s = nullptr;
    if (!s) defineAppSettings(s);
    return s;
}

void applyCoreUnits()
{
    const Project::Units &u = Project::instance()->units();
    bool inch = u.len == "in" || u.len == "mil";
    ke9esu::setUnits(u.awg ? "awg" : inch ? "in" : "mm", inch ? "in" : "mm");
}

/* ------------------------------------------------------------------ ChecksBar */
class ChecksNotifier : public QObject { public: static ChecksNotifier *get() { static ChecksNotifier *n = new ChecksNotifier; return n; } };
static const char *SEVS[4] = {"fail", "warn", "info", "ok"};

ChecksBar::ChecksBar(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *v = new QVBoxLayout(this); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(4);
    QHBoxLayout *h = new QHBoxLayout; h->setSpacing(10);
    QLabel *t = new QLabel("<b>" + tr("Checks:") + "</b>"); h->addWidget(t);
    for (int i = 0; i < 4; i++) {
        m_box[i] = new QCheckBox(this);
        m_box[i]->setToolTip(tr("Show/hide the %1 items").arg(sevTag(SEVS[i])));
        m_box[i]->setChecked(ke9esuSettings()->value(QString("KE9ESU/checks/show_") + SEVS[i], false).toBool());
        connect(m_box[i], &QCheckBox::toggled, this, [i](bool on) {
            ke9esuSettings()->setValue(QString("KE9ESU/checks/show_") + SEVS[i], on);
            for (QWidget *w : QApplication::allWidgets()) if (ChecksBar *b = qobject_cast<ChecksBar *>(w)) b->reload();
        });
        h->addWidget(m_box[i]);
    }
    QLabel *hint = new QLabel("<i>" + tr("tick to list") + "</i>"); hint->setWordWrap(true); h->addWidget(hint, 1);
    v->addLayout(h);
    m_list = new QLabel; m_list->setWordWrap(true); m_list->setTextFormat(Qt::RichText); m_list->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(m_list);
}

void ChecksBar::setChecks(const std::vector<Check> &checks) { m_checks = checks; reload(); }

void ChecksBar::reload()
{
    int n[4] = {0, 0, 0, 0};
    for (const Check &c : m_checks) for (int i = 0; i < 4; i++) if (c.status == SEVS[i]) n[i]++;
    QStringList items;
    for (int i = 0; i < 4; i++) {
        bool on = ke9esuSettings()->value(QString("KE9ESU/checks/show_") + SEVS[i], false).toBool();
        { QSignalBlocker b(m_box[i]); m_box[i]->setChecked(on); }
        m_box[i]->setText(QString("%1 %2").arg(sevTag(SEVS[i])).arg(n[i]));
        QColor c = sevColor(SEVS[i]);
        m_box[i]->setStyleSheet(QString("QCheckBox{color:%1;font-weight:bold;padding:2px 6px;border:1px solid %1;border-radius:9px;%2}")
                                    .arg(c.name(), n[i] ? "" : "color:#999;border-color:#bbb;font-weight:normal;"));
        if (on) for (const Check &ck : m_checks) if (ck.status == SEVS[i])
            items << QString("<tr><td style='padding-right:6px'><span style='color:%1;font-weight:bold'>%2</span></td><td>%3</td></tr>").arg(c.name(), sevTag(ck.status), esc(ck.text));
    }
    m_list->setText(items.isEmpty() ? QString() : "<table cellspacing='2'>" + items.join("") + "</table>");
    m_list->setVisible(!items.isEmpty());
}

QString ChecksBar::summaryText(const std::vector<Check> &checks)
{
    int n[4] = {0, 0, 0, 0};
    for (const Check &c : checks) for (int i = 0; i < 4; i++) if (c.status == SEVS[i]) n[i]++;
    return QString("FAIL %1 · CHECK %2 · NOTE %3 · OK %4").arg(n[0]).arg(n[1]).arg(n[2]).arg(n[3]);
}

QString ChecksBar::sheetHtml(const std::vector<Check> &checks)
{
    QString h = "<p><b>Checks:</b> " + esc(summaryText(checks)) + "</p>";
    QStringList bad;
    for (const char *s : {"fail", "warn"}) for (const Check &c : checks) if (c.status == s)
        bad << QString("<li><b style='color:%1'>%2</b> %3</li>").arg(sevColor(s).name(), sevTag(s), esc(c.text));
    return h + (bad.isEmpty() ? "<p>No FAIL or CHECK items.</p>" : "<ul>" + bad.join("") + "</ul>");
}

/* ------------------------------------------------------------------ Rollup */
Rollup::Rollup(const QString &key, const QString &title, QWidget *body, QWidget *parent) : QWidget(parent), m_key(key), m_body(body)
{
    QVBoxLayout *v = new QVBoxLayout(this); v->setContentsMargins(0, 2, 0, 2); v->setSpacing(2);
    QHBoxLayout *h = new QHBoxLayout; h->setSpacing(6);
    m_btn = new QToolButton; m_btn->setText(QString(title).replace("&", "&&")); m_btn->setCheckable(true); m_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); m_btn->setAutoRaise(true);
    m_btn->setStyleSheet("QToolButton{font-weight:bold;}");
    m_sum = new QLabel; m_sum->setTextFormat(Qt::RichText); m_sum->setWordWrap(true);
    h->addWidget(m_btn); h->addWidget(m_sum, 1);
    v->addLayout(h); v->addWidget(body);
    connect(m_btn, &QToolButton::toggled, this, [this](bool on) { m_body->setVisible(on); m_btn->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        ke9esuSettings()->setValue("KE9ESU/tables/" + m_key + "/open", on); });
    setOpen(ke9esuSettings()->value("KE9ESU/tables/" + key + "/open", false).toBool());
}
void Rollup::setSummary(const QString &html) { m_sum->setText(html); }
bool Rollup::isOpen() const { return m_btn->isChecked(); }
void Rollup::setOpen(bool open) { m_btn->setChecked(open); m_body->setVisible(open); m_btn->setArrowType(open ? Qt::DownArrow : Qt::RightArrow); }

/* ------------------------------------------------------------------ Cards */
Cards::Cards(int columns, QWidget *parent) : QWidget(parent), m_cols(columns)
{
    m_grid = new QGridLayout(this); m_grid->setContentsMargins(0, 0, 0, 0); m_grid->setSpacing(6);
}
void Cards::setCards(const QList<QPair<QString, QString>> &cards)
{
    while (QLayoutItem *it = m_grid->takeAt(0)) delete it;
    qDeleteAll(m_tiles); m_tiles.clear();
    for (int i = 0; i < cards.size(); i++) {
        QFrame *fr = new QFrame(this); fr->setFrameShape(QFrame::StyledPanel); fr->setObjectName("ke9esuCard");
        fr->setStyleSheet("#ke9esuCard{border:1px solid palette(mid);border-radius:6px;}");
        QVBoxLayout *v = new QVBoxLayout(fr); v->setContentsMargins(8, 4, 8, 4); v->setSpacing(0);
        QLabel *k = new QLabel(cards[i].first); QFont fk = k->font(); fk.setPointSizeF(fk.pointSizeF() * 0.85); k->setFont(fk); k->setStyleSheet("color:palette(dark);"); k->setWordWrap(true);
        QLabel *val = new QLabel(cards[i].second); QFont fv = val->font(); fv.setBold(true); fv.setPointSizeF(fv.pointSizeF() * 1.1); val->setFont(fv);
        val->setTextInteractionFlags(Qt::TextSelectableByMouse); val->setWordWrap(true);
        v->addWidget(k); v->addWidget(val);
        m_tiles << fr;
    }
    m_used = 0; relayout();
}
void Cards::relayout()
{
    int cols = qBound(2, width() / 150, m_cols);
    if (cols == m_used && m_grid->count() == m_tiles.size()) return;
    while (QLayoutItem *it = m_grid->takeAt(0)) delete it;
    for (int c = 0; c < m_cols; c++) m_grid->setColumnStretch(c, c < cols ? 1 : 0);
    for (int i = 0; i < m_tiles.size(); i++) m_grid->addWidget(m_tiles[i], i / cols, i % cols);
    m_used = cols;
}
void Cards::resizeEvent(QResizeEvent *e) { QWidget::resizeEvent(e); relayout(); }

/* ------------------------------------------------------------------ FromMarker */
FromMarker::FromMarker(QWidget *parent) : QLabel(parent) { setStyleSheet("color:#1f5fbf;font-style:italic;"); hide(); }
void FromMarker::setFrom(const QString &w) { m_from = w; setText(S("◂ from ") + w); setToolTip(tr("Value pulled in from the %1 window").arg(w)); setVisible(!w.isEmpty()); }
void FromMarker::clear() { m_from.clear(); setText(QString()); hide(); }

/* ------------------------------------------------------------------ LenSpin */
LenSpin::LenSpin(double minMM, double maxMM, double valueMM, QWidget *parent) : QDoubleSpinBox(parent), m_minMM(minMM), m_maxMM(maxMM), m_mm(valueMM)
{
    setKeyboardTracking(false);
    connect(Project::instance(), &Project::unitsChanged, this, &LenSpin::unitsChanged);
    connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (m_internal) return; m_mm = Project::instance()->fromDispLen(v); emit changedMM(m_mm); });
    unitsChanged();
}
double LenSpin::valueMM() const { return m_mm; }
void LenSpin::setValueMM(double mm)
{
    m_mm = mm; m_internal = true; setValue(Project::instance()->toDispLen(mm)); m_internal = false;
}
void LenSpin::unitsChanged()
{
    Project *p = Project::instance(); m_internal = true;
    setDecimals(p->lenDecimals()); setSuffix(" " + p->lenUnit());
    setRange(p->toDispLen(m_minMM), p->toDispLen(m_maxMM));
    setSingleStep(p->lenUnit() == "mm" ? 1 : p->lenUnit() == "cm" ? 0.1 : p->lenUnit() == "mil" ? 10 : 0.05);
    setValue(p->toDispLen(m_mm)); m_internal = false;
}

/* ------------------------------------------------------------------ ProjectBox */
static QDoubleSpinBox *spin(double lo, double hi, double v, int dec, const QString &suffix)
{
    QDoubleSpinBox *s = new QDoubleSpinBox; s->setRange(lo, hi); s->setDecimals(dec); s->setValue(v); s->setSuffix(suffix); s->setKeyboardTracking(false); return s;
}
ProjectBox::ProjectBox(const QString &window, int fields, QWidget *parent) : QGroupBox(tr("Project (shared by every RF window)"), parent), m_window(window)
{
    Project *p = Project::instance();
    setToolTip(tr("Set once: power, impedances, SWR, duty cycle and ambient temperature are used by every heating and voltage check in all RF windows."));
    QFormLayout *fl = new QFormLayout(this); fl->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto row = [&](const char *key, const QString &label, QWidget *w, bool show) {
        FromMarker *m = new FromMarker; m_marks.insert(key, m);
        QWidget *box = new QWidget; QHBoxLayout *h = new QHBoxLayout(box); h->setContentsMargins(0, 0, 0, 0); h->addWidget(w); h->addWidget(m); h->addStretch();
        fl->addRow(label, box); if (!show) { box->hide(); fl->labelForField(box)->hide(); }
    };
    m_power = spin(1, 10000, p->num(Key::Power, 100), 0, " W");
    m_zSys = spin(1, 600, p->num(Key::ZSys, 50), 1, S(" Ω"));
    m_vswr = spin(1, 20, p->num(Key::Vswr, 3), 2, "");
    m_duty = new QComboBox; m_duty->addItem("SSB (30 %)", "ssb"); m_duty->addItem("CW (50 %)", "cw"); m_duty->addItem("Digital / FT8 (100 %)", "digital"); m_duty->addItem(tr("Custom"), "custom");
    m_duty->setCurrentIndex(qMax(0, m_duty->findData(p->get(Key::Duty, "digital").toString())));
    m_dutyPct = spin(1, 100, p->num(Key::DutyPct, 100), 0, " %");
    QWidget *dutyW = new QWidget; QHBoxLayout *dh = new QHBoxLayout(dutyW); dh->setContentsMargins(0, 0, 0, 0); dh->addWidget(m_duty); dh->addWidget(m_dutyPct);
    m_zEnd = spin(10, 20000, p->num(Key::ZEnd, 3000), 0, S(" Ω"));
    m_ambient = spin(-40, 80, p->num(Key::Ambient, 30), 0, S(" °C"));
    row(Key::Power, tr("Power"), m_power, fields & Power);
    row(Key::ZSys, tr("System impedance (low side)"), m_zSys, fields & ZSys);
    row(Key::Vswr, tr("Worst-case SWR"), m_vswr, fields & Vswr);
    row(Key::Duty, tr("Duty cycle"), dutyW, fields & Duty);
    row(Key::ZEnd, tr("Impedance at the trap / wire end"), m_zEnd, fields & ZEnd);
    row(Key::Ambient, tr("Ambient temperature"), m_ambient, fields & Ambient);
    auto hook = [this](QDoubleSpinBox *s, const char *key) {
        connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, key](double v) { if (m_internal) return; m_marks[key]->clear(); publish(key, v); emit edited(); });
    };
    hook(m_power, Key::Power); hook(m_zSys, Key::ZSys); hook(m_vswr, Key::Vswr); hook(m_zEnd, Key::ZEnd); hook(m_ambient, Key::Ambient); hook(m_dutyPct, Key::DutyPct);
    connect(m_duty, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (m_internal) return; m_marks[Key::Duty]->clear(); QString d = duty(); bool known = false; double pc = dutyPercentFor(d, &known);
        m_dutyPct->setEnabled(d == "custom"); if (known) { m_internal = true; m_dutyPct->setValue(pc); m_internal = false; }
        publish(Key::Duty, d); publish(Key::DutyPct, m_dutyPct->value()); emit edited(); });
    m_dutyPct->setEnabled(duty() == "custom");
    connect(p, &Project::changed, this, &ProjectBox::projectChanged);
}
void ProjectBox::publish(const char *key, const QVariant &v) { Project::instance()->set(key, v, m_window); }
double ProjectBox::power() const { return m_power->value(); }
double ProjectBox::zSys() const { return m_zSys->value(); }
double ProjectBox::vswr() const { return m_vswr->value(); }
QString ProjectBox::duty() const { return m_duty->currentData().toString(); }
double ProjectBox::dutyPercent() const { return m_dutyPct->value(); }
double ProjectBox::zEnd() const { return m_zEnd->value(); }
double ProjectBox::ambient() const { return m_ambient->value(); }
void ProjectBox::setValues(double power, double zSys, double vswr, const QString &duty, double dutyPct, double zEnd, double ambient, bool pub)
{
    m_internal = true;
    m_power->setValue(power); m_zSys->setValue(zSys); m_vswr->setValue(vswr); m_duty->setCurrentIndex(qMax(0, m_duty->findData(duty)));
    m_dutyPct->setValue(dutyPct); m_dutyPct->setEnabled(duty == "custom"); m_zEnd->setValue(zEnd); m_ambient->setValue(ambient);
    m_internal = false;
    if (pub) { publish(Key::Power, power); publish(Key::ZSys, zSys); publish(Key::Vswr, vswr); publish(Key::Duty, duty); publish(Key::DutyPct, dutyPct); publish(Key::ZEnd, zEnd); publish(Key::Ambient, ambient); }
}
void ProjectBox::projectChanged(const QString &key, const QString &source)
{
    if (source == m_window || !m_marks.contains(key)) return;
    Project *p = Project::instance(); m_internal = true;
    if (key == Key::Power) m_power->setValue(p->num(key));
    else if (key == Key::ZSys) m_zSys->setValue(p->num(key));
    else if (key == Key::Vswr) m_vswr->setValue(p->num(key));
    else if (key == Key::ZEnd) m_zEnd->setValue(p->num(key));
    else if (key == Key::Ambient) m_ambient->setValue(p->num(key));
    else if (key == Key::DutyPct) m_dutyPct->setValue(p->num(key));
    else if (key == Key::Duty) { m_duty->setCurrentIndex(qMax(0, m_duty->findData(p->get(key).toString()))); m_dutyPct->setEnabled(duty() == "custom"); }
    m_internal = false;
    m_marks[key]->setFrom(source);
    emit edited();
}

/* ------------------------------------------------------------------ build sheets */
QString sheetCss()
{
    return "<style>body{font-family:sans-serif;font-size:10pt;} h1{font-size:15pt;margin:0;} h2{font-size:12pt;margin:10px 0 4px 0;}"
           "table.t{border-collapse:collapse;} table.t td, table.t th{border:1px solid #888;padding:2px 6px;} th{background:#eee;text-align:left;}"
           "td.n{text-align:right;} .hint{color:#555;font-size:8.5pt;}</style>";
}
QString partsTableHtml(const std::vector<Part> &parts)
{
    QString h = "<table class='t' cellspacing='0'><tr><th>Qty</th><th>Item</th></tr>";
    for (const Part &p : parts) h += "<tr><td class='n'>" + jsNum(p.qty) + "</td><td>" + esc(p.item) + "</td></tr>";
    return h + "</table>";
}
QString stepsHtml(const QStringList &steps)
{
    QString h = "<ol>"; for (const QString &s : steps) h += "<li>" + esc(s) + "</li>"; return h + "</ol>";
}
static void addImages(QTextDocument *doc, const QMap<QString, QImage> &images)
{
    for (auto it = images.constBegin(); it != images.constEnd(); ++it) doc->addResource(QTextDocument::ImageResource, QUrl("ke9esu://" + it.key()), it.value());
}
SheetDialog::SheetDialog(const QString &title, const QString &html, const QMap<QString, QImage> &images, QWidget *parent)
    : QDialog(parent), m_html(html), m_title(title), m_images(images)
{
    setWindowTitle(tr("Build sheet") + " - " + title); resize(820, 900); setAttribute(Qt::WA_DeleteOnClose);
    QVBoxLayout *v = new QVBoxLayout(this);
    QTextBrowser *tb = new QTextBrowser; addImages(tb->document(), images); tb->setHtml(html); v->addWidget(tb);
    QHBoxLayout *h = new QHBoxLayout; h->addStretch();
    QPushButton *pr = new QPushButton(tr("Print…")), *pdf = new QPushButton(tr("Save as PDF…")), *cl = new QPushButton(tr("Close"));
    h->addWidget(pr); h->addWidget(pdf); h->addWidget(cl); v->addLayout(h);
    connect(cl, &QPushButton::clicked, this, &QDialog::close);
    connect(pr, &QPushButton::clicked, this, [this] {
        QPrinter printer(QPrinter::HighResolution); QPrintDialog dlg(&printer, this);
        if (dlg.exec() != QDialog::Accepted) return;
        QTextDocument doc; addImages(&doc, m_images); doc.setHtml(m_html); doc.print(&printer);
    });
    connect(pdf, &QPushButton::clicked, this, [this] {
        QString name = m_title; name.replace(QRegExp("[^A-Za-z0-9._-]+"), "_");
        QString path = QFileDialog::getSaveFileName(this, tr("Save build sheet as PDF"), defineSavePath() + "/" + name + ".pdf", "PDF (*.pdf)");
        if (path.isEmpty()) return;
        if (!savePdf(m_html, m_images, path)) QMessageBox::warning(this, tr("Build sheet"), tr("Could not write %1").arg(path));
    });
}
bool SheetDialog::savePdf(const QString &html, const QMap<QString, QImage> &images, const QString &path)
{
    QPrinter printer(QPrinter::HighResolution); printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::Letter)); printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    QTextDocument doc; addImages(&doc, images); doc.setHtml(html); doc.print(&printer);
    return QFile::exists(path);
}

/* ------------------------------------------------------------------ Ke9esuDialog */
Ke9esuDialog::Ke9esuDialog(const QString &window, QWidget *parent) : QDialog(parent), m_window(window)
{
    setWindowTitle(window + " - KE9ESU");
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setModal(false);
    m_body = new QWidget;
}
void Ke9esuDialog::finishLayout(bool withAccept, const QString &acceptText)
{
    QVBoxLayout *v = new QVBoxLayout(this);
    v->addWidget(m_body, 1);
    QHBoxLayout *h = new QHBoxLayout;
    m_pull = new QToolButton; m_pull->setText(tr("Pull value…")); m_pull->setPopupMode(QToolButton::InstantPopup);
    m_pull->setToolTip(tr("Take a value another window published (it is marked \"from <window>\")."));
    QMenu *pm = new QMenu(m_pull); m_pull->setMenu(pm); connect(pm, &QMenu::aboutToShow, this, [this, pm] { pm->clear(); fillPull(pm); if (pm->isEmpty()) pm->addAction(tr("(nothing published yet)"))->setEnabled(false); });
    m_useIn = new QToolButton; m_useIn->setText(tr("Use in…")); m_useIn->setPopupMode(QToolButton::InstantPopup);
    m_useIn->setToolTip(tr("Send a result of this window to another RF window or to the Coil64 calculators."));
    QMenu *um = new QMenu(m_useIn); m_useIn->setMenu(um); connect(um, &QMenu::aboutToShow, this, [this, um] { um->clear(); fillUseIn(um); });
    QPushButton *sheet = new QPushButton(tr("Build sheet…"));
    sheet->setToolTip(tr("Printable build sheet: preview, print or save as PDF."));
    connect(sheet, &QPushButton::clicked, this, [this] { (new SheetDialog(sheetTitle(), sheetHtml(), sheetImages(), this))->show(); });
    h->addWidget(m_pull); h->addWidget(m_useIn); h->addWidget(sheet); h->addStretch();
    if (withAccept) {
        QPushButton *acc = new QPushButton(acceptText.isEmpty() ? tr("Accept → Coil64") : acceptText);
        acc->setToolTip(tr("Send the result to the Coil64 main window (in its current units), switch to the matching calculator and close."));
        connect(acc, &QPushButton::clicked, this, [this] { acceptToCoil64(); close(); });
        h->addWidget(acc);
    }
    QPushButton *cl = new QPushButton(tr("Close")); connect(cl, &QPushButton::clicked, this, &QDialog::close); h->addWidget(cl);
    v->addLayout(h);
    QByteArray g = ke9esuSettings()->value("KE9ESU/geometry/" + m_window).toByteArray();
    if (!g.isEmpty()) restoreGeometry(g);
}
void Ke9esuDialog::addPull(QMenu *m, const QString &key, const QString &text)
{
    Project *p = Project::instance();
    QString src = p->source(key);
    QAction *a = m->addAction(text + (src.isEmpty() ? QString() : S("  —  from ") + src));
    a->setEnabled(p->has(key) && src != m_window);
    connect(a, &QAction::triggered, this, [this, key] { pullKey(key); });
}
void Ke9esuDialog::closeEvent(QCloseEvent *e)
{
    ke9esuSettings()->setValue("KE9ESU/geometry/" + m_window, saveGeometry());
    Project::instance()->setState(m_window, saveState());
    QDialog::closeEvent(e);
}
void Ke9esuDialog::showEvent(QShowEvent *e) { QDialog::showEvent(e); }

} // namespace ke9esu
