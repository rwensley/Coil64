/* project.cpp - see project.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "project.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDateTime>

namespace ke9esu {

Project *Project::instance()
{
    static Project *p = new Project(qApp);
    return p;
}

void Project::set(const QString &key, const QVariant &value, const QString &source)
{
    if (m_values.value(key) == value && m_sources.value(key) == source) return;
    m_values.insert(key, value);
    m_sources.insert(key, source);
    emit changed(key, source);
}

double Project::num(const QString &key, double def) const
{
    bool ok = false; double v = m_values.value(key).toDouble(&ok);
    return ok ? v : def;
}

void Project::setState(const QString &window, const QJsonObject &o) { m_states.insert(window, o); }

void Project::setUnits(const Units &u)
{
    m_units = u;
    emit unitsChanged();
}

int Project::lenDecimals() const
{
    if (m_units.len == "mil") return 0;
    if (m_units.len == "in" || m_units.len == "cm") return 3;
    return 2;
}

QJsonObject Project::toJson() const
{
    const_cast<Project *>(this)->aboutToSave();
    QJsonObject o, vals, states;
    for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        QJsonObject v; v.insert("value", QJsonValue::fromVariant(it.value())); v.insert("from", m_sources.value(it.key()));
        vals.insert(it.key(), v);
    }
    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) states.insert(it.key(), it.value());
    o.insert("format", "KE9ESU Coil64 RF project");
    o.insert("version", 1);
    o.insert("app", QCoreApplication::applicationName() + " " + QCoreApplication::applicationVersion());
    o.insert("saved", QDateTime::currentDateTime().toString(Qt::ISODate));
    o.insert("shared", vals);
    o.insert("windows", states);
    return o;
}

bool Project::fromJson(const QJsonObject &o, QString *error)
{
    if (o.value("format").toString() != "KE9ESU Coil64 RF project") { if (error) *error = tr("Not a KE9ESU RF project file."); return false; }
    QJsonObject vals = o.value("shared").toObject(), states = o.value("windows").toObject();
    m_states.clear();
    for (const QString &k : states.keys()) m_states.insert(k, states.value(k).toObject());
    for (const QString &k : vals.keys()) {
        QJsonObject v = vals.value(k).toObject();
        m_values.insert(k, v.value("value").toVariant());
        m_sources.insert(k, v.value("from").toString());
    }
    emit loaded();
    for (const QString &k : vals.keys()) emit changed(k, m_sources.value(k));
    return true;
}

bool Project::saveFile(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { if (error) *error = f.errorString(); return false; }
    f.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    m_path = path;
    return true;
}

bool Project::openFile(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (error) *error = f.errorString(); return false; }
    QJsonParseError pe; QJsonDocument d = QJsonDocument::fromJson(f.readAll(), &pe);
    if (d.isNull()) { if (error) *error = pe.errorString(); return false; }
    if (!fromJson(d.object(), error)) return false;
    m_path = path;
    return true;
}

} // namespace ke9esu
