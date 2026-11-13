# Porting the KE9ESU Balun, Unun & Trap Designer into Coil64

Target: a Coil64 fork (Qt 5/6, C++, GPL-3.0-or-later, `qmake Coil64.pro`).
All maths is in `core.js` (`BalunCore`), written as pure, deterministic functions
with SI-ish units documented in comments, so each maps 1:1 to C++.
Nothing in `core.js` touches the DOM.

## Proposed new Coil64 dialogs

| New dialog (QDialog: .cpp/.h/.ui) | Menu (MainWindow) | Replaces in the web app |
|---|---|---|
| `BalunUnun` | `on_actionBalun_Unun_triggered()` | Designer tab: 9:1 Ruthroff, 9:1 autotransformer (`auto91`), 4:1 Ruthroff/Guanella, 49:1/64:1 EFHW |
| `CmChoke` | `on_actionCommon_mode_choke_triggered()` | Designer tab with `choke11` topology + impedance plot |
| `EndFedWire` | `on_actionEnd_fed_long_wire_triggered()` | My EFLW tab |
| `CoaxTrap` | `on_actionCoax_trap_triggered()` | Coax trap tab (`coaxTrap`) |
| `LcTrap` | `on_actionLC_trap_triggered()` | LC trap tab (`lcTrap`) — reuse Coil64's one-layer coil and toroid code directly |

Each follows the existing pattern (see `on_actionCoupled_coils_triggered`, mainwindow.cpp ~6229):

```cpp
BalunUnun *d = new BalunUnun();
d->setAttribute(Qt::WA_DeleteOnClose, true);
connect(d, SIGNAL(sendResult(QString)), this, SLOT(getAddCalculationResult(QString)));
connect(this, SIGNAL(sendOpt(_OptionStruct)), d, SLOT(getOpt(_OptionStruct)));
connect(this, SIGNAL(sendLocale(QLocale)), d, SLOT(getCurrentLocale(QLocale)));
emit sendLocale(loc); emit sendOpt(*myOpt); d->exec();
```
Results go to the main results pane through `sendResult(QString)` (HTML).
Add the files to `Coil64.pro` (SOURCES/HEADERS/FORMS) and new strings to the .ts files.

## Function map (core.js -> C++)

Suggested home: a new UI-free `balun_core.h/.cpp` (namespace `balun`), used by all three dialogs and unit-testable.

