/* ke9esu_util.cpp - see ke9esu_util.h. Copyright (C) 2026 KE9ESU. GPL-3.0-or-later. */
#include "ke9esu_util.h"
#include <QLocale>

namespace ke9esu {

QString toFixed(double v, int d)
{
    if (std::isnan(v)) return QStringLiteral("NaN");
    if (std::isinf(v)) return v > 0 ? QStringLiteral("Infinity") : QStringLiteral("-Infinity");
    // Qt uses the double-conversion library: exact decimal expansion, ties rounded away from zero = JS toFixed.
    QString s = QString::number(v, 'f', d);
    if (s.startsWith('-')) {           // JS keeps the sign of tiny negatives ("-0.0"); so does Qt. "-0" of -0.0 is not:
        if (v == 0) s = s.mid(1);       // (-0).toFixed(1) === "0.0"
    }
    return s;
}

double toFixedNum(double v, int d) { return toFixed(v, d).toDouble(); }

double toPrecisionNum(double v, int p)
{
    if (!std::isfinite(v) || v == 0) return v;
    return QString::number(v, 'e', p - 1).toDouble();
}

QString jsNum(double v)
{
    if (std::isnan(v)) return QStringLiteral("NaN");
    if (std::isinf(v)) return v > 0 ? QStringLiteral("Infinity") : QStringLiteral("-Infinity");
    if (v == 0) return QStringLiteral("0");
    QString neg;
    if (v < 0) { neg = QStringLiteral("-"); v = -v; }
    // shortest round-trip digits in exponent form, e.g. "1.2345e+02"
    QString e = QString::number(v, 'e', QLocale::FloatingPointShortest);
    int ePos = e.indexOf('e');
    QString mant = e.left(ePos), digits = mant;
    digits.remove('.');
    int exp10 = e.mid(ePos + 1).toInt();
    while (digits.length() > 1 && digits.endsWith('0')) digits.chop(1);
    int k = digits.length(), n = exp10 + 1;   // ECMAScript: value = 0.digits * 10^n
    QString out;
    if (k <= n && n <= 21) out = digits + QString(n - k, '0');
    else if (0 < n && n <= 21) out = digits.left(n) + "." + digits.mid(n);
    else if (-6 < n && n <= 0) out = "0." + QString(-n, '0') + digits;
    else {
        QString es = (n - 1 >= 0 ? "+" : "-") + QString::number(std::abs(n - 1));
        out = (k == 1 ? digits : digits.left(1) + "." + digits.mid(1)) + "e" + es;
    }
    return neg + out;
}

} // namespace ke9esu
