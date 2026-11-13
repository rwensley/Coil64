/* coreui.cpp - see coreui.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "coreui.h"
#include "widgets.h"
#include "ke9esu_util.h"
#include <QComboBox>
#include <QToolButton>
#include <QBoxLayout>
#include <QRegularExpression>
#include <cmath>

namespace ke9esu {

bool inDb(const Core &c, const QString &db) { return db == "all" || (db == "amidon" ? c.db == "Amidon" : c.db != "Amidon"); }
QString matLabel(const QString &m)
{
    const Material *M = material(m); if (!M) return "Mix " + m;
    return M->kind == "iron" ? "Iron powder " + m + (M->color.isEmpty() ? QString() : " (" + M->color.split('/')[0] + ")") : "Ferrite mix " + m;
}
QStringList matsFor(const QString &db, const QString &kind, bool ironFirst)
{
    QStringList mats;
    for (const Core &c : cores()) if (inDb(c, db) && (kind.isEmpty() || kind == "all" || material(c.mat)->kind == kind) && !mats.contains(c.mat)) mats << c.mat;
    std::stable_sort(mats.begin(), mats.end(), [&](const QString &a, const QString &b) {
        bool ia = material(a)->kind == "iron", ib = material(b)->kind == "iron";
        if (ia != ib) return ironFirst ? ia : ib;
        auto num = [](QString s) { s.remove(QRegularExpression("^-")); bool ok; double v = s.toDouble(&ok); return ok && v ? v : 999; };
        double na = num(a), nb = num(b); if (na != nb) return na < nb; return a < b;
    });
    return mats;
}
QString coreLabel(const Core &c)
{
    bool iron = material(c.mat)->kind == "iron";
    return c.id + "  (" + mmTxt(c.od, AS_STORED, 2) + ", AL " + jsNum(c.al) + S(" nH/T²") + (iron && c.db == "Amidon" && c.hasAlAmidon ? " = " + jsNum(c.alAmidonValue) + S(" µH/100T") : QString()) + ")";
}
QVariantMap wireInfo(const Wire &w)
{
    QVariantMap m; m["id"] = w.id; m["label"] = wireLabel(w); double d = wireDiaMM(w);
    m["diaMM"] = std::isnan(d) ? QVariant() : QVariant(d); m["odMM"] = w.od; m["insul"] = w.jacket.isEmpty() ? (w.id.startsWith("tube") ? "bare" : "enamel") : w.jacket.toLower();
    m["coax"] = w.kind == "coax";
    return m;
}
QString nearestWire(const std::vector<Wire> &list, double diaMM, const QString &preferInsul, bool wiresOnly)
{
    QString best; double bestErr = 1e9;
    for (const Wire &w : list) {
        if (wiresOnly && w.kind == "coax") continue;
        double d = wireDiaMM(w); if (std::isnan(d)) continue;
        double e = std::fabs(std::log(d / diaMM)) + (preferInsul.isEmpty() || w.jacket.toLower() == preferInsul.toLower() || (w.jacket.isEmpty() && preferInsul != "ptfe") ? 0 : 0.05);
        if (e < bestErr) { bestErr = e; best = w.id; }
    }
    return best;
}

CoreSelector::CoreSelector(const QString &kindFilter, QWidget *parent) : QWidget(parent), m_kind(kindFilter)
{
    QHBoxLayout *h = new QHBoxLayout(this); h->setContentsMargins(0, 0, 0, 0); h->setSpacing(4);
    m_db = new QComboBox; m_db->addItem("FR+MM", "fr"); m_db->addItem("Amidon", "amidon"); m_db->addItem(tr("All"), "all");
    m_db->setToolTip(tr("Core database: Fair-Rite + Micrometals datasheet cores, Amidon catalogue (with corrections), or all."));
    m_mat = new QComboBox; m_core = new QComboBox; m_core->setMinimumContentsLength(18); m_core->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    QToolButton *b = new QToolButton; b->setText(tr("Core DB…")); b->setToolTip(tr("Open the core database browser"));
    m_mark = new FromMarker;
    h->addWidget(m_db); h->addWidget(m_mat); h->addWidget(m_core, 1); h->addWidget(b);
    QVBoxLayout *outer = nullptr; Q_UNUSED(outer)
    connect(b, &QToolButton::clicked, this, &CoreSelector::browseRequested);
    connect(m_db, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { if (m_internal) return; fillMats(); fillCores(QString()); m_mark->clear(); emit coreChanged(coreId()); });
    connect(m_mat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { if (m_internal) return; fillCores(QString()); m_mark->clear(); emit coreChanged(coreId()); });
    connect(m_core, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { if (m_internal) return; if (const Core *c = coreById(coreId())) m_lastOd = c->od; m_mark->clear(); emit coreChanged(coreId()); });
    m_internal = true; fillMats(); fillCores(QString()); m_internal = false;
}
QString CoreSelector::coreId() const { return m_core->currentData().toString(); }
QString CoreSelector::db() const { return m_db->currentData().toString(); }
void CoreSelector::setDb(const QString &db) { m_internal = true; m_db->setCurrentIndex(qMax(0, m_db->findData(db))); fillMats(); fillCores(QString()); m_internal = false; }
void CoreSelector::fillMats()
{
    QString cur = m_mat->currentData().toString(); m_mat->clear();
    for (const QString &m : matsFor(db(), m_kind, m_kind == "iron")) m_mat->addItem(matLabel(m), m);
    int i = m_mat->findData(cur); m_mat->setCurrentIndex(i >= 0 ? i : 0);
}
void CoreSelector::fillCores(const QString &sel)
{
    QString mat = m_mat->currentData().toString(); m_core->clear();
    QString pick = sel; double bestErr = 1e9;
    for (const Core &c : cores()) if (c.mat == mat && inDb(c, db())) {
        m_core->addItem(coreLabel(c), c.id);
        if (sel.isEmpty()) { double e = std::fabs(std::log(c.od / m_lastOd)); if (e < bestErr) { bestErr = e; pick = c.id; } }
    }
    int i = m_core->findData(pick); m_core->setCurrentIndex(i >= 0 ? i : 0);
}
void CoreSelector::setCoreId(const QString &id)
{
    const Core *c = coreById(id); if (!c || c->id != id) return;
    m_internal = true;
    if (!inDb(*c, db())) m_db->setCurrentIndex(m_db->findData(c->db == "Amidon" ? "amidon" : "fr"));
    if (!m_kind.isEmpty() && material(c->mat)->kind != m_kind) { m_internal = false; return; }
    fillMats(); m_mat->setCurrentIndex(qMax(0, m_mat->findData(c->mat))); fillCores(id); m_lastOd = c->od;
    m_internal = false;
}

}
