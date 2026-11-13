/* balundialog.h - KE9ESU fork of Coil64: Balun / Unun designer window (1:1 choke, 4:1 Guanella/Ruthroff,
   9:1 Ruthroff trifilar, 9:1 autotransformer, 49:1 / 64:1 EFHW with compensation cap).
   Port of the KE9ESU Balun, Unun & Trap Designer v1.3.1 (MIT origin: hermes98761234/balun-unun-designer, V. Kopytsia).
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_BALUNDIALOG_H
#define KE9ESU_BALUNDIALOG_H
#include "widgets.h"
#include "balun_core.h"
class QComboBox; class QSpinBox; class QDoubleSpinBox; class QLabel; class QTableWidget;
namespace ke9esu {
class CoreSelector; class DesignView;
class BalunDialog : public Ke9esuDialog
{
    Q_OBJECT
public:
    explicit BalunDialog(QWidget *parent = nullptr);
    QJsonObject saveState() const override;
    void loadState(const QJsonObject &o) override;
    void pullKey(const QString &key) override;
    void applyPreset(const QString &id, const QString &from = QString());
    const DesignResult &result() const { return m_d; }
protected:
    void fillUseIn(QMenu *m) override;
    void fillPull(QMenu *m) override;
    QString sheetTitle() const override;
    QString sheetHtml() const override;
    QMap<QString, QImage> sheetImages() const override;
    void acceptToCoil64() override;
private slots:
    void recalc();
    void edited();
    void projectChanged(const QString &key, const QString &source);
private:
    BalunInput input() const;
    void setInput(const BalunInput &in, bool presetValues = false);   // presetValues: the preset's power/SWR/duty override the project values
    void fillConductors(const QString &sel);
    void publishCore(); void publishWire();
    QComboBox *m_preset, *m_top, *m_cond, *m_cs;
    CoreSelector *m_core;
    QSpinBox *m_stack, *m_turns;
    QDoubleSpinBox *m_fMin, *m_fMax, *m_xl, *m_target, *m_cm, *m_csPF;
    LenSpin *m_lead;
    ProjectBox *m_proj;
    FromMarker *m_wireMark, *m_presetMark;
    QLabel *m_title, *m_hint, *m_bandsHint, *m_capNote, *m_plotHint;
    Cards *m_cards; ChecksBar *m_checks;
    DesignView *m_diag, *m_plot;
    QTableWidget *m_pf, *m_parts; QLabel *m_steps;
    Rollup *m_rPf, *m_rParts, *m_rSteps;
    QWidget *m_xfOnly, *m_chokeOnly;
    DesignResult m_d;
    bool m_internal = false;
};
}
#endif
