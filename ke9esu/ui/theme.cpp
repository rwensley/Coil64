/* theme.cpp - see theme.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "theme.h"
#include "system_functions.h"
#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QFile>
#include <QLayout>
#include <QDialog>
#include <QTimer>
#include <QPainter>
#include <QImage>
#include <QDir>
#include <QSettings>

namespace ke9esu {

static ThemeColors g_theme = lightColors();
static bool g_captured = false;
static QString g_origStyle;
static QPalette g_origPalette;
static bool g_systemDarkAtStart = false;

static void capture()
{
    if (g_captured || !qApp) return;
    g_captured = true;
    g_origStyle = QApplication::style()->objectName();
    g_origPalette = QApplication::palette();
    g_systemDarkAtStart = g_origPalette.color(QPalette::Window).lightness() < 128;
}

ThemeColors lightColors()
{
    ThemeColors c; c.dark = false;
    c.bg = QColor("#f5f6f8"); c.panel = QColor("#ffffff"); c.ink = QColor("#16202b"); c.muted = QColor("#5a6675"); c.line = QColor("#d5dbe3");
    c.accent = QColor("#0b5cad"); c.ok = QColor("#1d7a33"); c.warn = QColor("#a86400"); c.fail = QColor("#b3261e"); c.info = QColor("#2b5f9e");
    c.grid = QColor("#e3e7ec"); c.band = QColor(11, 92, 173, 26); c.paper = QColor("#ffffff"); c.paperInk = QColor("#000000");
    return c;
}
ThemeColors darkColors()
{
    ThemeColors c; c.dark = true;
    c.bg = QColor("#12161c"); c.panel = QColor("#1b2129"); c.ink = QColor("#e6ebf1"); c.muted = QColor("#9aa7b6"); c.line = QColor("#34404e");
    c.accent = QColor("#5aa9ff"); c.ok = QColor("#5cc878"); c.warn = QColor("#f0b04a"); c.fail = QColor("#ff7a70"); c.info = QColor("#7fb3ff");
    c.grid = QColor("#2a333e"); c.band = QColor(90, 169, 255, 36); c.paper = QColor("#e4e8ee"); c.paperInk = QColor("#000000");
    return c;
}
const ThemeColors &theme() { return g_theme; }
bool themeIsDark() { return g_theme.dark; }

bool systemPrefersDark()
{
    capture();
    QByteArray env = qgetenv("KE9ESU_SYSTEM_THEME");      // override for tests / screenshots: "dark" or "light"
    if (!env.isEmpty()) return env.toLower() == "dark";
#ifdef Q_OS_WIN
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
    if (reg.contains("AppsUseLightTheme")) return reg.value("AppsUseLightTheme", 1).toInt() == 0;
#endif
    return g_systemDarkAtStart;
}

QString themeName(int s)
{
    switch (s) {
    case _DefaultStyle: return QObject::tr("Coil64 Default");
    case _SunnyStyle: return QObject::tr("Coil64 Sunny");
    case _DarkStyle: return QObject::tr("Coil64 Dark");
    case _Ke9esuLight: return QObject::tr("KE9ESU Light");
    case _Ke9esuDark: return QObject::tr("KE9ESU Dark");
    case _Ke9esuSystem: return QObject::tr("KE9ESU System (follow Windows)");
    default: return QString();
    }
}

/* check-mark image for styled indicators (QSS needs an image once ::indicator is styled); generated per colour */
static QString checkImage(const QColor &col)
{
    QString dir = QDir::tempPath() + "/ke9esu-qss"; QDir().mkpath(dir);
    QString fn = dir + "/check-" + col.name().mid(1) + ".png";
    if (!QFile::exists(fn)) {
        QImage img(28, 28, QImage::Format_ARGB32); img.fill(Qt::transparent);
        QPainter p(&img); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(col, 3.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({QPointF(6.5, 14.5), QPointF(11.5, 19.5), QPointF(21.5, 8.5)})); p.end();
        img.save(fn);
    }
    return fn;
}

