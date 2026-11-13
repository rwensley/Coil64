/* project.h - KE9ESU fork of Coil64: shared "RF project" state that links every RF transformers & traps window and the
   Coil64 main window. Values carry the window they came from, so a pulled-in value can show "from <window>".
   The whole project (shared values + every dialog's inputs) saves/opens as one JSON file (File menu).
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_PROJECT_H
#define KE9ESU_PROJECT_H

#include <QObject>
#include <QVariant>
#include <QJsonObject>
#include <QMap>

namespace ke9esu {

/* Window names used as value sources and "Use in" targets. */
namespace Win {
static const char *const Balun = "Balun/Unun designer";
static const char *const Eflw = "Long-wire analyzer";
static const char *const Coax = "Coax trap";
static const char *const Lc = "LC trap";
static const char *const CoreDb = "Core database";
static const char *const Combiner = "Standard-part combination";
static const char *const Coil64 = "Coil64";
static const char *const Coil64Lc = "Coil64 LC circuit";
static const char *const Coil64Coil = "Coil64 one-layer coil";
static const char *const Coil64Ferrite = "Coil64 ferrite toroid";
static const char *const Project = "Project";
}

/* Shared value keys. */
namespace Key {
static const char *const L = "L_uH";               // double, µH
static const char *const C = "C_pF";               // double, pF
static const char *const F = "f_MHz";              // double, MHz
static const char *const Core = "coreId";          // QString (core DB id) ; plus "stack" int
static const char *const Stack = "stack";
static const char *const Wire = "wire";            // QVariantMap {diaMM, odMM, label, insul, id}
static const char *const Power = "powerW";          // project level, used by every heating / voltage check
static const char *const ZSys = "zLow";             // system (transmitter side) impedance, Ω
static const char *const Vswr = "vswr";
static const char *const ZEnd = "zEnd";             // impedance at the trap / antenna end, Ω
static const char *const Duty = "duty";             // ssb | cw | digital | custom
static const char *const DutyPct = "dutyPercent";
static const char *const Ambient = "ambientC";
static const char *const EflwLen = "eflwLengthFt";
static const char *const ProblemBands = "problemBands";   // QStringList
static const char *const Recommend = "recommendedPreset"; // QString preset id
static const char *const TrapReq = "trapRequest";         // QVariantMap {band, fMHz, distanceFt, text}
static const char *const Coil = "coil64Coil";             // QVariantMap {N, D, d, k, p, L, f} (mm, µH, MHz)
static const char *const CombReq = "combinerRequest";      // QVariantMap {type 'R'|'C'|'L', value (SI), label}
}

class Project : public QObject
{
    Q_OBJECT
public:
    static Project *instance();

    void set(const QString &key, const QVariant &value, const QString &source);
    QVariant get(const QString &key, const QVariant &def = QVariant()) const { return m_values.value(key, def); }
    double num(const QString &key, double def = 0) const;
    QString source(const QString &key) const { return m_sources.value(key); }
    bool has(const QString &key) const { return m_values.contains(key); }

    void setState(const QString &window, const QJsonObject &o);
    QJsonObject state(const QString &window) const { return m_states.value(window); }

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &o, QString *error = nullptr);
    bool saveFile(const QString &path, QString *error = nullptr);
    bool openFile(const QString &path, QString *error = nullptr);
    QString filePath() const { return m_path; }

    /* Ask the bridge to raise a window (and optionally pull a key there). */
    void requestWindow(const QString &window, const QString &pullKey = QString()) { emit windowRequested(window, pullKey); }
    /* Ask the Coil64 main window to do something: "inductance", "ferriteCore", "lc", "wire". */
    void requestCoil64(const QString &what, const QVariantMap &args) { emit coil64Requested(what, args); }

    /* Coil64 Preferences mirrored for the RF windows (updated live by the bridge). */
    struct Units { bool awg = true; QString len = "mm"; double lenMul = 1; QString indUnit = "µH"; double indMul = 1; QString capUnit = "pF"; double capMul = 1; };
    const Units &units() const { return m_units; }
    void setUnits(const Units &u);
    QString lenUnit() const { return m_units.len; }
    double toDispLen(double mm) const { return mm / m_units.lenMul; }
    double fromDispLen(double v) const { return v * m_units.lenMul; }
    int lenDecimals() const;

signals:
    void changed(const QString &key, const QString &source);
    void loaded();                         // a project file was opened: every window reloads its state
    void aboutToSave();                    // windows push their inputs with setState()
    void unitsChanged();
    void windowRequested(const QString &window, const QString &pullKey);
    void coil64Requested(const QString &what, const QVariantMap &args);

private:
    explicit Project(QObject *parent = nullptr) : QObject(parent) {}
    QVariantMap m_values;
    QMap<QString, QString> m_sources;
    QMap<QString, QJsonObject> m_states;
    QString m_path;
    Units m_units;
};

} // namespace ke9esu
#endif
