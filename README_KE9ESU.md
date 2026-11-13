# Coil64 — KE9ESU fork

This is Ron Wensley's (KE9ESU, EN53) working fork of **[Coil64](https://github.com/radioacoustick/Coil64)**,
the RF inductor and choke calculator by **Valery Kustarev** (<https://coil32.net>). All credit for Coil64 goes to
him. The fork stays under the same **GNU GPL v3.0-or-later** licence (see `LICENSE`); icons are from icons8 as in
upstream.

Branches
- `master` — kept identical to upstream radioacoustick/Coil64.
- `ke9esu-awg-label-fix` — bug fixes only (suitable for offering upstream later).
- `ke9esu` — integration/working branch: the fixes + Windows/Linux CI + docs + the RF transformers & traps windows.

## Fixes so far (on top of v2.4.39)
1. **Wire-diameter unit label follows Preferences.** Some fields showed e.g. "Wire diameter d: 28 in" while the value
   was read as #28 AWG (Inductance tab → Ferrite toroid coil), the Multilayer Rdc/wire toggle hard-coded "mm", the
   metal-detector search coil overwrote its captions with "AWG", and the crossover dialog always used mm.
   One helper (`wireDiameterUnit()` + `MainWindow::setWireDiameterField()`) now sets every wire caption/validator:
   "AWG" when AWG is selected, otherwise the selected length unit (mm, cm, in, mil). Takes effect immediately after
   changing Preferences. Also fixes the ferrite-toroid result line missing its unit and the saturation-current panel
   receiving the wire diameter as µ.
2. **Integer division in the Arnold proximity table** (`resolve_q.cpp`, Table4_b): `4 / 19` evaluated to 0, making
   the proximity factor jump for long single-layer coils (ρ > 8). Now `4.0 / 19.0`, continuous with the table.

Details: `docs/PORTING.md` → "Coil64 bug fixes in the fork". The standalone designer (v1.3.1) already follows selectable wire units (AWG / mm / in).

## Windows build
`.github/workflows/windows-build.yml` builds `Coil64.exe` on every push to `ke9esu*` branches (or by hand via
*Run workflow*) with Qt 5.15.2 / MinGW 8.1 64-bit, runs `windeployqt`, adds the `Data` folder, and uploads the
artifact **Coil64-KE9ESU-win64** (unzip anywhere and run `Coil64.exe`). It also builds and runs the `tests/ke9esu`
test target and a headless smoke run that opens every RF window (screenshots uploaded as a second artifact); a Linux job
does the same with Qt 5.15.2 gcc_64.

## RF transformers & traps (new menu)
A C++ port of the **KE9ESU Balun, Unun & Trap Designer** and the **KE9ESU Component Combiner**, built into Coil64.
The designer derives from *balun-unun-designer* by **Volodymyr Kopytsia** (github.com/hermes98761234/balun-unun-designer,
MIT licence, kept in `docs/THIRD_PARTY-balun-unun-designer-MIT.txt`); the fork as a whole stays GPL-3.0-or-later.

Menu **RF transformers & traps**:
- **Balun / Unun designer** — 1:1, 4:1 Guanella / Ruthroff, 9:1 Ruthroff trifilar, 9:1 autotransformer, 49:1 / 64:1
  (with compensation capacitor), 1:1 common-mode choke. Cards, per-frequency results, parts & cut lengths, build steps,
  winding diagram and impedance plot, build sheet (print / PDF).
- **Long-wire analyzer** — band-by-band impedance tendency for an end-fed wire (default 124 ft, 160–6 m, 100 W), avoid
  zones and good lengths, recommended transformer (opens in the designer) and trap suggestions (open in the trap windows).
- **Coax trap** and **LC trap** — turns / coil / capacitor, Q, SRF, voltage and current checks, reactance on the other bands.
- **Core database** — Fair-Rite + Micrometals (datasheets) or Amidon (with corrections, e.g. FT-240-43 AL 1075,
  T-50-6 AL 40) or all, AL cross-checks, source links.
- **Find standard-part combination** — series / parallel / mixed networks of up to 6 parts from E6…E192 (each series
  shown with its usual part tolerance, e.g. "E12 (10%)") or your own parts list, for any needed R, C or L. Every
  result shows its error %, whether it is within your ±% and a wiring diagram (R1, R2 … with values); the selected
  one is drawn large with the spread its parts' own tolerance causes.
- **KE9ESU presets** — 9:1 Ruthroff (2 × FT240-43, 4 trifilar turns AWG 16 PTFE), 9:1 autotransformer 4:12 on
  2 × FT240-43, feedline choke 16 turns RG-400 on a 2.4" #31 core.

Every window: checks bar with FAIL / CHECK / NOTE / OK chips (lists hidden until ticked; remembered), collapsible
roll-ups (collapsed by default; remembered), wire sizes and lengths that follow Coil64 Preferences live (AWG / mm / cm /
in / mil), and **Pull value…** / **Use in…** menus. A value that came from another window is marked "◂ from <window>".

Value links (shared RF project): inductance → Coil64 (switches to the ferrite-toroid or one-layer calculator with the
core / coil preloaded); core shared both ways between Core DB, designer, LC trap and Coil64's ferrite toroid; wire gauge
and insulation; trap frequency (long-wire suggestions, Coil64 LC circuit → traps); capacitance → Coil64 LC; a coil
designed in Coil64 → LC trap (Q and SRF); project power / impedance / SWR / duty / ambient shared by every window;
long-wire problem bands → recommended transformer; needed C / L → standard-part combination and back.
**File → Open / Save RF project** stores the shared values and every window's inputs as one JSON file; the last
session is restored automatically. Menu toggles: link core & wire live; share Coil64 results with the RF windows.

Tests: `tests/ke9esu` (console target, also run in CI on Windows and Linux) — the designer's and combiner's own test
suites ported to C++, plus golden diffs of 150 000+ values against the JavaScript originals (byte-identical texts),
plus theme / text-size settings tests (80 cases).

## Look & feel (Milestone 2)
Coil64 now follows the KE9ESU web app's look; the drawings follow Coil64.
- **Themes** — *View → Theme* (also *Customize → Theme*): Coil64 Default / Sunny / Dark (unchanged) plus
  **KE9ESU Light**, **KE9ESU Dark** and **KE9ESU System** (follows Windows' app light/dark setting, also while running).
  The KE9ESU themes are one application-wide style sheet + palette (Fusion) with the web app's colours: cards, slim
  inputs, accent Calculate / Accept buttons, accent tab marker, slim scrollbars. They apply to the main window, every
  original Coil64 dialog and every RF window, live. An existing install that used Coil64 Default is moved to KE9ESU
  System once; pick any theme afterwards and it is kept.
- **Text size** — *View → A− / A+ / Reset* (Ctrl −, Ctrl +, Ctrl 0, anywhere in the app) and the A− / A+ buttons in
  every RF window: 1 px steps (2 px above 20 px), 9…32 px, main and result text together. ◐ in an RF window toggles
  KE9ESU Light / Dark. Theme and sizes are stored in Coil64's own settings (group `GUI`: `styleGUI`, `MainFontSize`,
  `TextFontSize`), so Coil64's Options dialog shows the same values.
- **RF windows** — result cards (key small, value large), FAIL / CHECK / NOTE / OK chips with the coloured left bar
  (zero counts muted), roll-up header bars (click anywhere on the bar), coloured texts taken from the theme; build
  sheets always print light.
- **Drawings in Coil64's style** — shaded ferrite, 3-D wire tubes (yellow / orange / blue per winding, orange-red
  leads), hatched core section with h, OD / ID dimension lines with arrowheads, italic serif labels: balun / unun
  windings (incl. the 9:1 autotransformer's TAP), the coax choke, the coax trap (form, coax turns, D / l + equivalent
  L ∥ C), the LC trap (air coil on Coil64's copper former or the toroid + trap circuit) and a long-wire sketch (unun,
  counterpoise, wire with its length, suggested trap positions) plus the web app's avoid-zone chart. Drawings sit on
  "paper" (white; light grey in dark themes) so they stay readable, and they are in the build sheets.
- **Layout** — input panes are scroll areas in splitters sized to their forms; forms wrap a field under its label
  when narrow; long preset / core / conductor names elide in the box with the full name as tooltip and in a wider
  list; result cards reflow (1…4 per row) with the text size; checks chips wrap; original Coil64 dialogs grow to their
  layout when the theme or text size needs more room. Checked at 1024 px wide and at large text.
- **Long-wire analyzer** — the web app's full guidance is back: the k hint, the band-table caveat, "9:1 vs 49:1 for
  this wire", the five counterpoise / choke / tuner / safety / measure reminders, the published-length cross-check,
  and the avoid-zone chart.