static QString arrowImage(const QColor &col, bool up)
{
    QString dir = QDir::tempPath() + "/ke9esu-qss"; QDir().mkpath(dir);
    QString fn = dir + QString("/arrow-%1-%2.png").arg(up ? "up" : "down", col.name().mid(1));
    if (!QFile::exists(fn)) {
        QImage img(20, 12, QImage::Format_ARGB32); img.fill(Qt::transparent);
        QPainter p(&img); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(col, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (up) p.drawPolyline(QPolygonF({QPointF(3, 9.5), QPointF(10, 2.5), QPointF(17, 9.5)}));
        else p.drawPolyline(QPolygonF({QPointF(3, 2.5), QPointF(10, 9.5), QPointF(17, 2.5)}));
        p.end(); img.save(fn);
    }
    return fn;
}

static QString fill(QString s, const ThemeColors &c)
{
    if (s.contains("@uparrow")) s.replace("@uparrow", arrowImage(c.muted, true));
    if (s.contains("@downarrow")) s.replace("@downarrow", arrowImage(c.muted, false));
    if (s.contains("@checkimg")) s.replace("@checkimg", checkImage(c.dark ? QColor("#0b1520") : QColor("#ffffff")));
    auto rgba = [](const QColor &q) { return QString("rgba(%1,%2,%3,%4)").arg(q.red()).arg(q.green()).arg(q.blue()).arg(q.alpha()); };
    s.replace("@bg", c.bg.name()).replace("@panel", c.panel.name()).replace("@ink", c.ink.name()).replace("@muted", c.muted.name())
     .replace("@line", c.line.name()).replace("@accent", c.accent.name()).replace("@ok", c.ok.name()).replace("@warn", c.warn.name())
     .replace("@fail", c.fail.name()).replace("@info", c.info.name()).replace("@grid", c.grid.name()).replace("@band", rgba(c.band))
     .replace("@paper", c.paper.name()).replace("@onaccent", c.dark ? "#0b1520" : "#ffffff");
    return s;
}

QString ke9esuWidgetsQss(const ThemeColors &c)
{
    return fill(QStringLiteral(R"QSS(
/* ---- KE9ESU RF windows */
QFrame#ke9esuCard { background: @bg; border: 1px solid @line; border-radius: 8px; }
QLabel#ke9esuCardKey { color: @muted; background: transparent; }
QLabel#ke9esuCardVal { color: @ink; background: transparent; }
QCheckBox[ke9esuSev] { font-weight: bold; color: @ink; background: @bg; border: 1px solid @line; border-left: 5px solid @line; border-radius: 5px; padding: 3px 9px 3px 6px; }
QCheckBox[ke9esuSev="fail"] { border-left-color: @fail; }
QCheckBox[ke9esuSev="warn"] { border-left-color: @warn; }
QCheckBox[ke9esuSev="info"] { border-left-color: @info; }
QCheckBox[ke9esuSev="ok"] { border-left-color: @ok; }
QCheckBox[ke9esuZero="true"] { color: @muted; border-color: @grid; }
QToolButton#ke9esuRoll { color: @ink; background: transparent; border: none; padding: 3px 4px; font-weight: bold; }
QLabel#ke9esuFrom { color: @accent; font-style: italic; background: transparent; }
QLabel#ke9esuHint, QLabel#ke9esuMuted { color: @muted; background: transparent; }
QLabel#ke9esuNote, QFrame#ke9esuNote { color: @ink; background: @bg; border: 1px dashed @line; border-radius: 6px; padding: 6px 8px; }
QLabel#ke9esuReq { color: @info; background: transparent; }
QFrame#ke9esuRollHead { background: @bg; border: 1px solid @line; border-radius: 6px; }
QFrame#ke9esuRollHead:hover { border-color: @accent; }
QLabel#ke9esuRollSum { color: @muted; background: transparent; }
QPushButton#ke9esuPrimary { background: @accent; color: @onaccent; border: 1px solid @accent; font-weight: bold; }
QToolButton#ke9esuViewBtn { min-width: 1.8em; border: 1px solid @line; border-radius: 5px; padding: 2px 6px; }
QLabel#ke9esuTitle { background: transparent; }
QWidget#ke9esuDiagram { background: @paper; border: 1px solid @line; border-radius: 8px; }
)QSS"), c);
}

QString themeQss(const ThemeColors &c)
{
    return fill(QStringLiteral(R"QSS(
QWidget { color: @ink; }
QMainWindow, QDialog, QDockWidget, QScrollArea > QWidget > QWidget { background: @bg; }
QScrollArea { background: @bg; border: none; }
QGroupBox { background: @panel; border: 1px solid @line; border-radius: 8px; margin-top: 1.3em; padding: 10px 8px 8px 8px; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 4px; color: @muted; font-weight: bold; }
QLabel { background: transparent; }
QLineEdit, QAbstractSpinBox, QComboBox, QPlainTextEdit, QTextEdit, QDateEdit {
    background: @bg; color: @ink; border: 1px solid @line; border-radius: 5px; padding: 3px 6px; min-height: 1.3em;
    selection-background-color: @accent; selection-color: @onaccent; }
QLineEdit:focus, QAbstractSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid @accent; }
QLineEdit:disabled, QAbstractSpinBox:disabled, QComboBox:disabled { color: @muted; background: @panel; border-color: @grid; }
QComboBox { padding-right: 22px; }
QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 20px; border: none; }
QComboBox QAbstractItemView { background: @panel; color: @ink; border: 1px solid @line; selection-background-color: @accent; selection-color: @onaccent; outline: 0; }
QAbstractSpinBox { padding-right: 18px; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { subcontrol-origin: border; width: 16px; border: none; background: transparent; }
QAbstractSpinBox::up-button { subcontrol-position: top right; }
QAbstractSpinBox::down-button { subcontrol-position: bottom right; }
QAbstractSpinBox::up-arrow { image: url(@uparrow); width: 9px; height: 6px; }
QAbstractSpinBox::down-arrow, QComboBox::down-arrow { image: url(@downarrow); width: 10px; height: 6px; }
QAbstractSpinBox::up-arrow:disabled, QAbstractSpinBox::down-arrow:disabled { image: none; }
QPushButton { background: @panel; color: @ink; border: 1px solid @line; border-radius: 5px; padding: 4px 12px; min-height: 1.3em; }
QPushButton:hover { border-color: @accent; }
QPushButton:pressed, QPushButton:checked { background: @accent; color: @onaccent; border-color: @accent; }
QPushButton:default { border: 1px solid @accent; }
QPushButton:disabled { color: @muted; border-color: @grid; }
QPushButton#pushButton_Calculate, QPushButton#pushButton_calculate { background: @accent; color: @onaccent; border-color: @accent; font-weight: bold; }
QPushButton#pushButton_Calculate:hover, QPushButton#pushButton_calculate:hover { background: @info; }
QToolButton { background: transparent; border: 1px solid transparent; border-radius: 5px; padding: 2px; }
QToolButton:hover { background: @panel; border-color: @line; }
QToolButton:checked { background: @band; border-color: @accent; }
QToolButton:pressed { background: @band; }
QTabWidget::pane { border: 1px solid @line; border-radius: 6px; background: @panel; top: -1px; }
QTabBar::tab { background: @bg; color: @muted; border: 1px solid @line; border-bottom: none; border-top-left-radius: 6px; border-top-right-radius: 6px; padding: 5px 12px; margin-right: 2px; }
QTabBar::tab:selected { background: @panel; color: @ink; border-top: 3px solid @accent; }
QTabBar::tab:hover:!selected { color: @ink; }
QMenuBar { background: @panel; border-bottom: 1px solid @line; }
QMenuBar::item { padding: 4px 10px; background: transparent; border-radius: 4px; }
QMenuBar::item:selected { background: @accent; color: @onaccent; }
QMenu { background: @panel; color: @ink; border: 1px solid @line; padding: 4px; }
QMenu::item { padding: 5px 44px 5px 26px; border-radius: 4px; }
QMenu::item:selected { background: @accent; color: @onaccent; }
QMenu::item:disabled { color: @muted; }
QMenu::separator { height: 1px; background: @line; margin: 4px 8px; }
QHeaderView { background: @bg; }
QHeaderView::section { background: @bg; color: @muted; border: none; border-bottom: 1px solid @line; border-right: 1px solid @grid; padding: 4px 6px; font-weight: bold; }
QTableView { background: @panel; alternate-background-color: @bg; gridline-color: @grid; border: 1px solid @line; border-radius: 6px;
    selection-background-color: @accent; selection-color: @onaccent; }
QTableView::item:selected { background: @accent; color: @onaccent; }
QTableView::item:selected:!active { background: @accent; color: @onaccent; }
QTableView QTableCornerButton::section { background: @bg; border: none; }
QListView, QTreeView { background: @panel; border: 1px solid @line; border-radius: 6px; outline: 0; }
QListView::item { padding: 2px; border-radius: 4px; }
QListView::item:selected, QTreeView::item:selected { background: @accent; color: @onaccent; }
QListView::item:hover:!selected { background: @band; }
QTextBrowser { background: @panel; color: @ink; border: 1px solid @line; border-radius: 6px; }
QScrollBar:vertical { background: transparent; width: 12px; margin: 0; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 0; }
QScrollBar::handle { background: @line; border-radius: 5px; margin: 2px; }
QScrollBar::handle:vertical { min-height: 24px; }
QScrollBar::handle:horizontal { min-width: 24px; }
QScrollBar::handle:hover { background: @muted; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; border: none; background: none; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
QSplitter::handle { background: @line; }
QSplitter::handle:horizontal { width: 3px; }
QSplitter::handle:vertical { height: 3px; }
QProgressBar { border: 1px solid @line; border-radius: 5px; background: @bg; text-align: center; color: @ink; }
QProgressBar::chunk { background: @accent; border-radius: 4px; }
QToolTip { background: @panel; color: @ink; border: 1px solid @line; padding: 4px; }
QStatusBar { background: @panel; border-top: 1px solid @line; color: @muted; }
QDockWidget::title { background: @panel; padding: 4px; }
QCheckBox, QRadioButton { spacing: 6px; background: transparent; }
QCheckBox::indicator, QGroupBox::indicator { width: 14px; height: 14px; border: 1px solid @muted; border-radius: 3px; background: @panel; }
QCheckBox::indicator:hover, QGroupBox::indicator:hover { border-color: @accent; }
QCheckBox::indicator:checked, QGroupBox::indicator:checked { background: @accent; border-color: @accent; image: url(@checkimg); }
QCheckBox::indicator:disabled { background: @grid; border-color: @line; }
QRadioButton::indicator { width: 14px; height: 14px; border: 1px solid @muted; border-radius: 8px; background: @panel; }
QRadioButton::indicator:hover { border-color: @accent; }
QRadioButton::indicator:checked { width: 8px; height: 8px; border: 4px solid @accent; border-radius: 8px; background: @onaccent; }
QLabel#image, QLabel#pict { background: @paper; border: 1px solid @line; border-radius: 8px; }
QFrame[frameShape="4"], QFrame[frameShape="5"] { color: @line; }
)QSS"), c) + ke9esuWidgetsQss(c);
}

static QPalette makePalette(const ThemeColors &c)
{
    QPalette p;
    p.setColor(QPalette::Window, c.bg); p.setColor(QPalette::WindowText, c.ink);
    p.setColor(QPalette::Base, c.panel); p.setColor(QPalette::AlternateBase, c.bg);
    p.setColor(QPalette::Text, c.ink); p.setColor(QPalette::BrightText, c.fail);
    p.setColor(QPalette::Button, c.panel); p.setColor(QPalette::ButtonText, c.ink);
    p.setColor(QPalette::Highlight, c.accent); p.setColor(QPalette::HighlightedText, c.dark ? QColor("#0b1520") : QColor(Qt::white));
    p.setColor(QPalette::ToolTipBase, c.panel); p.setColor(QPalette::ToolTipText, c.ink);
    p.setColor(QPalette::Link, c.accent); p.setColor(QPalette::LinkVisited, c.info);
    p.setColor(QPalette::PlaceholderText, c.muted);
    p.setColor(QPalette::Light, c.dark ? c.line.lighter(140) : QColor(Qt::white)); p.setColor(QPalette::Midlight, c.grid);
    p.setColor(QPalette::Mid, c.line); p.setColor(QPalette::Dark, c.muted); p.setColor(QPalette::Shadow, c.dark ? QColor(Qt::black) : c.muted);
    for (QPalette::ColorRole r : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) p.setColor(QPalette::Disabled, r, c.muted);
    p.setColor(QPalette::Disabled, QPalette::Base, c.bg);
    return p;
}

static QString legacyQss(int s)
{
    QString path = s == _DarkStyle ? ":/stylesheet/res/DarkStyle.qss" : s == _SunnyStyle ? ":/stylesheet/res/SunnyStyle.qss" : QString();
    if (path.isEmpty()) return QString();
    QFile f(path); if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    return QString::fromLatin1(f.readAll());
}

/* Original Coil64 dialogs carry fixed minimum sizes from their .ui files; with the KE9ESU padding or larger text the
   layout can need more, and Qt then overlaps rows. On show (and after a theme/text change) grow such windows to their
   layout's minimum. */
class LayoutGuard : public QObject
{
public:
    using QObject::QObject;
    static void fitWindow(QWidget *w)
    {
        if (!w || !w->isWindow() || !w->layout() || w->windowType() == Qt::Popup || w->windowType() == Qt::ToolTip) return;
        QSize need = w->layout()->totalMinimumSize(); if (!need.isValid()) return;
        if (w->maximumWidth() < need.width() || w->maximumHeight() < need.height()) w->setMaximumSize(w->maximumSize().expandedTo(need));
        if (w->minimumWidth() > 0 || w->minimumHeight() > 0) w->setMinimumSize(w->minimumSize().expandedTo(need));
        if (need.width() > w->width() || need.height() > w->height()) w->resize(w->size().expandedTo(need));
    }
protected:
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (e->type() == QEvent::Show && o->isWidgetType()) { QWidget *w = static_cast<QWidget *>(o);
            if (w->isWindow() && qobject_cast<QDialog *>(w)) QTimer::singleShot(0, w, [w] { fitWindow(w); }); }
        return false;
    }
};

void applyAppTheme(int s)
{
    capture();
    bool ke = s == _Ke9esuLight || s == _Ke9esuDark || s == _Ke9esuSystem;
    if (ke) {
        g_theme = isDarkStyle(s) ? darkColors() : lightColors();
        if (QApplication::style()->objectName().compare("fusion", Qt::CaseInsensitive) != 0) QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(makePalette(g_theme));
        qApp->setStyleSheet(themeQss(g_theme));
    } else {
        // Coil64's own themes: original platform style and palette, Coil64's style sheet, plus readable RF widgets
        if (!g_origStyle.isEmpty() && QApplication::style()->objectName().compare(g_origStyle, Qt::CaseInsensitive) != 0) {
            if (QStyle *st = QStyleFactory::create(g_origStyle)) QApplication::setStyle(st);
        }
        QApplication::setPalette(g_origPalette);
        ThemeColors c = s == _DarkStyle ? darkColors() : lightColors();
        if (s == _DarkStyle) { c.bg = QColor("#474748"); c.panel = QColor("#363637"); c.line = QColor("#5a5a5a"); c.ink = QColor("#ccfafc"); }
        g_theme = c;
        qApp->setStyleSheet(legacyQss(s) + ke9esuWidgetsQss(c));
    }
    static LayoutGuard *guard = nullptr;
    if (!guard) { guard = new LayoutGuard(qApp); qApp->installEventFilter(guard); }
    ThemeNotifier::instance()->notify();
    QTimer::singleShot(0, qApp, [] { for (QWidget *w : QApplication::topLevelWidgets()) if (w->isVisible() && qobject_cast<QDialog *>(w)) LayoutGuard::fitWindow(w); });
}

ThemeNotifier *ThemeNotifier::instance() { static ThemeNotifier *n = new ThemeNotifier; return n; }

} // namespace ke9esu

bool isDarkStyle(int s) { return s == _DarkStyle || s == _Ke9esuDark || (s == _Ke9esuSystem && ke9esu::systemPrefersDark()); }
