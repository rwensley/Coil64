// KE9ESU fork of Coil64 (GPL-3.0-or-later): dump Component Combiner solver.js results for the C++ port's differential test.
// Run: node ke9esu/tools/gen_golden_cc.js /path/to/solver.js tests/ke9esu/golden_cc.json
const CC = require(process.argv[2] || "/workspace/ke9esu_component_combiner/solver.js");
const out = { parse: [], fmt: [], solve: [] };
for (const s of ["4k7", "4.7k", "2u2", "100nF", "1meg", "1MEG", "4R7", "1k5Ω", "220 ohm", "10µF", "1e-6", "1234", "abc", "", "4.7k7", "-5", "k", "3.3pF", "68nH", "0.1u", "2M2", "1G"]) out.parse.push([s, CC.parseValue(s)]);
for (const v of [1, 999.95, 4700, 1.234e-9, 3.3e-6, 0.000999999, 1e7, 2.2e-12, 47e-6, 123456789]) for (const d of [3, 4, 6]) out.fmt.push([v, d, CC.fmt(v, d, "F")]);
const cases = [];
for (const [type, target] of [["R", 1234], ["R", 777], ["C", 3.3e-6], ["C", 1.234e-9], ["L", 123e-6], ["R", 2e7], ["C", 112.9e-12], ["L", 4.87e-6]])
  for (const topo of ["series", "parallel", "mixed"]) for (const [series, maxParts] of [["E6", 3], ["E12", 4], ["E24", 2], ["E96", 3]]) cases.push({ type, target, topo, series, maxParts });
cases.push({ type: "R", target: 1570, topo: "mixed", series: "E12", maxParts: 3, custom: [100, 220, 470, 1000, 2200] });
for (const p of cases) { const r = CC.solve(p); out.solve.push({ p, n: r.results.length, results: r.results.map(x => [x.expr, x.count, x.value, x.err]),
  acc: CC.sortResults(r.results, "accuracy", 1).slice(0, 5).map(x => x.expr), simple: CC.sortResults(r.results, "simple", 1).slice(0, 5).map(x => x.expr) }); }
require("fs").writeFileSync(process.argv[3] || "golden_cc.json", JSON.stringify(out));
console.log("cases", out.solve.length);
