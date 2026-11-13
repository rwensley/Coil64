# Coil64 — KE9ESU fork

This is Ron Wensley's (KE9ESU, EN53) working fork of **[Coil64](https://github.com/radioacoustick/Coil64)**,
the RF inductor and choke calculator by **Valery Kustarev** (<https://coil32.net>). All credit for Coil64 goes to
him. The fork stays under the same **GNU GPL v3.0-or-later** licence (see `LICENSE`); icons are from icons8 as in
upstream.

Branches
- `master` — kept identical to upstream radioacoustick/Coil64.
- `ke9esu-awg-label-fix` — bug fixes only (suitable for offering upstream later).
- `ke9esu` — integration/working branch: the fixes + Windows CI + docs, and later the balun/unun/trap dialogs.

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
artifact **Coil64-KE9ESU-win64** (unzip anywhere and run `Coil64.exe`).

## Planned: balun / unun / trap integration
The KE9ESU Balun, Unun & Trap Designer (a standalone offline HTML app) will be ported into Coil64 as new dialogs:
balun/unun designer (1:1, 4:1 Guanella/Ruthroff, 9:1 trifilar and autotransformer, 49:1/64:1), common-mode choke,
coax trap and LC trap, with the Fair-Rite/Micrometals and Amidon core databases. Requirements from Ron:
calculated inductance is pushed into Coil64's coil designer; checks are shown as a colour-coded summary bar with
roll-up tables; drawings follow Coil64's style while the windows adopt the designer's look; wire sizes and lengths
follow Coil64's unit Preferences (AWG / mm / in). Full plan and function map: `docs/PORTING.md`.
