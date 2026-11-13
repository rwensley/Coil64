/* combiner_core.h - KE9ESU Component Combiner core: find series/parallel/mixed networks of standard parts (E6-E192
   or a custom junk-box list, up to 6 parts) that hit a target R, C or L. C++ port of Ron's ComponentCombiner solver.js
   (meet-in-the-middle over log-bucketed tables of 1..k-part networks).
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. */
#ifndef KE9ESU_COMBINER_CORE_H
#define KE9ESU_COMBINER_CORE_H

#include <QString>
#include <QStringList>
#include <vector>
#include <functional>

namespace ke9esu {
namespace cc {

struct PartType { QString name, unit; double min, max; };
const PartType &partType(QChar type);        // 'R', 'C', 'L'
std::vector<double> eSeries(const QString &name, bool *ok = nullptr);   // E6 E12 E24 E48 E96 E192
double parseValue(const QString &str);        // "4k7", "2u2", "100nF", "1meg", "4R7" ... ; NaN if invalid
struct ParsedList { std::vector<double> values; QStringList bad; };
ParsedList parseList(const QString &text);
std::vector<double> buildLeaves(QChar type, const QString &seriesName, const std::vector<double> &custom = {});
QString fmt(double v, int digits = 4, const QString &unit = QString());

struct Node { bool leaf = true; double v = 0, x = 0; int op = 0; std::vector<Node> kids; };   // op 1 = "+" (series), 2 = "||" (parallel)
struct Result { Node tree; std::vector<double> parts; int count; double value, err; QString expr; };
struct Params {
    QChar type = 'R'; double target = 0; int maxParts = 3; QString topo = "mixed";   // series | parallel | mixed
    QString series = "E12"; std::vector<double> custom; double window = 1e4, budget = 3e7; int thin = 1500;
};
struct Solution { std::vector<Result> results; int leaves = 0; std::vector<int> tableSizes; qint64 ms = 0; QString error; };
/** progress(phase, fraction) may return false to abort. */
Solution solve(const Params &p, const std::function<bool(const QString &, double)> &progress = nullptr);
std::vector<Result> sortResults(const std::vector<Result> &results, const QString &mode, double tolPct);   // mode "accuracy" | "simple"
double evalTree(const Node &n, QChar type);   // independent evaluator (tests)

/* ---- display helpers shared by the Coil64 window, its build sheet and the tests (same text as ComponentCombiner.html) */
QString seriesTolerance(const QString &series);   // "±10%"; E192 "±0.5%, ±0.25% or ±0.1%"; "" if unknown
QString seriesLabel(const QString &series);       // "E12 (10%)", "E192 (0.5% / 0.25% / 0.1%)" - the web app's option text
QString seriesHint(const QString &series);        // "Parts in this series are typically ±10%"
double seriesTolFrac(const QString &series);      // usual part tolerance as a fraction (E192: 0.005); NaN if unknown
QString pctText(double err);                      // web app pct(): "0 % (exact)", "+0.040 %", "−1.25 %" (U+2212 minus)
bool withinTol(double err, double tolPct);        // |err| <= tol (same 1e-12 slack as the solver's ranking)
QString tolBadge(double err, double tolPct);      // "within ±1%" / "outside ±1%"
QString partsSummary(const std::vector<double> &parts, const QString &unit);   // "2× 1 kΩ, 470 Ω"
/* Spread of a network's value caused by its parts' own tolerance t (fraction). worst = all parts at the same limit
   (for any series/parallel network this equals t exactly); typical = independent random errors (root-sum-square of
   the relative sensitivities), which is smaller for networks of several similar parts. */
struct Spread { double worst = 0, typical = 0; };
Spread partSpread(const Node &n, QChar type, double t);
int leafCount(const Node &n);

} // namespace cc
} // namespace ke9esu
#endif
