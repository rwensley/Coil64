/* widgets.h - KE9ESU fork of Coil64: shared widgets of the RF transformers & traps windows
   (checks summary bar, roll-up tables, result cards, "from <window>" markers, length spin boxes that follow the
   Coil64 length unit, the shared project box, build sheets and the common dialog base with "Use in…" / "Pull…").
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_WIDGETS_H
#define KE9ESU_WIDGETS_H

#include <QDialog>
#include <QWidget>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QToolButton>
#include <QCheckBox>
#include <QComboBox>
#include <QTableWidget>
#include <QJsonObject>
#include <QImage>
#include <functional>
#include <vector>
#include "balun_core.h"

class QVBoxLayout;
class QGridLayout;
class QHBoxLayout;
class QMenu;
#include <QSettings>

namespace ke9esu {

QString f(double v, int d = 1);                // JS toFixed
QString fmtOhm(double v);
QString esc(const QString &s);
QString sevTag(const QString &status);         // FAIL / CHECK / NOTE / OK
QColor sevColor(const QString &status);
void applyCoreUnits();                         // map Coil64 Preferences onto the core text units
QSettings *ke9esuSettings();                   // Coil64's own settings file (portable aware), group "KE9ESU"

/* Checks summary bar: one chip per severity with its count; ticking a chip lists that severity's items.
   Hidden by default; the choice is shared by every window and remembered (QSettings KE9ESU/checks/show_<sev>). */
class ChecksBar : public QWidget
{
    Q_OBJECT
public:
    explicit ChecksBar(QWidget *parent = nullptr);
    void setChecks(const std::vector<Check> &checks);
    static QString sheetHtml(const std::vector<Check> &checks);   // counts + every FAIL/CHECK item (print)
    static QString summaryText(const std::vector<Check> &checks);
private slots:
    void reload();
private:
    std::vector<Check> m_checks;
    QCheckBox *m_box[4];
    QLabel *m_list;
};

/* Collapsible table section: collapsed by default, one-line summary in the header,
   open state per table remembered (QSettings KE9ESU/tables/<key>/open). */
class Rollup : public QWidget
{
    Q_OBJECT
public:
    Rollup(const QString &key, const QString &title, QWidget *body, QWidget *parent = nullptr);
    void setSummary(const QString &html);
    bool isOpen() const;
    void setOpen(bool open);
private:
    QString m_key;
    QToolButton *m_btn;
    QLabel *m_sum;
    QWidget *m_body;
};

/* Result cards (key / value tiles). */
class Cards : public QWidget
{
    Q_OBJECT
public:
    explicit Cards(int columns = 4, QWidget *parent = nullptr);
    void setCards(const QList<QPair<QString, QString>> &cards);
protected:
    void resizeEvent(QResizeEvent *e) override;   // 2…columns tiles per row, whatever fits
private:
    void relayout();
    QGridLayout *m_grid;
    int m_cols, m_used = 0;
    QList<QWidget *> m_tiles;
};

/* Small italic marker next to a field: "◂ from Long-wire analyzer". Cleared when the user edits the field. */
class FromMarker : public QLabel
{
    Q_OBJECT
public:
    explicit FromMarker(QWidget *parent = nullptr);
    void setFrom(const QString &window);
    void clear();
    QString from() const { return m_from; }
private:
    QString m_from;
};

/* Length in the Coil64 length unit (mm, cm, in, mil); value kept in mm, follows Preferences live. */
class LenSpin : public QDoubleSpinBox
{
    Q_OBJECT
public:
    LenSpin(double minMM, double maxMM, double valueMM, QWidget *parent = nullptr);
    double valueMM() const;
    void setValueMM(double mm);
signals:
    void changedMM(double mm);
private slots:
    void unitsChanged();
private:
    double m_minMM, m_maxMM, m_mm;
    bool m_internal = false;
};

/* Project-level power / impedance / SWR / duty / ambient: set once, used by every window. */
class ProjectBox : public QGroupBox
{
    Q_OBJECT
public:
    enum Field { Power = 1, ZSys = 2, Vswr = 4, Duty = 8, ZEnd = 16, Ambient = 32 };
    ProjectBox(const QString &window, int fields, QWidget *parent = nullptr);
    double power() const;
    double zSys() const;
    double vswr() const;
    QString duty() const;
    double dutyPercent() const;
    double zEnd() const;
    double ambient() const;
    void setValues(double power, double zSys, double vswr, const QString &duty, double dutyPct, double zEnd, double ambient, bool publish);
signals:
    void edited();
private slots:
    void projectChanged(const QString &key, const QString &source);
private:
    void publish(const char *key, const QVariant &v);
    QString m_window;
    QDoubleSpinBox *m_power, *m_zSys, *m_vswr, *m_dutyPct, *m_zEnd, *m_ambient;
    QComboBox *m_duty;
    QMap<QString, FromMarker *> m_marks;
    bool m_internal = false;
};

/* Build sheet preview with Print… and Save as PDF…. */
class SheetDialog : public QDialog
{
    Q_OBJECT
public:
    SheetDialog(const QString &title, const QString &html, const QMap<QString, QImage> &images, QWidget *parent = nullptr);
    static bool savePdf(const QString &html, const QMap<QString, QImage> &images, const QString &path);
private:
    QString m_html, m_title;
    QMap<QString, QImage> m_images;
};
QString sheetCss();
QString partsTableHtml(const std::vector<Part> &parts);
QString stepsHtml(const QStringList &steps);

/* Base of every RF window: modeless, single instance, remembers geometry; bottom row with
   Pull… / Use in… menus, Build sheet…, an optional Accept (send to Coil64) and Close. */
class Ke9esuDialog : public QDialog
{
    Q_OBJECT
public:
    Ke9esuDialog(const QString &window, QWidget *parent = nullptr);
    QString window() const { return m_window; }
    bool exportSheetPdf(const QString &path) const { return SheetDialog::savePdf(sheetHtml(), sheetImages(), path); }
    virtual QJsonObject saveState() const = 0;
    virtual void loadState(const QJsonObject &o) = 0;
    virtual void pullKey(const QString &key) { Q_UNUSED(key) }     // take a shared value (shows "from <window>")
protected:
    QWidget *body() const { return m_body; }
    void finishLayout(bool withAccept, const QString &acceptText = QString());
    virtual void fillUseIn(QMenu *m) { Q_UNUSED(m) }
    virtual void fillPull(QMenu *m) { Q_UNUSED(m) }
    virtual QString sheetTitle() const { return m_window; }
    virtual QString sheetHtml() const { return QString(); }
    virtual QMap<QString, QImage> sheetImages() const { return {}; }
    virtual void acceptToCoil64() {}
    /* Pull menu entry for a shared key, enabled only if a value exists from another window. */
    void addPull(QMenu *m, const QString &key, const QString &text);
    void closeEvent(QCloseEvent *e) override;
    void showEvent(QShowEvent *e) override;
    QToolButton *m_useIn = nullptr, *m_pull = nullptr;
private:
    QString m_window;
    QWidget *m_body;
};

} // namespace ke9esu
#endif
