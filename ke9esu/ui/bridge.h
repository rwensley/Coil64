/* bridge.h - KE9ESU fork of Coil64: glue between the Coil64 main window and the RF transformers & traps windows.
   Adds the "RF transformers & traps" menu and the File > RF project actions, mirrors Coil64 Preferences (units) live,
   turns "Use in Coil64" requests into Coil64 inputs (switching to the matching calculator) and publishes Coil64
   results (LC circuit, one-layer coil, ferrite toroid) into the shared project.
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. Coil64 is (C) Valery Kustarev, GPL-3.0. */
#ifndef KE9ESU_BRIDGE_H
#define KE9ESU_BRIDGE_H
#include <QObject>
#include <QMap>
#include <QPointer>
#include <QVariantMap>
class MainWindow; class QAction; class QMenu; class QTimer;
struct _OptionStruct;
namespace ke9esu {
class Ke9esuDialog;
class Bridge : public QObject
{
    Q_OBJECT
public:
    explicit Bridge(MainWindow *mw);
    void captureOutput(const QString &resultHtml);       // called from MainWindow::showOutput
    void optionsChanged();                               // called when Coil64 Preferences change
    Ke9esuDialog *openWindow(const QString &name, const QString &pullKey = QString());
    static QStringList windowNames();
    void themeApplied();                                  // MainWindow::applyGuiStyle() finished
    void stepFont(int dir);                               // A- (dir -1), A+ (+1), reset (0)
    void screenshots(const QString &dir);                 // KE9ESU_SHOTS=<dir>
private slots:
    void coil64Request(const QString &what, const QVariantMap &args);
    void projectChanged(const QString &key, const QString &source);
    void openProject();
    void saveProject(bool as);
private:
    void applyFerrite(const QString &coreId, int stack);
    void applyWire(const QVariantMap &w);
    void refreshView(int tab, int row);
    void status(const QString &s);
    bool ferriteShowing(int tab) const;
    MainWindow *m;
    QMap<QString, QPointer<Ke9esuDialog>> m_wins;
    QAction *m_live = nullptr, *m_capture = nullptr;
    QList<QAction *> m_themeActs;
    QTimer *m_sysPoll = nullptr; bool m_sysDark = false;
    void buildViewMenu(QMenu *before);
    bool m_applying = false;
};
}
#endif
