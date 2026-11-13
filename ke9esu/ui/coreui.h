/* coreui.h - KE9ESU fork of Coil64: core / material / wire helpers shared by the RF windows (database filter
   FR+MM / Amidon / All, material and core labels, a core selector, wire <-> project mapping).
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_COREUI_H
#define KE9ESU_COREUI_H
#include <QWidget>
#include <QVariantMap>
#include "balun_core.h"
class QComboBox; class QToolButton;
namespace ke9esu {
class FromMarker;
bool inDb(const Core &c, const QString &db);                 // db: "fr" (Fair-Rite + Micrometals), "amidon", "all"
QString matLabel(const QString &m);
QStringList matsFor(const QString &db, const QString &kind = QString(), bool ironFirst = false);
QString coreLabel(const Core &c);
QVariantMap wireInfo(const Wire &w);                         // {id, label, diaMM, odMM, insul}
QString nearestWire(const std::vector<Wire> &list, double diaMM, const QString &preferInsul, bool wiresOnly = true);

/* Database + material + core pickers in one row, plus a "Core DB…" button. */
class CoreSelector : public QWidget
{
    Q_OBJECT
public:
    explicit CoreSelector(const QString &kindFilter = QString(), QWidget *parent = nullptr);   // kindFilter "" | "iron" | "ferrite"
    QString coreId() const;
    void setCoreId(const QString &id);
    QString db() const;
    void setDb(const QString &db);
    FromMarker *marker() const { return m_mark; }
signals:
    void coreChanged(const QString &id);
    void browseRequested();
private:
    void fillMats(); void fillCores(const QString &sel);
    QString m_kind;
    QComboBox *m_db, *m_mat, *m_core;
    FromMarker *m_mark;
    bool m_internal = false;
    double m_lastOd = 61;
};
}
#endif
