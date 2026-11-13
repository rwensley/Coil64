/* eflwdialog.h - KE9ESU fork of Coil64: end-fed long-wire analyzer (124 ft default, 160-6 m, 100 W): band-by-band
   impedance tendency, avoid zones / good lengths, recommended transformer and trap suggestions that feed the other windows.
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_EFLWDIALOG_H
#define KE9ESU_EFLWDIALOG_H
#include "widgets.h"
#include "eflw_core.h"
class QDoubleSpinBox; class QComboBox; class QCheckBox; class QLabel; class QTableWidget; class QPushButton;
namespace ke9esu {
class EflwDialog : public Ke9esuDialog
{
    Q_OBJECT
public:
    explicit EflwDialog(QWidget *parent = nullptr);
    QJsonObject saveState() const override;
    void loadState(const QJsonObject &o) override;
protected:
    void fillUseIn(QMenu *m) override;
    QString sheetHtml() const override;
    QString sheetTitle() const override;
private slots:
    void recalc();
private:
    double lengthFt() const;
    void sendTrap(int row, const QString &target);
    QDoubleSpinBox *m_len, *m_k, *m_tol, *m_min, *m_max;
    QComboBox *m_unit;
    QCheckBox *m_6m, *m_60m;
    ProjectBox *m_proj;
    QLabel *m_summary, *m_good, *m_reco, *m_zonesLbl;
    QPushButton *m_recoBtn;
    QTableWidget *m_table, *m_traps;
    Rollup *m_rTable, *m_rTraps, *m_rZones;
    std::vector<EflwRow> m_rows; TransformerAdvice m_adv; std::vector<TrapSuggestion> m_sugg;
    bool m_internal = false;
};
}
#endif
