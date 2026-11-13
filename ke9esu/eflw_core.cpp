/* eflw_core.cpp - see eflw_core.h. Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#include "eflw_core.h"
#include "trap_core.h"
#include <algorithm>

namespace ke9esu {

double halfWaveFt(double fMHz, double k) { return 491.8 * (k ? k : 0.95) / fMHz; }

QString classifyHalfWaves(double h, double tol)
{
    if (h < 0.4) return "short";
    double dN = std::fabs(h - jsRound(h)), dQ = std::fabs(h - (std::floor(h) + 0.5));
    if (jsRound(h) >= 1 && dN <= tol) return "high";
    if (dQ <= tol) return "low";
    return "medium";
}

std::vector<EflwRow> eflwAnalysis(double lengthFt, const EflwOpts &opts)
{
    double k = opts.k ? opts.k : 0.95, tol = (std::isnan(opts.tol) || !opts.tol) ? 0.08 : opts.tol;
    const std::vector<Band> &bs = opts.bands.empty() ? bands() : opts.bands;
    std::vector<EflwRow> out;
    for (const Band &b : bs) {
        std::vector<double> pts = logSpace(b.lo, b.hi, 9), hs; QMap<QString, bool> cls;
        for (double f : pts) { double h = lengthFt / halfWaveFt(f, k); hs.push_back(h); cls[classifyHalfWaves(h, tol)] = true; }
        QStringList kinds; for (const char *c : {"high", "low", "medium", "short"}) if (cls.value(c)) kinds << c;
        QString verdict, nine, fortynine;
        if (cls.value("high")) {
            double v = hs[0] < hs[8] && jsRound(hs[0]) != jsRound(hs[8]) ? hs[8] : jsRound(hs[4]);
            verdict = S("Very high Z near ") + rnd(v) + S("·λ/2 (≈2–5 kΩ)"); nine = "poor"; fortynine = "good";
        } else if (cls.value("low")) { verdict = S("Low Z near an odd λ/4 (≈30–150 Ω)"); nine = "fair (tuner)"; fortynine = "poor"; }
        else if (cls.value("short")) { verdict = S("Electrically short (<λ/5): low R, large −jX"); nine = "fair (tuner, lossy)"; fortynine = "poor"; }
        else { verdict = S("Moderate, reactive (≈200–1500 Ω)"); nine = "good (tuner)"; fortynine = "poor/fair"; }
        if (kinds.size() > 1) verdict += S(" — changes across the band");
        EflwRow r; r.band = b.id; r.lo = b.lo; r.hi = b.hi; r.halfWaves[0] = hs[0]; r.halfWaves[1] = hs[8];
        r.lambdaLo[0] = lengthFt / (2 * halfWaveFt(b.lo, k)); r.lambdaLo[1] = lengthFt / (2 * halfWaveFt(b.hi, k));
        r.kinds = kinds; r.verdict = verdict; r.unun9 = nine; r.unun49 = fortynine;
        out.push_back(r);
    }
    return out;
}

std::vector<AvoidZone> avoidZones(double maxFt, const EflwOpts &opts)
{
    double k = opts.k ? opts.k : 0.95, tol = (std::isnan(opts.tol) || !opts.tol) ? 0.05 : opts.tol;
    const std::vector<Band> &bs = opts.bands.empty() ? bands() : opts.bands;
    std::vector<AvoidZone> out;
    for (const Band &b : bs) for (int n = 1; n < 200; n++) {
        double lo = (n - tol) * halfWaveFt(b.hi, k), hi = (n + tol) * halfWaveFt(b.lo, k);
        if (lo > maxFt) break;
        out.push_back({b.id, n, lo, std::min(hi, maxFt * 1.2)});
    }
    std::stable_sort(out.begin(), out.end(), [](const AvoidZone &a, const AvoidZone &b) { return a.loFt < b.loFt; });
    return out;
}

std::vector<std::pair<double, double>> goodLengths(double minFt, double maxFt, const EflwOpts &opts)
{
    std::vector<AvoidZone> z = avoidZones(maxFt, opts); std::vector<std::pair<double, double>> gaps;
    double cur = minFt, minGap = opts.minGapFt ? opts.minGapFt : 2;
    for (const AvoidZone &r : z) {
        if (r.hiFt < cur) continue;
        if (r.loFt > cur && r.loFt - cur >= minGap && cur < maxFt) gaps.push_back({cur, std::min(r.loFt, maxFt)});
        cur = std::max(cur, r.hiFt);
    }
    if (maxFt - cur >= minGap) gaps.push_back({cur, maxFt});
    std::vector<std::pair<double, double>> out;
    for (auto &g : gaps) if (g.first < maxFt) out.push_back(g);
    return out;
}

TransformerAdvice recommendTransformer(double lengthFt, const std::vector<EflwRow> &rows)
{
    TransformerAdvice a;
    for (const EflwRow &r : rows) {
        if (r.kinds.contains("high")) a.highBands << r.band;
        if (r.kinds.contains("low")) a.lowBands << r.band;
        if (r.kinds.contains("short")) a.shortBands << r.band;
        if (r.kinds.size() == 1 && r.kinds[0] == "medium") a.moderateBands << r.band;
    }
    QString L = jsNum(lengthFt) + " ft";
    if (!rows.empty() && a.highBands.size() == (int)rows.size()) {
        a.topology = "efhw49"; a.presetId = "efhw49-80";
        a.reason = L + S(" is close to a half-wave multiple on every analysed band (") + a.highBands.join(", ") + S("): a 49:1 EFHW transformer suits it.");
    } else {
        a.topology = "ruthroff91"; a.presetId = "ke9esu";
        a.reason = L + S(" is non-resonant on most bands: keep a 9:1 unun + tuner (Ruthroff trifilar, or the 4:12 autotransformer preset).");
        if (!a.highBands.isEmpty()) a.reason += S(" High-Z (n·λ/2) on ") + a.highBands.join(", ") + S(" — trim/extend into a good gap or add a trap (see trap suggestions).");
        if (!a.lowBands.isEmpty()) a.reason += S(" Low-Z (odd λ/4) on ") + a.lowBands.join(", ") + S(" — the 9:1 makes this a few ohms; the tuner works harder there.");
    }
    return a;
}

std::vector<TrapSuggestion> trapSuggestions(double lengthFt, const std::vector<EflwRow> &rows, double k)
{
    std::vector<TrapSuggestion> out;
    for (const EflwRow &r : rows) {
        if (!r.kinds.contains("high")) continue;
        double fc = bandFrequency(r.band, "mid"); if (std::isnan(fc)) continue;
        double hw = halfWaveFt(fc, k), best = NaN, bestH = NaN;
        for (double h = std::floor(lengthFt / hw) + 0.75; h >= 0.75; h -= 0.5) {
            double D = h * hw;
            if (D > lengthFt - 3) continue;
            bool ok = classifyHalfWaves(D / halfWaveFt(r.lo, k), 0.08) == "medium" && classifyHalfWaves(D / halfWaveFt(r.hi, k), 0.08) == "medium";
            if (ok) { best = D; bestH = h; break; }
        }
        if (std::isnan(best)) continue;
        TrapSuggestion t; t.band = r.band; t.fMHz = fc; t.distanceFt = best; t.halfWaves = bestH;
        t.text = r.band + ": trap resonant at " + toFixed(fc, 3) + " MHz, " + toFixed(best, 1) + " ft (" + toFixed(best * 0.3048, 2) + S(" m) from the feed point → the wire looks ") + toFixed(bestH, 2) + S("·λ/2 long on ") + r.band + S(" (moderate Z) instead of a half-wave multiple. Rule of thumb — the trap's loading shortens it a little; confirm with an analyser.");
        out.push_back(t);
    }
    return out;
}

} // namespace ke9esu
