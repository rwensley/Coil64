/* ke9esu_util.h - KE9ESU Balun, Unun & Trap tools for Coil64: number/text helpers that reproduce JavaScript
   formatting exactly, so the C++ port prints the same text as the KE9ESU web app.
   Copyright (C) 2026 KE9ESU (Ron Wensley). Part of the KE9ESU fork of Coil64 (C) Valery Kustarev.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public
License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later
version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
details. You should have received a copy of the GNU General Public License along with this program.
If not, see <https://www.gnu.org/licenses/>. */
#ifndef KE9ESU_UTIL_H
#define KE9ESU_UTIL_H

#include <QString>
#include <QStringList>
#include <cmath>
#include <limits>

namespace ke9esu {

static const double PI = 3.14159265358979323846;
static const double SQRT2 = 1.41421356237309504880;
static const double NaN = std::numeric_limits<double>::quiet_NaN();
inline bool isNull(double v) { return std::isnan(v); }

/** UTF-8 literal -> QString. */
inline QString S(const char *utf8) { return QString::fromUtf8(utf8); }
/** JavaScript Math.round (half towards +infinity). */
inline double jsRound(double x) { return std::floor(x + 0.5); }
/** JavaScript Number.prototype.toFixed(d) (exact decimal value, ties away from zero). */
QString toFixed(double v, int d);
/** +(v.toFixed(d)) */
double toFixedNum(double v, int d);
/** +(v.toPrecision(p)) */
double toPrecisionNum(double v, int p);
/** JavaScript String(number): shortest round-trip representation in ECMAScript format. */
QString jsNum(double v);
/** jsNum(Math.round(v)) */
inline QString rnd(double v) { return jsNum(jsRound(v)); }
/** Join helper. */
inline QString join(const QStringList &l, const QString &sep) { return l.join(sep); }

} // namespace ke9esu
#endif
