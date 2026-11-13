/* trapdialogs.h - KE9ESU fork of Coil64: coax trap and LC trap windows.
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_TRAPDIALOGS_H
#define KE9ESU_TRAPDIALOGS_H
#include "widgets.h"
#include "trap_core.h"
class QDoubleSpinBox; class QComboBox; class QSpinBox; class QLabel; class QTableWidget; class QRadioButton;
namespace ke9esu {
class CoreSelector;

/* Band picker: band combo + "centre / just below the band" + frequency spin box with a "from" marker. */
class FreqPicker : public QWidget
{
    Q_OBJECT
public:
    explicit FreqPicker(QWidget *parent = nullptr);
    double mhz() const;
    void setMhz(double f, const QString &from = QString());
    FromMarker *marker() const { return m_mark; }
signals:
    void changed();
private:
    QComboBox *m_band, *m_place; QDoubleSpinBox *m_f; FromMarker *m_mark; bool m_internal = false;
};

class PaperView;
class TrapTables : public QWidget   // checks, band reactance, steps, parts (shared by both trap windows)
{
    Q_OBJECT
public:
    TrapTables(const QString &prefix, QWidget *parent = nullptr);
    void set(const std::vector<Check> &checks, const std::vector<TrapBand> &bands, const QStringList &steps, const std::vector<Part> &parts);
    Cards *cards; ChecksBar *checks; PaperView *drawing;
private:
    QTableWidget *m_bands, *m_parts; QLabel *m_steps; Rollup *m_rb, *m_rs, *m_rp;
};

class CoaxTrapDialog : public Ke9esuDialog
{
    Q_OBJECT
public:
    explicit CoaxTrapDialog(QWidget *parent = nullptr);
    QJsonObject saveState() const override;
    void loadState(const QJsonObject &o) override;
    void pullKey(const QString &key) override;
protected:
    void themeChanged() override { recalc(); }
    void fillUseIn(QMenu *m) override;
    void fillPull(QMenu *m) override;
    QString sheetTitle() const override;
    QString sheetHtml() const override;
    QMap<QString, QImage> sheetImages() const override;
    void acceptToCoil64() override;
private slots:
    void recalc();
private:
    CoaxTrapInput input() const;
    void setInput(const CoaxTrapInput &i);
    FreqPicker *m_f; QComboBox *m_coax, *m_form, *m_model; LenSpin *m_od, *m_spacing, *m_lead, *m_trim;
    QDoubleSpinBox *m_pf, *m_vf, *m_cOd, *m_z0; QWidget *m_custom; ProjectBox *m_proj; QLabel *m_req;
    TrapTables *m_t; CoaxTrapResult m_r; bool m_internal = false;
};

class LcTrapDialog : public Ke9esuDialog
{
    Q_OBJECT
public:
    explicit LcTrapDialog(QWidget *parent = nullptr);
    QJsonObject saveState() const override;
    void loadState(const QJsonObject &o) override;
    void pullKey(const QString &key) override;
protected:
    void themeChanged() override { recalc(); }
    void fillUseIn(QMenu *m) override;
    void fillPull(QMenu *m) override;
    QString sheetTitle() const override;
    QString sheetHtml() const override;
    QMap<QString, QImage> sheetImages() const override;
    void acceptToCoil64() override;
private slots:
    void recalc();
private:
    LcTrapInput input() const;
    void setInput(const LcTrapInput &i);
    FreqPicker *m_f; QComboBox *m_mode, *m_snap, *m_coreType, *m_form, *m_wire, *m_cap;
    QDoubleSpinBox *m_x, *m_l, *m_c; QSpinBox *m_stack; LenSpin *m_dia, *m_pitch, *m_lead;
    CoreSelector *m_core; QWidget *m_air, *m_tor; ProjectBox *m_proj; QLabel *m_req, *m_coilNote;
    FromMarker *m_lMark, *m_cMark, *m_wireMark;
    TrapTables *m_t; LcTrapResult m_r; bool m_internal = false;
};
}
#endif