| core.js | C++ | Notes / reuse in Coil64 |
|---|---|---|
| `MATERIALS[*].mu` (from mu_data.json) | `RF_Toroid::readMaterialData()` (rf_toroid.cpp:114) on `data/<mix>-Material-Fair-Rite.csv` | Do NOT embed: Coil64 already ships the CSVs. Mix 77 file must exist in data/. |
| `permeability(mat,fHz)` | `balun::permeability()` | log-f linear interpolation, clamp at table ends; iron: mu'=mui, mu''=mui/Q |
| `CORES`, `coreById` | extend `amidon.cpp` tables or new `data/balun_cores.csv` | keep Fair-Rite PN, AL, AL tol, Ae, le, Ve, src |
| `geometry(od,id,h)` | reuse Coil64 toroid geometry helpers | Ae, le, Ve, l0 (AL at mu=1) |
| `inductanceUH(core,N,stack,tol)` | `balun::inductance()` | L = AL*N^2*stack (nH->uH), tol band |
| `selfCapacitancePF` | port of G3RBJ toroid Cs already in rf_toroid.cpp | |
| `K9YC_CS_PF`, `strayPF` | constant + option | 1.3 pF fit to K9YC #31 data |
| `windingImpedance(core,N,fHz,stack,csPF)` | `balun::impedance()` returns `std::complex<double>` | Z = w N^2 (AL/mui)(mu'' + j mu'), parallel Cs |
| `selfResonanceMHz`, `impedanceCurve`, `logSpace` | `balun::` helpers | feed a QCustomPlot/QChart (or QPainter) plot with band shading |
| `fluxDensity_mT`, `heatingFluxLimitGauss` | `balun::` | B = Vrms*sqrt2/(w N Ae); Amidon 150/sqrt(fMHz) G |
| `ironCoreLoss_mWcm3` | `balun::` | Micrometals formula (-2), extrapolated at HF |
| `coreSurfaceCm2`, `tempRiseC` | `balun::` | Amidon dT=(mW/cm^2)^0.833 |
| `mismatchFactor` | `balun::` | 2S/(S+1) |
| `turnLengthMM`, `windingFit` | `balun::` | wire cut lengths incl. leads |
| `compensationCap` | `balun::` | Xc ~55 ohm at top HF, clamp 47-220 pF, rating >= 5x Vpk |
| `TOPOLOGIES`, `DEFAULT_INPUT`, `PRESETS`, `normalize` | `struct BalunInput` + `QSettings` | presets -> combo box; QSettings replaces localStorage |
| `TOPOLOGIES.auto91` (v1.3) | `BalunUnun` device combo entry | single tapped winding, `turnsRatio: 3`: same code path as the EFHW autotransformers (n1 = tap turns, n2 = 3·n1); adds `lFullUH` = 9·L(n1), its own `connectionSteps` case (even spacing, tap at n1 → SO-239 centre), NOTE checks `auto`/`leak`/`split`/`comp` (compensation cap optional, not in parts). Validated vs Cromwell (FT140-43, 8 T = 56.6 µH). |
| `design(input)` | `balun::Result balun::design(const BalunInput&)` | returns checks (enum ok/warn/fail/info + text), steps, parts |
| `bandsInRange`, `BANDS` | `balun::` | US allocations 160-6 m (make region-selectable) |
| `halfWaveFt`, `classifyHalfWaves`, `eflwAnalysis`, `avoidZones`, `goodLengths` | `EndFedWire` dialog backend | units: ft in the UI, metres option via `_OptionStruct` |


### Traps (v1.1) — core.js -> C++

| core.js | C++ / Coil64 equivalent | Notes |
|---|---|---|
| `COAX`, `coaxById` | new `data/coax.csv` | id, label, Z0, pF/ft, VF, OD in, centre/shield mm, V rms, dielectric, source |
| `PVC_FORMS` | combo box | Sch 40 OD (ASTM D1785) |
| `wheelerUH(rIn,lenIn,n)` | `trap::wheeler()` | VE6YP/K7MEM model uses Wheeler with D = form OD + coax OD |
| `stubCapPF(coax,lenIn,fMHz)` | `trap::stubC()` | C = tan(βl)/(ωZ0): open stub, W8JI observation |
| `coaxTrap(input)` | `trap::coax()` | solves n for f by bisection; returns checks/steps/parts like `design()` |
| `helicalUH(N,p,Dk,dw,acc)` | Coil64 `getOneLayerN_withRoundWire()` / `HeliCoilS()` (resolves.cpp) | direct port of Weaver's helical-filament method — call the original in C++ |
| `nagaokaRosaUH`, `nagaokaK`, `sheetUH`, `rosaKs/Km` | independent cross-check | Nagaoka (Lundin/W82W fit) + Rosa round-wire corrections |
| `coilCs_pF`, `coilSRF_MHz`, `vfNom` | Coil64 `find_Cs()`, `findSRF()` (resolves.cpp) | Knight 2016 self-capacitance & SRF |
| `coilQ`, `skinFactor`, `lookupPsi` | Coil64 `solve_Qr()` + Arnold tables (resolves.cpp) | see bug note below |
| `snapE`, `E_SERIES`, `capRating`, `CAP_RATINGS`, `CAP_TYPES` | `trap::` | E6/E12/E24; rating ≥ 2×Vpk snapped to standard kV steps |
| `trapVoltage(P,Zend)` | `trap::` | √(P·Z) estimate at the trap |
| `trapReactanceAtBands(L,C,f0)` | `trap::` | X = ωL/(1−ω²LC) per band |
| `lcTrap(input)` | `trap::lc()` | air coil → helical; toroid → AL·N² with integer N and C re-solved |

### Amidon data

| core.js | C++ | Notes |
|---|---|---|
| `AMIDON_DATA` (amidon_data.json, built by tools/gen_amidon.py from Coil64 `amidon.cpp` + amidon_web.json) | use `amidon.cpp` tables directly | sizes `[name, OD, ID, H in inches, AL]` |
| `alToNH(al, iron)`, `alFromNH` | `amidon::toNH()` | ferrite mH/1000T = nH/T²; iron µH/100T = 10·nH/T² |
| `pickAL(...)` | `amidon::pickAL()` | candidates Coil64 vs amidoncorp.com; reject > 2.5× off the geometric estimate (l0·µi); use the one closest (log) to geometry |
| `addAmidon()` | — | creates `A:FT-240-43` / `A:T-200-2` cores (db "Amidon", alAmidon {value, unit}, alCoil64, alWeb, alGeom, alPick, alSuspect, alDisagree, equiv cross-ref ±3% dims same mix) |

### Issues found in Coil64 while porting (worth fixing upstream)
- `resolves.cpp` Arnold table `Table4_b`: the entry written `4 / 19` is C++ integer division (= 0). The JS port uses 4.0/19. Affects proximity-factor interpolation slightly at that point.
- `amidon.cpp` FToroid_AL row for mix 43 appears shifted by one size: FT-240-43 = 1300 (Amidon web and Fair-Rite 5943003801: 1075), FT-193A-43 = 1075 (geometry ≈ 1500). The app uses 1075 for FT-240-43 and flags FT-193A-43 as "far from geometry".
- `amidon.cpp` T-50-6 = 46 µH/100T; amidoncorp.com and Micrometals list 40. The app uses 40.
- amidoncorp.com typos rejected by the geometry rule: T-106-6 166 (Coil64 116 used), T-106-17 1851.
- Coil64 C++ could not be compiled on the build box (no g++), so the JS ports were validated against independent formulas (Nagaoka+Rosa ≤ 2 %, Wheeler ≤ 5 %) and published coax-trap builds instead of against Coil64's own output — re-check in the C++ port with QtTest.

### Ron's integration requirement #2 — checks summary bar (all Coil64 dialogs)
Every dialog that produces checks (BalunUnun, CmChoke, CoaxTrap, LcTrap) must show the same **summary bar**
directly under the results/statistics area instead of a long list:
- one chip per severity in the order FAIL > CHECK (warn) > NOTE (info) > OK, each with its **count always visible**,
  colour-coded like the list items (red / amber / blue / green); zero-count chips stay visible but dimmed;
- each chip has a checkbox (QCheckBox, keyboard-focusable, accessible name "Show N failures" etc.) that shows/hides
  that severity's items in the list below (QTextBrowser/QListWidget), sorted fail > warn > note > ok;
- default: all unchecked (items hidden); state remembered per user with QSettings
  (`checks/show_fail`, `checks/show_warn`, `checks/show_info`, `checks/show_ok`), shared by all dialogs —
  the web app uses localStorage key `ke9esu-balun-designer-v1` → `showSev`;
- printed/exported results (QTextDocument → QPrinter/PDF, and the `sendResult` HTML) always include a compact summary:
  the counts plus every FAIL and CHECK item, regardless of the on-screen toggles.
Reference implementation: `renderChecks()`, `sortedChecks()`, `sevCounts()`, `checksSheetHTML()` in src/ui.js.

**Roll-up tables (same requirement, all dialogs):** every table (per-frequency detail, parts, band/reactance tables,
EFLW band table, core and material database tables) is collapsible — collapsed by default, with a one-line summary in
its header (row counts, extremes, colour-coded FAIL/CHECK/OK or category counts) and a chevron; click or keyboard
toggles it. Open/closed state is stored per table (QSettings `tables/<key>/open`; web app: `roll` in localStorage).
Qt: a QToolButton (arrow, checkable) + QLabel summary as the header of a collapsible QWidget around each QTableWidget.
Core-database table layout (Ron, compact): no long source text — a narrow "Src" column with a ↗ link that opens the
part's page (amidoncorp.com product page, fair-rite.com/product/toroids-<PN>/, Micrometals literature) with the full
source text + URL as tooltip; iron-powder colour as a small swatch (tooltip = colour name); DB abbreviated FR/MM/Am;
part number under the core name; Ae · le · Ve in one column (* = computed); AL tolerance and cross-reference DB/part in
tooltips. In Qt: QTableView with a delegate for the swatch, a link column (QDesktopServices::openUrl) and ToolTipRole
for the long texts; target: no horizontal scrolling at 1280 px. Reference: `renderCores()`, `sourceUrl()` in src/ui.js.
Printed/exported sheets always contain the full tables. Reference: `initRolls()`, `setSum()`, `perFreqSum()`,
`bandsSum()`, `partsSum()` in src/ui.js.

UI pieces to rebuild with Qt widgets: winding diagram (QPainter on a QLabel/QWidget),
impedance plot, checks list (QTextBrowser HTML like other Coil64 results),
printable build sheet (QTextDocument -> QPrinter / PDF).

## Tests
`test.js` holds the reference cases (G4AON, Fair-Rite single-turn Z, K9YC table,
DG1JAN efficiency, G3TXQ T200-2, Amidon flux example; v1.1: K7MEM RG-8X coax-trap table,
VE6YP 7.04 MHz example, W4ULD RG-58 table, W8JI stub/LC measurements, Amidon AL
conversions and datasheet cross-checks). Port them to a small
QtTest target so the C++ port is checked against the same numbers.

## Licensing
GPL-3.0-or-later (compatible with Coil64). Keep the MIT notice for code adapted from
balun-unun-designer (Copyright (c) 2026 Volodymyr Kopytsia) in the source headers and About dialog.

## Ron's integration requirements (from KE9ESU)
1. **Push calculated inductance into Coil64's coil designer (Nov 13, 2026).** Whenever a Balun/Unun, Coax trap, or LC trap dialog calculates an inductance (primary L, trap L, choke L), add a "Send to coil designer" action and do it automatically on accept. It should fill Coil64's main-window Inductance input field (in its current unit), so the user can go straight to a single-layer, multilayer, or toroid coil design for that value. Implement it the way Coil64's other dialogs return results: a signal from the dialog to a MainWindow slot that sets the inductance field and switches to the matching coil calculator (air-core, or toroid with the selected core preloaded).

### Requirement #3: Visual style when merged (from KE9ESU, Nov 13, 2026)
- **Drawings and artwork:** redraw the balun, unun, choke, coax trap and LC trap diagrams in Coil64's drawing style. Match its schematic and winding images in line weight, colors, labeling and image format, so the new dialogs look native next to Coil64's existing coil pictures.
- **UI look and feel:** restyle Coil64's interface to match this app's look. That covers the dark/light theme, card-style results, color-coded check chips (FAIL/CHECK/NOTE/OK), roll-up tables with summary headers, A-/A+ text size, and spacing and fonts. In Qt, do this with an application-wide QSS stylesheet and a QPalette for dark/light, plus a theme toggle and a font-scale setting stored in QSettings, applied to MainWindow and every dialog.
- **In short:** the pictures follow Coil64, and the windows follow this app.

## Coil64 bug fixes in the fork
Local branch `ke9esu-awg-label-fix` in /workspace/coil64 (commit ded6756 on top of upstream v2.4.39 b9b4e83);
patch: /workspace/coil64_awg_label_fix.patch. Not reported upstream (no matching issue at radioacoustick/Coil64 as of Nov 13, 2026).

### 1. Wire-diameter unit caption does not follow Preferences (reported by KE9ESU)
Symptom: with Preferences → wire size = AWG and length unit = inches, some fields show e.g. "Wire diameter d: 28 in"
while the 28 is actually read as #28 AWG (and the reverse: "mm" shown when the unit is inches or the value is AWG).
Root cause: the unit caption beside each wire field was set by ~14 copy-pasted `if (isAWG) label->setText("AWG")`
blocks in `MainWindow::on_listWidget_currentRowChanged()`; a few paths missed it or hard-coded a system:
- mainwindow.cpp, Inductance tab → Ferrite toroid coil (`case _FerrToroid`, tab 1): `label_04_2` was never set to AWG
  and the field kept the decimal validator, so the caption showed the length unit (in/mm) for an AWG value.
  This is the exact "28 in" case.
- mainwindow.cpp `on_radioButton_8_toggled()` (Inductance tab → Multilayer coil, Rdc ↔ wire diameter; also runs every
  time Multilayer is selected): caption hard-coded `tr("mm")` — wrong when the unit is in/cm/mil, and wrong in AWG mode
  when no wire was stored yet (d = 0). The Rdc field also inherited the AWG validator (could not type 0.52 Ω).
- mainwindow.cpp `get_ferriteI_Result()`: the wire line in the results had no unit at all; and (unrelated, same spot)
  the saturation-current dock was sent the wire diameter (`lineEdit_4_2`) as permeability µ instead of `lineEdit_5_2`.
- multiloop.cpp (Search coil of a metal detector, DD tab): AWG mode overwrote the captions `label_dw`/`label_dw2`
  ("Wire diameter dt/dr") with "AWG" and left the unit labels `label_dw_m`/`label_dw2_m` in length units →
  results read "AWG: 28 in".
- crossover.cpp (Audio crossover coil): wire-range selector always listed/labelled the metric series in mm,
  ignoring the length-unit preference.
Fix (central, no system hard-coded):
- `QString wireDiameterUnit(const _OptionStruct*)` in system_functions.{h,cpp}: "AWG" when isAWG, else the selected
  length unit (mm / cm / in / mil …, translated via the existing "Context").
- `MainWindow::setWireDiameterField(QLabel *unit, QLineEdit *edit)`: sets the caption from wireDiameterUnit() and the
  validator (AWG regex or decimal). Used by every wire field (18 call sites; the 14 duplicated blocks removed).
- Preferences change applies immediately: `getOptionStruct()` already re-runs the calculator setup
  (`on_tabWidget_currentChanged` → `on_listWidget_currentRowChanged`), which now always goes through the helper, so
  captions, validators and the re-rendered value (from the mm value in `data->d`) switch together without restarting
  or reselecting. Dialogs are modal and re-read options each time they open.
- multiloop/crossover use wireDiameterUnit(); crossover shows the metric wire series converted to the selected unit
  (combo item data = series index, so the calculation is unit-independent).
Verification: changed files compile against Qt 5.15.2 (clang, -Wall, no errors); unit check of wireDiameterUnit()
for AWG/mm/cm/in/mil passes; a static audit confirms all 15 "Wire diameter d:" captions in the main window have a
matching setWireDiameterField() call (before the fix, Inductance/Ferrite toroid had none). Full Windows GUI build
and click-through still to do in the fork's CI.

### 2. Integer division in resolves.cpp (found earlier, not yet patched)
`Table4_b` entry `4 / 19` is C++ integer division (= 0); should be `4.0 / 19`. See "Issues found" above. Fix in a
separate commit in the fork.

### Wire units in the ported KE9ESU dialogs (Ron, Nov 13, 2026)
Wire size must stay user-selectable (EU users work in mm): the ported Balun/Unun, Choke, Coax-trap and LC-trap dialogs
must follow Coil64's Preferences exactly like the main window — AWG gauge with "AWG"/# when isAWG, otherwise wire
diameter in the selected length unit — using `wireDiameterUnit()`, `converttoAWG()/convertfromAWG()` and
`dwLengthMultiplier`; store all lengths internally in mm. Never hard-code AWG, mm or inches in a caption.
Standalone HTML app v1.3.1 honours this: `BalunCore.setUnits({wire: "awg"|"mm"|"in", len: "mixed"|"mm"|"in"})`
and the text helpers `wireLabel()`, `wireSize()`, `wireDiaMM()`, `mmTxt()`, `inchTxt()`, `lenDual()`, `dimsTxt()`,
`dispUnit()` build every caption; inputs keep their native unit internally (mm, coax trap in inches) and are
converted for display (ui.js `LEN_FIELDS`, `putVal()`, `fromDisp()`). In the C++ port these map 1:1 onto
`_OptionStruct.isAWG` / `ssLengthMeasureUnit` / `dwLengthMultiplier` and `wireDiameterUnit()` (Coil64 has no
"mixed" mode — use its single length unit). Calculations never depend on the display unit (tested).

## Implementation status — Milestone 1 (Nov 13, 2026)
Credits: Coil64 © Valery Kustarev (GPL-3.0). The designer's core and tests derive from *balun-unun-designer* by
Volodymyr Kopytsia (hermes98761234, MIT; licence text in `docs/THIRD_PARTY-balun-unun-designer-MIT.txt`). The KE9ESU
port and additions are GPL-3.0-or-later.

Layout
- `ke9esu/` (UI-free, QtCore only; `ke9esu.pri`): `balun_core` (materials, cores, wires, topologies, presets, physics,
  `design()`), `trap_core` (coax / LC traps), `eflw_core` (long-wire analysis, recommendation, trap suggestions),
  `combiner_core` (`ke9esu::cc`, meet-in-the-middle series/parallel search), `serialize` (input JSON), `ke9esu_util`
  (JS-faithful number formatting: toFixed / toPrecision / Number→string, so texts are byte-identical to the web app).
  Data as Qt resources (`ke9esu.qrc`): Coil64's own `data/<mix>-Material-Fair-Rite.csv` (aliased, not copied) and
  `data/amidon_ordered.json`.
- `ke9esu/ui/` (`ke9esu_ui.pri`, included by `Coil64.pro`): `project` (shared RF project: values + source window +
  window states, JSON file format "KE9ESU Coil64 RF project"), `widgets` (ChecksBar, Rollup, Cards, FromMarker,
  LenSpin, ProjectBox, SheetDialog, `Ke9esuDialog` base with Pull value / Use in / Build sheet / Accept),
  `diagram` (QPainter winding diagram + impedance plot), `coreui` (CoreSelector), the dialogs (`balundialog`,
  `eflwdialog`, `trapdialogs`, `coredbdialog`, `combinerdialog`) and `bridge` (`ke9esu::Bridge`).
- MainWindow changes are three lines plus a friend declaration: create the bridge at the end of the constructor and
  call `captureOutput()` at the top of `showOutput()`. The bridge adds the menu and File actions, mirrors Preferences
  (`sendOpt`) into the project units, and maps requests onto `_Data`: ferrite → `Do, Di, h×stack, µ = AL / l0, Ch = 0`
  and switches to Ferrite toroid; air coil → `D, d, k, p, f` and One-layer; LC → `inductance, capacitance`, LC tab and
  Calculate. Results captured from Coil64: LC tab → L / C / f; one-layer coil → {N, D, d, k, p, L, f}; ferrite toroid →
  nearest database core (OD/ID within 3 %, height = 1…4 stacked cores) + L. Results containing an error are ignored.

Tests (`tests/ke9esu`, 73 cases): the designer's 38 tests and the combiner's tests ported, fork tests (Ron's presets,
long-wire recommendation / traps, JSON round trip, project save/open, units), and golden diffs generated by
`ke9esu/tools/gen_golden.js` / `gen_golden_cc.js` from the JS sources (`golden.json`, `golden_cc.json`).
JSON writes NaN / ±Infinity as null; the diff accepts null for a non-finite C++ value.

CI (`.github/workflows/windows-build.yml`): Windows (Qt 5.15.2 / MinGW 8.1) builds Coil64 and the test target, runs
the tests, packages with windeployqt, then runs a smoke copy of the package with `KE9ESU_SHOTS=<dir>` (the bridge opens
every RF window, exercises the Coil64 links, grabs PNGs, exports every build sheet as PDF, saves/opens a project file
and quits). Linux (Qt 5.15.2 gcc_64) does the same headless (`QT_QPA_PLATFORM=offscreen`). The combiner's 6-part
speed tests keep the JS suite's 6 s budget locally; CI sets `KE9ESU_SPEED_LIMIT_MS=20000` for shared runners
(measured ≈5.3 s on windows-2022). Windows note: `<windows.h>` defines `near`/`far`, so core structs avoid those names.

Known approximations / gaps
- The long-wire guide text is condensed relative to the web app's prose; numbers and classifications are identical.
- A Coil64 coil passed to the LC trap uses the nearest wire in the trap's coil-wire list (Coil64 allows any diameter).
- Coil64 ferrite-toroid results map back to a database core only when the dimensions match a listed core.
- Window styling (QSS/palette, A−/A+ font size, chip/card restyle) and the diagram redraw are Milestone 2 (done, below).
  The long-wire guide text was restored to the web app's full wording in Milestone 2.

## Implementation status — Milestone 2 (Nov 13, 2026): requirement #3, look & feel
Files
- `ke9esu/viewprefs.{h,cpp}` (QtCore, tested): style ids (0–2 Coil64 Default / Sunny / Dark, 3–5 KE9ESU Light / Dark /
  System), text-size steps and clamping (9…32 px; 1 px, 2 px from 20 px), load / save in Coil64's `GUI` settings group,
  one-time migration Default → KE9ESU System (`ke9esuThemeMigrated`).
- `ke9esu/ui/theme.{h,cpp}`: `ThemeColors` = the web app's CSS variables (light and dark) + "paper" for drawings;
  `themeQss()` (full KE9ESU sheet), `ke9esuWidgetsQss()` (RF widgets only, appended to Coil64's own three themes);
  `applyAppTheme()` (Fusion + palette + sheet for KE9ESU themes; Coil64's original style / palette / sheet otherwise);
  `systemPrefersDark()` (`KE9ESU_SYSTEM_THEME` override, Windows `AppsUseLightTheme`, else the start-up palette);
  generated check / arrow images for styled indicators; `ThemeNotifier` (changed / requested); a layout guard that
  grows Coil64 dialogs with fixed .ui minimum sizes to their layout's minimum on show and after theme / text changes;
  `isDarkStyle()` used by every original dialog instead of `== _DarkStyle` (icon inversion, colours).
- `ke9esu/ui/drawings.{h,cpp}`: Coil64-style drawing kit (`draw::tube`, `dim`, `hatched`, `ferriteRing`, `solenoid`,
  `parallelLC`, italic serif labels) and `paintCoaxTrap`, `paintLcTrap`, `paintLongWire`, `paintZoneChart`,
  `PaperView` (aspect-keeping widget on themed paper), `renderPaper()` (2×, white, for sheets). `diagram.cpp`'s winding
  drawing was rewritten with the kit (460 × 500 logical).
- Coil64 sources: `system_functions.h` (three enum values + `isDarkStyle`), `options.ui` (three theme entries),
  `mainwindow.{h,cpp}` (`applyGuiStyle()` replaces both theme switch blocks; icon inversion tracked by `iconsInverted`),
  21 dialogs (`isDarkStyle`). `bridge.cpp` builds the View menu (Theme submenu shared with Customize → Theme,
  A− / A+ / Reset as application-wide QShortcuts), polls the OS theme every 4 s for KE9ESU System, saves the prefs
  and answers the RF windows' A− / A+ / ◐ buttons.

Screenshot mode (`KE9ESU_SHOTS=<dir>`) now also writes `light/` and `dark/` (main window: one-layer, ferrite toroid,
LC result; every RF window; balun auto 9:1, choke, 1024 px and large-text passes; View and File menus; Coil64 Options
and RF toroid dialogs) and `drawings/` (every build-sheet image). CI requires ≥ 14 PNGs per theme on Windows and Linux.

Tests: 80 cases (73 + 7): A− / A+ steps and clamps; ini round trip of theme + both sizes; migration once; KE9ESU
colours equal the web CSS; contrast (ink ≥ 7:1, muted ≥ 3.5:1, severities ≥ 3:1, drawing ink on paper ≥ 12:1);
no unexpanded `@` placeholders; applying Light / Dark / System sets palette, sheet and dark flag. The test target now
links QtGui / QtWidgets and runs on the offscreen platform.

Recommendations (Milestone 2)
- Keep the one-time move of Coil64 Default users to KE9ESU System (reversible from View → Theme); offer upstream
  only the `isDarkStyle()` / `applyGuiStyle()` clean-up, which also fixes Coil64's double icon inversion when
  switching Dark → Dark.
- Coil64's own PNG pictures stay as they are (they already sit on paper in dark themes); a later pass could
  re-render them as SVG for crisp scaling at large text.
- Optional next steps: per-window remembered splitter positions; a "compact" density toggle; a Coil64 Options page for
  the KE9ESU text sizes (today in View); translating the new strings (Coil64 ships .ts files).

Known gaps (Milestone 2)
- Coil64's original dialogs keep their own .ui layouts; they are themed and grown to fit, not re-laid-out as cards.
- The Linux offscreen screenshots use DejaVu fonts; Windows uses Segoe UI / Times New Roman, so text widths differ.
- At the largest sizes on a 1024 px screen some input panes need their horizontal scroll bar (nothing is cut off).
