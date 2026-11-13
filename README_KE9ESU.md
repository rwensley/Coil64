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
- **Find standard-part combination** — series / parallel / mixed networks of up to 6 parts from E6…E192 or your own
  parts list, for any needed R, C or L.
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
suites ported to C++, plus golden diffs of 150 000+ values against the JavaScript originals (byte-identical texts).

