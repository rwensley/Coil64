/* coredbdialog.h - KE9ESU fork of Coil64: core database browser (Fair-Rite + Micrometals datasheet cores, Amidon catalogue
   with corrections). Selecting a core shares it with the balun designer, LC trap and Coil64's ferrite toroid calculator.
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_COREDBDIALOG_H
#define KE9ESU_COREDBDIALOG_H
#include "widgets.h"
#include "balun_core.h"
class QComboBox; class QLineEdit; class QTableWidget; class QLabel;
namespace ke9esu {
class CoreDbDialog : public Ke9esuDialog
{
    Q_OBJECT
public:
    explicit CoreDbDialog(QWidget *parent = nullptr);
    QJsonObject saveState() const override;
    void loadState(const QJsonObject &o) override;
    void pullKey(const QString &key) override;
    QString selectedId() const;
protected:
    void themeChanged() override { fill(); }
    void fillUseIn(QMenu *m) override;
    void fillPull(QMenu *m) override;
    QString sheetHtml() const override;
    void acceptToCoil64() override;
private slots:
    void fill();
private:
    void select(const QString &id);
    QComboBox *m_db, *m_kind, *m_mat; QLineEdit *m_search; QTableWidget *m_t; QLabel *m_info; FromMarker *m_mark;
    bool m_internal = false;
};
QString coreSourceUrl(const Core &c);
}
#endif
