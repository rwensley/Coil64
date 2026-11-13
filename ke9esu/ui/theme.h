/* theme.h - KE9ESU fork of Coil64: application-wide look (QSS + QPalette) matching the KE9ESU web app.
   Themes: Coil64's own Default / Sunny / Dark plus KE9ESU Light, KE9ESU Dark and KE9ESU System (follows the OS).
   The KE9ESU themes use the Fusion style with a full palette and one generated style sheet that also styles the
   RF windows' cards, chips and roll-ups; every theme keeps the RF widgets readable.
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_THEME_H
#define KE9ESU_THEME_H
#include <QObject>
#include <QColor>
#include <QString>
namespace ke9esu {

struct ThemeColors {
    bool dark = false;
    QColor bg, panel, ink, muted, line, accent, ok, warn, fail, info, grid, band;
    QColor paper, paperInk;          // drawings are made on "paper" (light even in dark themes, like Coil64's images)
};
ThemeColors lightColors();
ThemeColors darkColors();
const ThemeColors &theme();         // colours of the active theme
bool themeIsDark();
bool systemPrefersDark();           // OS setting (Windows registry, otherwise the platform palette at start-up)
QString themeName(int styleGUI);
QString themeQss(const ThemeColors &c);        // the full KE9ESU style sheet
QString ke9esuWidgetsQss(const ThemeColors &c); // only the RF-window widgets (appended to Coil64's own themes)
/* Apply a Coil64 styleGUI value (0 Default, 1 Sunny, 2 Dark, 3 KE9ESU Light, 4 KE9ESU Dark, 5 KE9ESU System). */
void applyAppTheme(int styleGUI);

class ThemeNotifier : public QObject
{
    Q_OBJECT
public:
    static ThemeNotifier *instance();
    void notify() { emit changed(); }
    void request(int code) { emit requested(code); }   // -1 / +1 text size, 0 reset, 2 toggle light/dark
signals:
    void requested(int code);
    void changed();                 // theme or font size changed: re-render rich text and drawings
};
}
#endif
