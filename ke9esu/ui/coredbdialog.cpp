/* coredbdialog.cpp - see coredbdialog.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "coredbdialog.h"
#include "coreui.h"
#include "project.h"
#include "balun_core.h"
#include "ke9esu_util.h"
#include <QBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonDocument>
#include <cmath>

namespace ke9esu {

QString coreSourceUrl(const Core &c)
{
    if (!c.webUrl.isEmpty()) return c.webUrl;
    if (c.db == "Amidon") return "https://www.amidoncorp.com/";
    if (c.db == "Fair-Rite") return QRegExp("^59\\d{8}$").exactMatch(c.pn) ? "https://fair-rite.com/product/toroids-" + c.pn + "/" : "https://fair-rite.com/?s=" + QUrl::toPercentEncoding(c.pn);
    if (c.db == "Micrometals") return "https://www.micrometals.com/design-and-applications/literature/";
    return "https://www.google.com/search?q=" + QUrl::toPercentEncoding(c.src);
}

enum Col { CCore, CDb, CMat, CSw, CDims, CAl, CAlAm, CEq, CMag, CFreq, CSrc, NCOL };

CoreDbDialog::CoreDbDialog(QWidget *parent) : Ke9esuDialog(Win::CoreDb, parent)
{
    initData(); applyCoreUnits();
    QVBoxLayout *v = new QVBoxLayout(body()); v->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *h = new QHBoxLayout;
    m_db = new QComboBox; m_db->addItem("FR+MM (datasheets)", "fr"); m_db->addItem("Amidon", "amidon"); m_db->addItem(tr("All"), "all");
    m_kind = new QComboBox; m_kind->addItem(tr("Ferrite + iron powder"), "all"); m_kind->addItem(tr("Ferrite"), "ferrite"); m_kind->addItem(tr("Iron powder"), "iron");
    m_mat = new QComboBox; m_search = new QLineEdit; m_search->setPlaceholderText(tr("search id / part no."));
    m_mark = new FromMarker;
    h->addWidget(new QLabel(tr("Database"))); h->addWidget(m_db); h->addWidget(m_kind); h->addWidget(m_mat); h->addWidget(m_search, 1); h->addWidget(m_mark);
    v->addLayout(h);
    m_t = new QTableWidget(0, NCOL);
    m_t->setHorizontalHeaderLabels({"Core / part no.", "DB", "Mix", "", "OD×ID×H", S("AL nH/T²"), "Amidon AL", S("Equiv. AL"), S("Ae · le · Ve"), "Freq.", "Src"});
    m_t->verticalHeader()->hide(); m_t->setEditTriggers(QAbstractItemView::NoEditTriggers); m_t->setSelectionBehavior(QAbstractItemView::SelectRows); m_t->setSelectionMode(QAbstractItemView::SingleSelection);
    m_t->setAlternatingRowColors(true); m_t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); m_t->setSortingEnabled(false);
    v->addWidget(m_t, 1);
    m_info = new QLabel; m_info->setWordWrap(true); m_info->setTextFormat(Qt::RichText); m_info->setOpenExternalLinks(true);
    v->addWidget(m_info);
    QLabel *hint = new QLabel(S("<span style='color:gray'>* = computed from dimensions; ⚠ = Amidon AL far from geometry (suspect); ≠ = Coil64 and amidoncorp.com differ (the value closer to the geometric estimate is used). "
                                "DB: FR Fair-Rite, MM Micrometals, Am Amidon. AL in nH/turn² everywhere; Amidon iron T cores µH/100 T (= 10 × nH/T²). Double-click Src to open the source page; "
                                "double-click a core to use it in the Balun/Unun designer.</span>")); hint->setWordWrap(true);
    v->addWidget(hint);
    finishLayout(true, tr("Accept → Coil64 ferrite toroid"));
    resize(1240, 760);
    auto refillMats = [this] { m_internal = true; QString cur = m_mat->currentData().toString(); m_mat->clear(); m_mat->addItem(tr("all mixes"), "");
        for (const QString &m : matsFor(m_db->currentData().toString(), m_kind->currentData().toString())) m_mat->addItem(matLabel(m), m);
        m_mat->setCurrentIndex(qMax(0, m_mat->findData(cur))); m_internal = false; };
    connect(m_db, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, refillMats] { refillMats(); fill(); });
    connect(m_kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, refillMats] { refillMats(); fill(); });
    connect(m_mat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { if (!m_internal) fill(); });
    connect(m_search, &QLineEdit::textChanged, this, &CoreDbDialog::fill);
    connect(m_t, &QTableWidget::itemSelectionChanged, this, [this] {
        if (m_internal) return; QString id = selectedId(); const Core *c = coreById(id); if (!c) return; m_mark->clear();
        const Material *M = material(c->mat);
        m_info->setText(QString("<b>%1</b> — %2 %3, %4; AL %5 nH/T² (+%6/−%7%); µi %8, Bsat %9 mT, Curie %10 °C. Source: <a href='%11'>%12</a>")
                            .arg(esc(c->id), esc(c->db), esc(c->pn), esc(dimsTxt(*c)), jsNum(c->al), jsNum(c->alTol[0]), jsNum(c->alTol[1]), jsNum(M->mui), jsNum(M->bsat_mT), jsNum(M->curieC), coreSourceUrl(*c), esc(c->src)));
        if (isVisible() && ke9esuSettings()->value("KE9ESU/link/live", true).toBool()) Project::instance()->set(Key::Core, id, window());
    });
    connect(m_t, &QTableWidget::cellDoubleClicked, this, [this](int r, int c) {
        QString id = m_t->item(r, CCore)->data(Qt::UserRole).toString(); const Core *core = coreById(id);
        if (c == CSrc && core) { QDesktopServices::openUrl(QUrl(coreSourceUrl(*core))); return; }
        Project::instance()->set(Key::Core, id, window()); Project::instance()->requestWindow(Win::Balun, Key::Core); });
    connect(Project::instance(), &Project::unitsChanged, this, [this] { applyCoreUnits(); fill(); });
    connect(Project::instance(), &Project::changed, this, [this](const QString &key, const QString &src) {
        if (key == Key::Core && src != window() && ke9esuSettings()->value("KE9ESU/link/live", true).toBool() && isVisible()) pullKey(key); });
    refillMats(); fill();
    QJsonObject st = Project::instance()->state(window()); if (st.isEmpty()) st = QJsonDocument::fromJson(ke9esuSettings()->value("KE9ESU/state/" + window()).toByteArray()).object();
    loadState(st);
}

void CoreDbDialog::fill()
{
    if (m_internal) return;
    QString keep = selectedId(), db = m_db->currentData().toString(), kind = m_kind->currentData().toString(), mat = m_mat->currentData().toString(), q = m_search->text().trimmed();
    m_internal = true; m_t->setRowCount(0); int r = 0;
    bool am = db != "fr";
    m_t->setColumnHidden(CAlAm, !am); m_t->setColumnHidden(CSw, !am);
    for (const Core &c : cores()) {
        if (!inDb(c, db)) continue; const Material *M = material(c.mat);
        if (kind != "all" && M->kind != kind) continue; if (!mat.isEmpty() && c.mat != mat) continue;
        if (!q.isEmpty() && !c.id.contains(q, Qt::CaseInsensitive) && !c.pn.contains(q, Qt::CaseInsensitive)) continue;
        m_t->insertRow(r);
        auto put = [&](int col, const QString &s, const QString &tip = QString(), bool num = false) { QTableWidgetItem *it = new QTableWidgetItem(s); if (!tip.isEmpty()) it->setToolTip(tip);
            if (num) it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter); m_t->setItem(r, col, it); return it; };
        auto cm = [&](const char *k, double v) { return jsNum(v) + (c.computed.contains(k) ? "*" : ""); };
        QString pn = QString(c.pn).remove(QRegExp("^Amidon ")); QTableWidgetItem *ci = put(CCore, c.id + (pn != QString(c.id).remove(QRegExp("^A:")) && !pn.isEmpty() ? "\n" + c.pn : QString()));
        ci->setData(Qt::UserRole, c.id); QFont fb = ci->font(); fb.setBold(true); ci->setFont(fb);
        put(CDb, c.db == "Fair-Rite" ? "FR" : c.db == "Micrometals" ? "MM" : "Am", c.db);
        put(CMat, c.mat, matLabel(c.mat));
        QString col = !c.color.isEmpty() ? c.color : M->color; QTableWidgetItem *sw = put(CSw, QString(), col);
        if (!col.isEmpty()) { QString first = col.split(QRegExp("[/ ]"))[0].toLower(); QColor qc(first); if (qc.isValid()) sw->setBackground(qc); }
        put(CDims, QString(dimsTxt(c)).remove(QRegExp(" (mm|in)$")), QString(), true);
        QString alTxt = jsNum(c.al) + (c.computed.contains("AL") ? "*" : "") + (c.alSuspect ? S(" ⚠") : "") + (c.alDisagree ? S(" ≠") : "");
        QString alTip = S("tolerance +%1/−%2%").arg(jsNum(c.alTol[0]), jsNum(c.alTol[1]));
        if (c.db == "Amidon") alTip += QString("\nCoil64: %1  web: %2  geometry: %3  → picked: %4").arg(std::isnan(c.alCoil64) ? "-" : jsNum(c.alCoil64), std::isnan(c.alWeb) ? "-" : jsNum(c.alWeb), std::isnan(c.alGeom) ? "-" : jsNum(c.alGeom), c.alPick);
        QTableWidgetItem *ai = put(CAl, alTxt, alTip, true); if (c.alSuspect || c.alDisagree) ai->setForeground(sevColor("warn"));
        put(CAlAm, c.hasAlAmidon ? jsNum(c.alAmidonValue) + (c.alAmidonUnit.contains("uH") ? S(" µH/100T") : " mH/kT") : QString(), QString(), true);
        if (c.equiv.has) { QTableWidgetItem *e = put(CEq, jsNum(c.equiv.al), c.equiv.db + " " + c.equiv.id, true); if (std::fabs(c.equiv.al - c.al) > 0.05 * c.al) e->setForeground(sevColor("warn")); }
        put(CMag, cm("Ae", c.ae) + S(" · ") + cm("le", c.le) + S(" · ") + cm("Ve", c.ve), S("Ae cm² · le cm · Ve cm³"), true);
        put(CFreq, c.freq.isEmpty() ? M->freq : c.freq);
        QTableWidgetItem *s = put(CSrc, S("↗"), c.src + S(" — ") + coreSourceUrl(c)); s->setForeground(QColor("#1f5fbf"));
        r++;
    }
    m_t->resizeRowsToContents();
    m_internal = false;
    setWindowTitle(QString("Core database (%1 cores) - KE9ESU").arg(r));
    if (!keep.isEmpty()) select(keep);
}

QString CoreDbDialog::selectedId() const
{
    QList<QTableWidgetItem *> s = m_t->selectedItems(); if (s.isEmpty()) return QString();
    QTableWidgetItem *it = m_t->item(s[0]->row(), CCore); return it ? it->data(Qt::UserRole).toString() : QString();
}
void CoreDbDialog::select(const QString &id)
{
    for (int r = 0; r < m_t->rowCount(); r++) if (m_t->item(r, CCore)->data(Qt::UserRole).toString() == id) {
        bool was = m_internal; m_internal = true; m_t->selectRow(r); m_t->scrollToItem(m_t->item(r, CCore)); m_internal = was; return; }
}
void CoreDbDialog::pullKey(const QString &key)
{
    if (key != Key::Core) return;
    QString id = Project::instance()->get(key).toString(); const Core *c = coreById(id); if (!c || c->id != id) return;
    if (!inDb(*c, m_db->currentData().toString())) m_db->setCurrentIndex(m_db->findData("all"));
    if (!m_mat->currentData().toString().isEmpty() && m_mat->currentData().toString() != c->mat) m_mat->setCurrentIndex(0);
    if (m_kind->currentData() != "all" && material(c->mat)->kind != m_kind->currentData().toString()) m_kind->setCurrentIndex(0);
    if (!m_search->text().isEmpty()) m_search->clear();
    fill(); select(id); m_mark->setFrom(Project::instance()->source(key));
    m_internal = true; emit m_t->itemSelectionChanged(); m_internal = false;
}
void CoreDbDialog::fillPull(QMenu *m) { addPull(m, Key::Core, tr("Core") + ": " + Project::instance()->get(Key::Core).toString()); }
void CoreDbDialog::fillUseIn(QMenu *m)
{
    QString id = selectedId(); if (id.isEmpty()) { m->addAction(tr("(select a core first)"))->setEnabled(false); return; }
    Project *p = Project::instance(); QString w = window();
    m->addAction(tr("%1 → Balun/Unun designer").arg(id), this, [p, w, id] { p->set(Key::Core, id, w); p->requestWindow(Win::Balun, Key::Core); });
    m->addAction(tr("%1 → LC trap (toroid)").arg(id), this, [p, w, id] { p->set(Key::Core, id, w); p->requestWindow(Win::Lc, Key::Core); });
    m->addAction(tr("%1 → Coil64 ferrite toroid calculator").arg(id), this, [p, w, id] { p->set(Key::Core, id, w); p->requestCoil64("ferriteCore", {{"coreId", id}, {"stack", 1}, {"switch", true}}); });
}
void CoreDbDialog::acceptToCoil64()
{
    QString id = selectedId(); if (id.isEmpty()) return;
    Project::instance()->set(Key::Core, id, window()); Project::instance()->requestCoil64("ferriteCore", {{"coreId", id}, {"stack", 1}, {"switch", true}});
}
QJsonObject CoreDbDialog::saveState() const
{
    QJsonObject o; o.insert("db", m_db->currentData().toString()); o.insert("kind", m_kind->currentData().toString()); o.insert("mat", m_mat->currentData().toString()); o.insert("selected", selectedId()); return o;
}
void CoreDbDialog::loadState(const QJsonObject &o)
{
    if (o.isEmpty()) return;
    m_db->setCurrentIndex(qMax(0, m_db->findData(o.value("db").toString("fr")))); m_kind->setCurrentIndex(qMax(0, m_kind->findData(o.value("kind").toString("all"))));
    m_mat->setCurrentIndex(qMax(0, m_mat->findData(o.value("mat").toString()))); fill(); select(o.value("selected").toString());
}
QString CoreDbDialog::sheetHtml() const
{
    QString h = "<html><head>" + sheetCss() + "</head><body><h1>Core database</h1><table class='t' cellspacing='0'><tr><th>Core</th><th>DB</th><th>Mix</th><th>OD×ID×H</th><th>AL nH/T²</th><th>Ae · le · Ve</th></tr>";
    for (int r = 0; r < m_t->rowCount(); r++) {
        const Core *c = coreById(m_t->item(r, CCore)->data(Qt::UserRole).toString()); if (!c) continue;
        h += "<tr><td>" + esc(c->id) + "</td><td>" + esc(c->db) + "</td><td>" + esc(c->mat) + "</td><td>" + esc(dimsTxt(*c)) + "</td><td class='n'>" + jsNum(c->al) + "</td><td class='n'>" + jsNum(c->ae) + S(" · ") + jsNum(c->le) + S(" · ") + jsNum(c->ve) + "</td></tr>";
    }
    return h + "</table></body></html>";
}

} // namespace ke9esu
