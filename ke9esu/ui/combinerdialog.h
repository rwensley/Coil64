/* combinerdialog.h - KE9ESU fork of Coil64: "Find standard-part combination" - series / parallel / mixed networks of
   E6...E192 (or junk-box) resistors, capacitors or inductors, up to 6 parts, for a needed R, C or L.
   Port of KE9ESU Component Combiner. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_COMBINERDIALOG_H
#define KE9ESU_COMBINERDIALOG_H
#include "widgets.h"
#include "combiner_core.h"
#include <atomic>
#include <QList>
class QComboBox; class QLineEdit; class QSpinBox; class QDoubleSpinBox; class QTableWidget; class QLabel; class QPushButton; class QProgressBar; class QPlainTextEdit; class QSplitter; class QGridLayout; class QScrollArea;
namespace ke9esu {
class CombinerDialog : public Ke9esuDialog
{
    Q_OBJECT
public:
    explicit CombinerDialog(QWidget *parent = nullptr);
    ~CombinerDialog() override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject &o) override;
    void pullKey(const QString &key) override;
    void runSync();   // for tests / screenshots
    void selectResult(int row);                    // screenshots
    void setInputs(QChar type, const QString &target, const QString &topo, int maxParts);   // screenshots
    const std::vector<cc::Result> &results() const { return m_sorted; }
    QChar resultType() const { return m_solType; }
protected:
    void themeChanged() override { m_cols = 0; relayoutForm(); fitRunButton(); showParsed(); showSeriesInfo(); showResults(); sizeDetail(); }
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void fillUseIn(QMenu *m) override;
    void fillPull(QMenu *m) override;
    QString sheetHtml() const override;
    QMap<QString, QImage> sheetImages() const override;
private slots:
    void run();
    void showResults();
private:
    cc::Params params(QString *err) const;
    void relayoutForm();
    void fitRunButton();
    void showParsed();
    void showSeriesInfo();
    void showLegend();
    void showDetail();
    void sizeDetail();
    QString caption(const cc::Result &x) const;
    QString spreadText(const cc::Result &x) const;
    QComboBox *m_type, *m_series, *m_topo, *m_sort; QLineEdit *m_target; QSpinBox *m_max; QDoubleSpinBox *m_tol; QPlainTextEdit *m_custom;
    QPushButton *m_run; QProgressBar *m_prog; QLabel *m_status; QTableWidget *m_t; FromMarker *m_mark;
    QLabel *m_parsed, *m_seriesHint, *m_customInfo, *m_legend, *m_detailHead; QWidget *m_detail; QSplitter *m_split;
    QGridLayout *m_grid; QList<QWidget *> m_cells; QWidget *m_customCell; int m_cols = 0; bool m_sized = false; QWidget *m_inputs; QScrollArea *m_inScroll;
    QString m_solSeries; int m_solCustom = 0; int m_solMax = 3; double m_solTarget = 0;
    cc::Solution m_sol; std::vector<cc::Result> m_sorted; QChar m_solType = 'R';
    std::atomic<bool> m_abort{false}; QThread *m_thread = nullptr;
};
}
#endif
