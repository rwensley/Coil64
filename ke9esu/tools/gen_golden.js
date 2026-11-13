// KE9ESU fork of Coil64 (GPL-3.0-or-later): dump reference outputs of the KE9ESU web app's core.js (v1.3.1) for the
// C++ port's differential test (tests/ke9esu/golden.json). Run: node ke9esu/tools/gen_golden.js /path/to/core.js
const C = require(process.argv[2] || "/workspace/ke9esu_balun_designer/core.js");
const out = { version: C.VERSION, design: [], coax: [], lc: [], eflw: [], units: [] };
const txt = r => ({ checks: r.checks.map(c => [c.id, c.status, c.text]), steps: r.steps, parts: r.parts.map(p => [p.qty, p.item]) });
function designCase(inp) {
  const d = C.design(inp);
  return { input: inp, n1: d.n1, n2: d.n2, nMin: d.nMin, target: d.target, lNom: d.inductanceUH.nom, lMin: d.inductanceUH.min, lMax: d.inductanceUH.max,
    zr: d.zAtFmin.r, zx: d.zAtFmin.x, lossPeakW: d.lossPeakW, lossAvgW: d.lossAvgW, effPct: d.efficiencyPct, effWorstPct: d.efficiencyWorstPct, lossDensity: d.lossDensity_mWcm3,
    tempRiseC: d.tempRiseC, coreTempC: d.coreTempC, stray: d.stray_pF, passes: d.passes, turnLen: d.turnLengthMM, vHighRms: d.vHighRms, lFullUH: d.lFullUH,
    compCap: d.compCap ? d.compCap.pF : null, perFreq: d.perFreq.map(p => [p.fMHz, p.r, p.x, p.z, p.bPk_mT, p.bHeat_G, p.heatLimit_G, p.lossPeakW, p.lossMatchedW]),
    wires: d.wires.map(w => [w.name, w.turns, w.lengthMM]), ...txt(d) };
}
const tops = Object.keys(C.TOPOLOGIES);
const coreIds = ["FT240-43", "FT140-43", "2.4in-31", "T200-2", "A:T-200-2", "A:FT-240-43", "FT82-61", "2643251002", "A:FT-140-77", "A:T-106-6"];
const conds = ["awg16t", "awg18e", "rg400", "rg58", "cat5"];
for (const t of tops) for (const c of coreIds) for (const w of conds) out.design.push(designCase({ topology: t, coreId: c, conductorId: w, stack: (t.length + c.length) % 3 + 1, fMinMHz: 1.8, fMaxMHz: t === "efhw49" ? 29.7 : 54 }));
for (const p of C.PRESETS) out.design.push(designCase(Object.assign({}, C.DEFAULT_INPUT, { turns: null }, p.input)));
out.design.push(designCase({ topology: "choke11", coreId: "2.4in-31", conductorId: "rg400", csMode: "g3rbj" }));
out.design.push(designCase({ topology: "choke11", coreId: "2.4in-31", conductorId: "rg400", csMode: "manual", csPF: 3.3, turns: 12, cmPercent: 20 }));
out.design.push(designCase({ topology: "guanella41", coreId: "FT240-61", conductorId: "awg14t", csMode: "none", duty: "cw", vswr: 5, ambientC: 45, leadMM: 250 }));
out.design.push(designCase({ topology: "efhw64", coreId: "FT240-43", turns: 2, xlFactor: 8, zLow: 75, powerW: 500, duty: "custom", dutyPercent: 66, fMinMHz: 3.5, fMaxMHz: 29.7 }));
const coaxIds = C.COAX.map(c => c.id).concat(["custom"]);
for (const id of coaxIds) for (const f of [1.85, 3.75, 7.05, 14.175, 28.85]) for (const model of ["lumped", "stub"]) {
  const inp = { fMHz: f, coaxId: id, formOdIn: f < 5 ? 3.5 : 2.375, model, spacingIn: f > 20 ? 0.1 : 0, powerW: 100 + f, zEnd: 2500 };
  const r = C.coaxTrap(inp);
  out.coax.push({ input: inp, n: r.n, L: r.L_uH, C: r.C_pF, X: r.X, fCheck: r.fCheckMHz, fStub: r.fStubMHz, fLumped: r.fLumpedMHz, lNag: r.lNagaokaUH, q: r.q, rp: r.rpOhm, pdKey: r.pdKeyW, pdAvg: r.pdAvgW, cut: r.cutLenIn, fQ: r.fQuarterMHz, pitch: r.pitchIn, meanDia: r.meanDiaIn, wlen: r.windingLenIn, coaxLen: r.coaxLenIn, ld: r.ldRatio, vTrap: r.vTrap,
    bands: r.bands.map(b => [b.band, b.fMHz, b.x, b.kind, b.eqUH, b.eqPF, b.near]), ...txt(r) });
}
const lcCases = [];
for (const mode of ["X", "L", "C"]) for (const snap of ["none", "E6", "E12", "E24"]) for (const w of ["awg12", "awg18", "tube6"]) lcCases.push({ fMHz: 7.05, mode, snap, wireId: w, xOhm: 150, lUH: 3.3, cPF: 120 });
for (const core of ["A:T-200-2", "A:T-130-6", "A:FT-240-43", "T200-2", "A:T-106-17"]) for (const f of [3.6, 7.1, 14.1, 21.2]) lcCases.push({ fMHz: f, coreType: "toroid", coreId: core, stack: f > 10 ? 2 : 1, snap: f > 10 ? "none" : "E12" });
lcCases.push({ fMHz: 10.12, pitchMM: 6, formDiaMM: 60, capType: "vacuum", powerW: 1500, duty: "cw" });
lcCases.push({ fMHz: 24.9, wireId: "tube3", capType: "doorknob", duty: "custom", dutyPercent: 15 });
for (const inp of lcCases) {
  const r = C.lcTrap(inp);
  out.lc.push({ input: inp, L: r.L_uH, C: r.C_pF, cIdeal: r.cIdealPF, X: r.X, q: r.q, qCoil: r.qCoil, rp: r.rpOhm, cap: r.capRatingV, ic: r.capCurrentA, pd: r.pdKeyW, pdAvg: r.pdAvgW, vTrap: r.vTrap, fcs: r.fWithCsMHz,
    coil: r.coil ? [r.coil.N, r.coil.pitchMM, r.coil.DkMM, r.coil.lenMM, r.coil.esr, r.coil.uH, r.coil.csPF, r.coil.srfMHz, r.coil.q, r.coil.wheelerUH, r.coil.nagaokaRosaUH, r.coil.wireLenMM] : null,
    toroid: r.toroid ? [r.toroid.N, r.toroid.uH, r.toroid.qCore, r.toroid.qWire, r.toroid.wireLenMM, r.toroid.bGauss, r.toroid.limGauss, r.toroid.pCoreW, r.toroid.tempRiseC, r.toroid.coreTempC] : null,
    bands: r.bands.map(b => [b.band, b.fMHz, b.x, b.kind, b.eqUH, b.eqPF, b.near]), ...txt(r) });
}
for (const len of [41, 53, 66, 84, 107, 124, 135, 250]) for (const k of [0.95, 0.97]) {
  const a = C.eflwAnalysis(len, { k });
  out.eflw.push({ len, k, rows: a.map(r => [r.band, r.halfWaves[0], r.halfWaves[1], r.lambdaLo[0], r.lambdaLo[1], r.kinds.join(","), r.verdict, r.unun9, r.unun49]),
    zones: C.avoidZones(300, { k, tol: 0.05 }).map(z => [z.band, z.n, z.loFt, z.hiFt]), good: C.goodLengths(30, 300, { k, bands: C.BANDS.filter(b => b.id !== "6m") }) });
}
for (const [w, l] of [["awg", "mixed"], ["mm", "mm"], ["in", "in"], ["awg", "in"], ["mm", "mixed"], ["in", "mm"]]) {
  C.setUnits({ wire: w, len: l });
  const d1 = C.design(Object.assign({}, C.DEFAULT_INPUT, C.PRESETS.find(p => p.id === "ke9esu-auto91").input)), d2 = C.design({ topology: "efhw49", coreId: "A:FT-240-43", conductorId: "awg18e" });
  const ct = C.coaxTrap({ coaxId: "custom" }), l1 = C.lcTrap({ wireId: "tube5" }), l2 = C.lcTrap({ coreType: "toroid" });
  out.units.push({ wire: w, len: l, design: [txt(d1), txt(d2)], coax: txt(ct), lc: [txt(l1), txt(l2)],
    labels: C.CONDUCTORS.map(c => C.wireLabel(c)).concat(C.COIL_WIRES.map(c => C.wireLabel(c))), dims: C.CORES.slice(0, 40).map(c => C.dimsTxt(c)),
    mm: [C.mmTxt(1245), C.mmTxt(35.55, null, 3), C.mmTxt(10, 0, 1), C.mmTxt(48.26, 1), C.mmTxt(2.13, 2)], inch: [C.inchTxt(2.375, 3), C.inchTxt(64.2, 1), C.inchTxt(1, 0), C.inchTxt(0.195, 3, "″")], dual: C.lenDual(1245) });
}
C.setUnits({ wire: "awg", len: "mixed" });
out.cores = C.CORES.map(c => [c.id, c.amidon || "", c.pn || "", c.db, c.mat, c.od, c.idia, c.h, c.al, c.ae, c.le, c.ve, c.alTol[0], c.alTol[1], c.alAmidon ? c.alAmidon.value : null, c.alCoil64 == null ? null : c.alCoil64, c.alWeb == null ? null : c.alWeb, c.alGeom == null ? null : c.alGeom, c.alPick || "", !!c.alSuspect, !!c.alDisagree, c.equiv ? c.equiv.id : "", c.equiv ? c.equiv.al : null, c.src, c.color || "", c.freq || "", (c.computed || []).join(","), c.webUrl || ""]);
out.materials = Object.keys(C.MATERIALS).map(k => { const m = C.MATERIALS[k]; return [k, m.kind, m.name, m.mui, m.bsat_mT, m.curieC, m.mu ? m.mu.length : 0, m.mu ? m.mu : []]; });
out.defaults = { design: C.DEFAULT_INPUT, coax: C.COAX_TRAP_DEFAULT, lc: C.LC_TRAP_DEFAULT };
require("fs").writeFileSync(process.argv[3] || "golden.json", JSON.stringify(out));
console.log("design", out.design.length, "coax", out.coax.length, "lc", out.lc.length, "eflw", out.eflw.length, "cores", out.cores.length);
