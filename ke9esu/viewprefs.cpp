/* viewprefs.cpp - see viewprefs.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "viewprefs.h"
#include <QSettings>
namespace ke9esu {
int clampFontPx(int px) { return px < kMinFontPx ? kMinFontPx : px > kMaxFontPx ? kMaxFontPx : px; }
int stepFontPx(int px, int dir) { int step = (px + (dir > 0 ? 0 : -1)) >= 20 ? 2 : 1; return clampFontPx(px + (dir > 0 ? step : -step)); }
int validStyle(int s) { return s < StyleCoil64Default || s > StyleKe9esuSystem ? StyleKe9esuSystem : s; }
int migrateStyle(QSettings &g, int stored)
{
    int s = validStyle(stored);
    if (!g.value("ke9esuThemeMigrated", false).toBool()) {
        if (s == StyleCoil64Default) s = StyleKe9esuSystem;
        g.setValue("ke9esuThemeMigrated", true);
    }
    return s;
}
ViewPrefs loadViewPrefs(QSettings &g, int defaultPx)
{
    ViewPrefs p;
    p.style = validStyle(g.value("styleGUI", StyleKe9esuSystem).toInt());
    p.mainPx = clampFontPx(g.value("MainFontSize", defaultPx).toInt());
    p.textPx = clampFontPx(g.value("TextFontSize", defaultPx).toInt());
    return p;
}
void saveViewPrefs(QSettings &g, const ViewPrefs &p)
{
    g.setValue("styleGUI", validStyle(p.style)); g.setValue("MainFontSize", clampFontPx(p.mainPx)); g.setValue("TextFontSize", clampFontPx(p.textPx));
}
}
