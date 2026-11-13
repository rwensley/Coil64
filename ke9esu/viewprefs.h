/* viewprefs.h - KE9ESU fork of Coil64: theme and text-size preferences (QtCore only, unit-tested).
   Stored in Coil64's own settings file, group "GUI": styleGUI, MainFontSize, TextFontSize, ke9esuThemeMigrated.
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_VIEWPREFS_H
#define KE9ESU_VIEWPREFS_H
class QSettings;
namespace ke9esu {
enum { StyleCoil64Default = 0, StyleCoil64Sunny = 1, StyleCoil64Dark = 2, StyleKe9esuLight = 3, StyleKe9esuDark = 4, StyleKe9esuSystem = 5 };
const int kMinFontPx = 9, kMaxFontPx = 32;
struct ViewPrefs { int style = StyleKe9esuSystem; int mainPx = 13; int textPx = 13; };
int clampFontPx(int px);
int stepFontPx(int px, int dir);                       // A- / A+ : one pixel, then 2 px steps above 20 px, clamped
int validStyle(int style);                             // unknown values -> KE9ESU System
/* One-time move of an upgraded install from Coil64's Default look to KE9ESU System (records the migration). */
int migrateStyle(QSettings &groupGUI, int stored);
ViewPrefs loadViewPrefs(QSettings &groupGUI, int defaultPx);
void saveViewPrefs(QSettings &groupGUI, const ViewPrefs &p);
}
#endif
