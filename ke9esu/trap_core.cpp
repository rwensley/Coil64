/* trap_core.cpp - KE9ESU coax-trap and LC-trap core, C++ port of core.js v1.3.3 (see trap_core.h).
   Copyright (C) 2026 KE9ESU (Ron Wensley). GPL-3.0-or-later. Coil-maths ports: Coil64 (C) Valery Kustarev, GPL-3.0-or-later. */
#include "trap_core.h"
#include "combiner_core.h"
#include <QRegularExpression>
#include <QMap>
#include <algorithm>

namespace ke9esu {

const std::vector<double> CAP_RATINGS = {500, 1000, 2000, 3000, 5000, 7500, 10000, 15000, 20000, 30000};
static const double RHO_CU = 1.7241e-8, CHI_CU = -9.56e-6, E0 = 8.854187818;

/* Coax database. pfFt & vf from Belden/Times/MIL datasheets as listed by K7MEM & VE6YP; odIn = jacket OD; centerMM = centre
   conductor dia; shieldMM = braid (dielectric) dia; vMaxRms = typical max operating voltage. pF/ft ~= 1016.7/(Z0*VF). */
const std::vector<Coax> &coaxList()
{
    static const std::vector<Coax> C = {
        {"rg58", "RG-58/U solid PE (Belden 8240)", 53.5, 28.5, 0.66, 0.193, 0.81, 2.95, 1400, "solid PE", false, false, "Belden 8240"},
        {"rg58a", "RG-58A/U solid PE (Belden 8259)", 50, 30.8, 0.66, 0.193, 0.90, 2.95, 1400, "solid PE", false, false, "Belden 8259"},
        {"rg58c", "RG-58C/U solid PE (Belden 8262, MIL)", 50, 30.8, 0.66, 0.195, 0.90, 2.95, 1900, "solid PE", false, false, "Belden 8262 / MIL-C-17"},
        {"rg58foam", "RG-58A/U foam (Belden 8219)", 50, 26.5, 0.73, 0.198, 0.90, 2.95, 1000, "foam PE", true, false, "Belden 8219"},
        {"rg174", "RG-174/U (Belden 8216)", 50, 30.8, 0.66, 0.101, 0.48, 1.52, 1100, "solid PE", false, true, "Belden 8216"},
        {"rg316", "RG-316/U PTFE (Belden 84316)", 50, 29.0, 0.695, 0.098, 0.53, 1.52, 1200, "PTFE", false, true, "Belden 84316"},
        {"rg400", "RG-400 PTFE double braid (M17/128)", 50, 29.4, 0.695, 0.195, 0.99, 2.95, 1900, "PTFE", false, false, "MIL-C-17 M17/128"},
        {"rg142", "RG-142B PTFE (Belden 83242)", 50, 29.2, 0.695, 0.195, 0.94, 2.95, 1900, "PTFE", false, true, "Belden 83242"},
        {"rg8x", "RG-8X foam (Belden 9258)", 50, 25.3, 0.82, 0.242, 1.47, 3.94, 600, "foam PE", true, false, "Belden 9258"},
        {"rg213", "RG-213/U solid PE (Belden 8267)", 50, 30.8, 0.66, 0.405, 2.26, 7.24, 5000, "solid PE", false, false, "Belden 8267 / MIL-C-17"},
        {"rg8", "RG-8/U low-loss (Belden 9913)", 50, 24.6, 0.84, 0.405, 2.74, 7.24, 600, "semi-solid air PE", true, false, "Belden 9913"},
        {"rg59", "RG-59B/U 75 ohm (Belden 8263)", 75, 20.5, 0.66, 0.242, 0.58, 3.71, 1700, "solid PE", false, true, "Belden 8263"},
        {"lmr240", "LMR-240 (Times Microwave)", 50, 24.2, 0.84, 0.240, 1.42, 3.81, 1000, "foam PE", true, false, "Times Microwave LMR-240"},
        {"lmr400", "LMR-400 (Times Microwave)", 50, 23.9, 0.85, 0.405, 2.74, 7.24, 1600, "foam PE", true, true, "Times Microwave LMR-400"}
    };
    return C;
}
const Coax *coaxById(const QString &id) { for (const Coax &c : coaxList()) if (c.id == id) return &c; return &coaxList()[2]; }

const std::vector<PvcForm> &pvcForms()
{   // PVC pipe coil forms, ASTM D1785 Schedule 40 actual OD (inches)
    static const std::vector<PvcForm> F = {
        {"0.5", "1/2\" Sch 40 PVC", 0.840}, {"0.75", "3/4\" Sch 40 PVC", 1.050}, {"1", "1\" Sch 40 PVC", 1.315}, {"1.25", "1-1/4\" Sch 40 PVC", 1.660},
        {"1.5", "1-1/2\" Sch 40 PVC", 1.900}, {"2", "2\" Sch 40 PVC", 2.375}, {"2.5", "2-1/2\" Sch 40 PVC", 2.875}, {"3", "3\" Sch 40 PVC", 3.500},
        {"3.5", "3-1/2\" Sch 40 PVC", 4.000}, {"4", "4\" Sch 40 PVC", 4.500}};
    return F;
}

const std::vector<Wire> &coilWires()
{   // Bare copper conductors for air coils: dia mm (bare), od mm (with enamel)
    static std::vector<Wire> W;
    if (W.empty()) {
        auto w = [](const char *id, const char *label, double dia, double od) { Wire x; x.id = id; x.label = S(label); x.dia = dia; x.od = od; return x; };
        W = {w("awg18", "AWG 18 enamelled", 1.024, 1.09), w("awg16", "AWG 16 enamelled", 1.291, 1.37), w("awg14", "AWG 14 bare/enamel", 1.628, 1.71),
             w("awg12", "AWG 12 bare/enamel", 2.053, 2.13), w("awg10", "AWG 10 bare/enamel", 2.588, 2.67), w("tube3", "1/8\" copper tubing", 3.175, 3.175),
             w("tube5", "3/16\" copper tubing", 4.763, 4.763), w("tube6", "1/4\" copper tubing", 6.35, 6.35)};
    }
    return W;
}
const Wire *coilWireById(const QString &id) { for (const Wire &w : coilWires()) if (w.id == id) return &w; return &coilWires()[3]; }

const std::vector<CapType> &capTypes()
{
    static const std::vector<CapType> T = {
        {"np0", S("C0G/NP0 HV ceramic disc"), 1000, S("Low drift; check the RF-current rating — small discs heat at >1–2 A.")},
        {"mica", S("Silver/dipped mica"), 1500, S("Stable; small parts handle ~1 A RF and 500–1000 V — fine at 100 W with margin checks.")},
        {"doorknob", S("Transmitting 'doorknob' ceramic"), 2500, S("kV ratings and several amps RF — the usual choice above a few hundred watts.")},
        {"vacuum", S("Vacuum capacitor"), 10000, S("Highest Q and voltage; costly.")},
        {"coaxcap", S("Coax capacitor (open stub)"), 600, S("Cut coax as a capacitor; W8JI measured Q≈600 for RG-58 — it is a stub, so C rises with frequency.")}};
    return T;
}
const CapType *capType(const QString &id) { for (const CapType &c : capTypes()) if (c.id == id) return &c; return nullptr; }

const std::vector<double> *eSeries(const QString &name)
{
    static const std::vector<double> E6 = {1.0, 1.5, 2.2, 3.3, 4.7, 6.8};
    static const std::vector<double> E12 = {1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9, 4.7, 5.6, 6.8, 8.2};
    static const std::vector<double> E24 = {1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1};
    if (name == "E6") return &E6; if (name == "E12") return &E12; if (name == "E24") return &E24; return nullptr;
}

/* ---------------------------------------------------------------------------------------- coil maths */
double wheelerUH(double rIn, double lenIn, double n) { return rIn * rIn * n * n / (9 * rIn + 10 * lenIn); }
double nagaokaK(double x)
{
    if (x == 0) return 1;
    double zk = 2 / (PI * x), K0 = 1 / (std::log(8 / PI) - 0.5), k2 = 24 / (3 * PI * PI - 16);
    double w = -0.47 / std::pow(0.755 + x, 1.44), p = K0 + 3.437 / x + k2 / (x * x) + w;
    return zk * (std::log(1 + 1 / zk) + 1 / p);
}
double sheetUH(double Dmm, double lenMM, double n) { double D = Dmm / 1000, l = lenMM / 1000; return MU0 * PI * D * D / 4 * n * n / l * nagaokaK(D / l) * 1e6; }
double rosaKs(double x) { return 1.25 - std::log(2 * x); }
double rosaKm(double n)
{
    double n2 = n * n, n3 = n2 * n, n5 = n3 * n2, n7 = n5 * n2, n9 = n7 * n2;
    return std::log(2 * PI) - 1.5 - std::log(n) / (6 * n) - 0.33084236 / n - 1 / (120 * n3) + 1 / (504 * n5) - 0.0011923 / n7 + 0.0005068 / n9;
}
double nagaokaRosaUH(double Dk, double p, double dw, double n) { double ls = sheetUH(Dk, n * p, n); return ls - MU0 * (Dk / 2000) * n * (rosaKs(p / dw) + rosaKm(n)) * 1e6; }

static double ingrnd(double phi, double kphitheta, double sinpsi, double cos2psi, double rr, double y)
{
    return (1 + cos2psi * (std::cos(kphitheta) - 1)) / std::sqrt(2 * rr * (1 - std::cos(kphitheta)) + (sinpsi * phi - y) * (sinpsi * phi - y));
}
double heliCoilS(double Lw, double psi, double r, double dw, double accuracy)
{
    double MaxErr = std::pow(10, -accuracy), g = std::exp(-0.25) * dw / 2, rr = r * r, psio = 0.5 * PI - psi, ThetaO, Y0;
    if (Lw > 2 * PI * r) { double sinpsic = dw / (2 * PI * r), psic = std::atan(sinpsic / std::sqrt(1 - sinpsic * sinpsic)); if (psi < psic) return -1; }
    if (psi == 0) { ThetaO = 0; Y0 = g; }
    else if (psio == 0) { double cto = 1 - g * g / (2 * rr); ThetaO = -std::fabs(std::atan(std::sqrt(1 - cto * cto) / cto)); Y0 = 0; }
    else {
        double k1 = g * g / (2 * rr) - 1, k2 = std::tan(psio); k2 = 0.5 * k2 * k2;
        double t1 = g / r * std::sin(psi), t0;
        do { t0 = t1; t1 = t0 - (k1 + std::cos(t0) - k2 * t0 * t0) / (-std::sin(t0) - 2 * k2 * t0); } while (std::fabs(t1 - t0) > 1e-12);
        ThetaO = -std::fabs(t1); Y0 = std::sqrt(g * g - 2 * rr * (1 - std::cos(ThetaO)));
    }
    double c2s = std::cos(psi) * std::cos(psi), ss = std::sin(psi), k = std::cos(psi) / r;
    double a = 0, b = Lw / 32768, grand = 0; if (b > Lw) b = Lw;
    while (a < Lw) {
        double dx = b - a; double m = 1; double CurrentErr = 2 * MaxErr, kat = k * a, kbt = k * b, Integral = 0;
        double Sum2 = (Lw - a) * ingrnd(-a, -kat - ThetaO, ss, c2s, rr, Y0) + ingrnd(a, kat - ThetaO, ss, c2s, rr, Y0) +
                      (Lw - b) * ingrnd(-b, -kbt - ThetaO, ss, c2s, rr, Y0) + ingrnd(b, kbt - ThetaO, ss, c2s, rr, Y0);
        double LastIntg = Sum2 / 2 * dx;
        while (CurrentErr > MaxErr || m < 512) {
            m *= 2; dx = dx / 2; double Sum = 0;
            for (double i = 1; i <= m; i += 2) { double phi = i * dx + a, kpt = k * phi; Sum += (Lw - phi) * (ingrnd(-phi, -kpt - ThetaO, ss, c2s, rr, Y0) + ingrnd(phi, kpt - ThetaO, ss, c2s, rr, Y0)); }
            Integral = (4 * Sum + Sum2) * dx / 3;
            CurrentErr = std::fabs(Integral / LastIntg - 1); LastIntg = Integral; Sum2 += Sum * 2;
            if (m > (1 << 20)) break;
        }
        grand += Integral; a = b; b *= 2; if (b > Lw) b = Lw;
    }
    return 1e-1 * grand;
}
Helical helicalUH(double N, double p, double Dk, double dw, double accuracy)
{
    double pm = p / 1000, D = Dk / 1000, sinpsi = pm / (PI * D), psi = std::atan(sinpsi / std::sqrt(1 - sinpsi * sinpsi));
    double lw = PI * N * D / std::cos(psi);
    return {heliCoilS(lw, psi, D / 2, dw / 1000, accuracy ? accuracy : 4), lw};
}

/* Port of Coil64 bessel.cpp + resolve_srf_cs.cpp (D W Knight self-capacitance and SRF) */
static double bessi0(double x) { double ax = std::fabs(x), y; if (ax < 3.75) { y = x / 3.75; y *= y; return 1 + y * (3.5156229 + y * (3.0899424 + y * (1.2067492 + y * (0.2659732 + y * (0.360768e-1 + y * 0.45813e-2))))); } y = 3.75 / ax; return (0.39894228 + y * (0.1328592e-1 + y * (0.225319e-2 + y * (-0.157565e-2 + y * (0.916281e-2 + y * (-0.2057706e-1 + y * (0.2635537e-1 + y * (-0.1647633e-1 + y * 0.392377e-2)))))))) * std::exp(ax) / std::sqrt(ax); }
static double bessi1(double x) { double ax = std::fabs(x), y, a; if (ax < 3.75) { y = x / 3.75; y *= y; return ax * (0.5 + y * (0.87890594 + y * (0.51498869 + y * (0.15084934 + y * (0.2658733e-1 + y * (0.301532e-2 + y * 0.32411e-3)))))); } y = 3.75 / ax; a = 0.2282967e-1 + y * (-0.2895312e-1 + y * (0.1787654e-1 - y * 0.420059e-2)); a = 0.39894228 + y * (-0.3988024e-1 + y * (-0.362018e-2 + y * (0.163801e-2 + y * (-0.1031555e-1 + y * a)))); return a * std::exp(ax) / std::sqrt(ax); }
static double bessk0(double x) { double y; if (x <= 2) { y = x * x / 4; return -std::log(x / 2) * bessi0(x) + (-0.57721566 + y * (0.42278420 + y * (0.23069756 + y * (0.3488590e-1 + y * (0.262698e-2 + y * (0.10750e-3 + y * 0.74e-5)))))); } y = 2 / x; return std::exp(-x) / std::sqrt(x) * (1.25331414 + y * (-0.7832358e-1 + y * (0.2189568e-1 + y * (-0.1062446e-1 + y * (0.587872e-2 + y * (-0.251540e-2 + y * 0.53208e-3)))))); }
static double bessk1(double x) { double y; if (x <= 2) { y = x * x / 4; return std::log(x / 2) * bessi1(x) + (1 / x) * (1 + y * (0.15443144 + y * (-0.67278579 + y * (-0.18156897 + y * (-0.1919402e-1 + y * (-0.110404e-2 + y * (-0.4686e-4))))))); } y = 2 / x; return std::exp(-x) / std::sqrt(x) * (1.25331414 + y * (0.23498619 + y * (-0.3655620e-1 + y * (0.1504268e-1 + y * (-0.780353e-2 + y * (0.325614e-2 + y * (-0.68245e-3))))))); }
static double r0tan(double x) { return 59.9584916 * std::sqrt(2 * bessi0(x) * bessk0(x) * bessi1(x) * bessk1(x)); }
static double ollendf(double x) { return std::sqrt(bessi0(x) * bessk0(x) / (bessi1(x) * bessk1(x))); }
double vfNom(double lod, double ei, double ex)
{
    double x = PI / (2 * lod); x = x + 0.117 * std::exp(-22 * std::exp(-0.75 * lod));
    double kL = nagaokaK(1 / lod), erad = (ex / 2) * (1 + kL + (ei / ex) * (1 - kL)), vf0 = 1 / std::sqrt(erad), vfh = vf0 * ollendf(x);
    double Cff = erad * 2 * E0 / (1 + std::log(1 + lod)), arg = 1.015 + lod * lod, Caf = E0 * (ei + ex) / std::log(arg + std::sqrt(arg * arg - 1));
    double Rotn = vf0 * r0tan(x), b = 1e6 * lod / (299.792458 * PI * (Cff + Caf) * Rotn), a = PI / (2 * vfh), z = vfh, y = 1, diff = 1; int n = 0;
    while (std::fabs(diff) > 1e-9 || n < 255) { y = (1 / a) * std::atan(b / z); diff = z - y; double der = -(b / a) / (z * z + b * b); z = z + diff / (der - 1); n++; if (n > 5000) break; }
    return y;
}
double coilSRF_MHz(double lk, double Dk, double lwM) { return vfNom(lk / Dk, 1, 1) * 299.792458 / (2 * lwM); }
static double ctdw(double ff, double ei, double ex) { double kct = 1 / nagaokaK(1 / ff) - 1; return 11.27350207 * ex * ff * (1 + kct * (1 + ei / ex) / 2); }
static double ciae(double ff, double ei, double ex) { return 17.70837564 * (ei + ex) / std::log(1 + PI * PI * ff); }
double coilCs_pF(double p, double Dk, double lk)
{
    double P = p / 1000, D = Dk / 1000, L = lk / 1000, s = P / (PI * D), c = std::sqrt(1 - s * s);
    return (ctdw(L / D, 1, 1) / (c * c) + ciae(L / D, 1, 1)) * D;
}

/* Port of Coil64 resolve_q.cpp solve_Qr (Arnold proximity tables + Knight skin effect), copper */
static double linTab(const std::vector<double> &za, const std::vector<double> &ya, double z)
{
    size_t zi = 0; double r = 0;
    while (z >= za[zi] && zi < za.size() - 1) { zi++; r = (ya[zi] - ya[zi - 1]) / (za[zi] - za[zi - 1]) * (z - za[zi - 1]) + ya[zi - 1]; }
    return r;
}
static const std::vector<double> T7Z = {0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0, 4.2, 4.4, 4.6, 4.8, 5.0, 5.2, 5.4, 5.6, 5.8, 6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0, 9.5, 10.0};
static const std::vector<double> T7F = {0.001, 0.003, 0.005, 0.008, 0.013, 0.02, 0.029, 0.041, 0.055, 0.072, 0.092, 0.114, 0.139, 0.167, 0.196, 0.226, 0.257, 0.288, 0.319, 0.349, 0.378, 0.406, 0.432, 0.457, 0.48, 0.501, 0.539, 0.571, 0.599, 0.622, 0.643, 0.661, 0.677, 0.691, 0.704, 0.716, 0.727, 0.737, 0.746, 0.755, 0.763, 0.782, 0.797, 0.811, 0.823, 0.833, 0.843, 0.851, 0.858};
static const std::vector<double> T7C = {0.001, 0.002, 0.004, 0.006, 0.010, 0.015, 0.022, 0.03, 0.041, 0.053, 0.067, 0.083, 0.101, 0.12, 0.14, 0.16, 0.18, 0.199, 0.218, 0.235, 0.251, 0.265, 0.278, 0.289, 0.299, 0.307, 0.32, 0.33, 0.337, 0.343, 0.348, 0.353, 0.357, 0.361, 0.365, 0.37, 0.374, 0.378, 0.382, 0.386, 0.389, 0.397, 0.404, 0.41, 0.416, 0.42, 0.425, 0.428, 0.432};
static const std::vector<double> T8N = {2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 18, 20, 25, 30, 35, 40, 45, 50, 67, 100, 125, 167, 250, 500};
static const std::vector<double> T8V = {-1, -0.25, 0.21, 0.52, 0.76, 0.94, 1.09, 1.22, 1.33, 1.5, 1.63, 1.73, 1.82, 1.91, 2.06, 2.17, 2.26, 2.33, 2.39, 2.44, 2.56, 2.71, 2.77, 2.85, 2.94, 3.05};
static const std::vector<double> T8W = {1, 2.4, 3.5, 4.3, 4.9, 5.4, 5.9, 6.3, 6.6, 7.0, 7.4, 7.8, 8.0, 8.2, 8.6, 8.9, 9.1, 9.3, 9.4, 9.5, 9.7, 10.0, 10.2, 10.3, 10.4, 10.6};
static const std::vector<double> T3N = {2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 18, 20, 25, 34, 50, 100};
static const std::vector<double> T3P = {2.29, 1.79, 1.48, 1.28, 1.12, 1.01, 0.91, 0.84, 0.78, 0.68, 0.60, 0.54, 0.50, 0.46, 0.39, 0.31, 0.22, 0.12};
static const std::vector<double> T5R = {0.1, 0.2, 0.3, 0.4, 0.6, 0.8, 1, 1.2, 1.4, 1.6, 1.8, 2, 2.2, 2.4, 2.6, 2.8, 3, 3.2, 3.4, 3.6, 3.8, 4, 4.2, 4.4, 4.6, 4.8, 5.0, 5.6, 6.25, 7.14, 7.7, 8.33, 9.1, 10, 11.1, 12.5, 14.28, 16.66, 20, 25, 33.33, 50, 100};
static const std::vector<double> T5G = {0.06, 0.18, 0.32, 0.49, 0.85, 1.23, 1.61, 1.99, 2.36, 2.71, 3.04, 3.35, 3.63, 3.90, 4.16, 4.39, 4.61, 4.82, 5.01, 5.19, 5.36, 5.51, 5.66, 5.80, 5.93, 6.06, 6.18, 6.46, 6.76, 7.09, 7.26, 7.43, 7.61, 7.79, 7.98, 8.17, 8.37, 8.57, 8.78, 9.00, 9.21, 9.43, 9.65};
static const std::vector<double> T4R = {0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.2, 2.4, 2.6, 2.8, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0};
static const std::vector<double> T4A = {3.29, 3.24, 3.13, 2.99, 2.83, 2.67, 2.51, 2.36, 2.21, 2.07, 1.95, 1.83, 1.72, 1.63, 1.54, 1.46, 1.28, 1.14, 1.02, 0.93, 0.85, 0.78, 0.72, 0.67, 0.63, 0.59};
static const std::vector<double> T4B = {0, -0.03, -0.04, -0.04, -0.03, -0.02, -0.01, 0, 0.01, 0.02, 0.03, 0.04, 0.04, 0.05, 0.06, 0.06, 0.08, 0.09, 0.1, 0.11, 0.12, 0.12, 0.13, 0.13, 0.13, 0.14};

double lookupPsi(double Df, double dw, double pt, double N, double fkHz)
{
    double d = dw / 10, P = pt / 10, rc = Df / 20, f = fkHz * 1e3, si = 1e-11 / RHO_CU, eta = d / P, zeta = P / rc, ro = (N - 1) * zeta;
    double z = PI * d * std::sqrt(2 * f * si);
    double Phi = z > 10 ? 1 - SQRT2 / z : linTab(T7Z, T7F, z), Chi = z > 10 ? 0.5 - 1 / (z * SQRT2) : linTab(T7Z, T7C, z);
    double v = N > 500 ? 3.29 : linTab(T8N, T8V, N), w = N > 500 ? 10.8 : linTab(T8N, T8W, N);
    double alpha = (1 + 0.209 * w * Phi * Phi * std::pow(eta, 4)) / (1 + 0.084 * w * Phi * Phi * std::pow(eta, 4));
    double betta = 1 / std::pow(1 - 0.278 * v * Phi * eta * eta, 1.8), gamma = 1 / std::pow(1 + 0.385 * v * Phi * eta * eta, 1.3);
    double pm = N > 100 ? 1e-6 : linTab(T3N, T3P, N);
    double a = ro > 8 ? 19 / (4 * ro) : linTab(T4R, T4A, ro), b = ro > 8 ? 4.0 / 19 - 3 / (5 * ro) : linTab(T4R, T4B, ro);
    double u1 = a - 1 / (1 / pm + ro * b), g = ro > 100 ? 9.87 : linTab(T5R, T5G, ro);
    double u2 = g / (1 + (0.42 + 0.3 / ro) * std::sqrt(zeta) + (1.35 + 2.8 / std::sqrt(ro)) * std::pow(zeta, 1.5));
    return alpha + eta * eta * Chi * (betta * u1 + gamma * u2);
}
double skinFactor(double fkHz, double dw)
{
    double f = fkHz * 1e3, r = dw / 2000, di = std::sqrt(RHO_CU / (f * PI * MU0 * (1 + CHI_CU))), dp = di * (1 - std::exp(-r / di));
    double z = 0.62006 * r / di, y = 0.189774 / std::pow(1 + 0.272481 * std::pow(std::pow(z, 1.82938) - std::pow(z, -0.99457), 2), 1.0941);
    return r * r / ((2 * r * dp - dp * dp) * (1 + y));
}
CoilQ coilQ(double I, double Df, double pm, double dw, double fa, double fsr, double N, double Cs)
{
    double fkHz = fa * 1e3, f = fkHz * 1e3, D = Df / 1000, r = dw / 2000, p = pm / 1000;
    double wl = PI * N * std::sqrt(D * D + p * p / 4), Rdc = RHO_CU * wl / (PI * r * r);
    double Xi = skinFactor(fkHz, dw), Psi = lookupPsi(Df, dw, pm, N, fkHz);
    double Rac = Rdc * (1 + ((Xi - 1) * Psi * (N - 1 + 1 / Psi)) / N) / std::pow(1 - (fkHz * I * Cs) / 2.53e7, 2);
    double Rl = 2 * PI * f * I * 1e-6, Rc = 1 / (2 * PI * f * Cs * 1e-12), Rind = 1 / (1 / Rl + 1 / Rc);
    double Q = (Rind / Rac) * (1 - fa / fsr);
    return {Q, Rind / Q, Rac, Rdc};
}
double wireRac(double lenM, double dwMM, double fMHz) { double r = dwMM / 2000; return RHO_CU * lenM / (PI * r * r) * skinFactor(fMHz * 1e3, dwMM); }

QString eSeriesTolerance(const QString &series) { return series.startsWith('E') ? cc::seriesTolerance(series) : QString(); }
QString eSeriesLabel(const QString &series) { return eSeriesTolerance(series).isEmpty() ? series : cc::seriesLabel(series); }
QString eSeriesHint(const QString &series) { return eSeriesTolerance(series).isEmpty() ? QString() : cc::seriesHint(series); }
double eSeriesTolPct(const QString &series)
{
    static const QMap<QString, double> T = {{"E6", 20}, {"E12", 10}, {"E24", 5}, {"E48", 2}, {"E96", 1}, {"E192", 0.5}};
    return T.value(series, 0);
}
QString pctSigned(double v, int d) { QString t = toFixed(std::fabs(v), d); return (t.toDouble() == 0 ? QString() : v < 0 ? S("−") : QString("+")) + t + " %"; }
SnapInfo snapInfo(const QString &series, double exactPF, double pF, double fMHz)
{
    SnapInfo s; s.series = series; s.label = eSeriesLabel(series); s.tolPct = eSeriesTolPct(series); s.tolText = eSeriesTolerance(series); s.hint = eSeriesHint(series);
    double t = s.tolPct / 100; s.exactPF = exactPF; s.pF = pF; s.errPct = (pF - exactPF) / exactPF * 100; s.fMHz = fMHz;
    s.fLoMHz = fMHz / std::sqrt(1 + t); s.fHiMHz = fMHz / std::sqrt(1 - t); return s;
}
QString spreadTxt(const SnapInfo &sn, int d)
{
    return toFixed(sn.fLoMHz, d) + S("–") + toFixed(sn.fHiMHz, d) + " MHz (" + pctSigned((sn.fLoMHz / sn.fMHz - 1) * 100) + S(" … ") + pctSigned((sn.fHiMHz / sn.fMHz - 1) * 100) + ")";
}

QString snapNote(const QString &series, double exact, double snapped, double fMHz)
{
    if (!(exact > 0) || !(snapped > 0)) return QString();
    SnapInfo sn = snapInfo(series, exact, snapped, fMHz > 0 ? fMHz : 1);
    QString h = sn.hint; if (!h.isEmpty()) h[0] = h[0].toLower();
    return sn.label + " standard value (snap error " + pctSigned(sn.errPct) + "); " + h + (fMHz > 0 ? S(" → resonance ") + spreadTxt(sn) + " with L fixed" : QString()) + ".";
}

double snapE(double v, const QString &series)
{
    const std::vector<double> *s = eSeries(series);
    if (!s || !(v > 0)) return v;
    double dec = std::pow(10, std::floor(std::log10(v))), best = NaN;
    for (double d : {dec / 10, dec, dec * 10}) for (double m : *s) { double c = m * d; if (std::isnan(best) || std::fabs(std::log(c / v)) < std::fabs(std::log(best / v))) best = c; }
    return toPrecisionNum(best, 3);
}
double capRating(double vPk, double margin) { double need = vPk * (margin ? margin : 2); for (double r : CAP_RATINGS) if (r >= need) return r; return CAP_RATINGS.back(); }
TrapVoltage trapVoltage(double powerW, double zEnd) { double v = std::sqrt(powerW * zEnd); return {v, v * SQRT2}; }
std::vector<TrapBand> trapReactanceAtBands(double L_uH, double C_pF, double f0MHz, bool skipSame)
{
    std::vector<TrapBand> out;
    for (const Band &b : bands()) {
        double f = (b.lo + b.hi) / 2, w = 2 * PI * f * 1e6, x = w * L_uH * 1e-6 / (1 - w * w * L_uH * 1e-6 * C_pF * 1e-12);
        TrapBand r{b.id, f, x, x > 0 ? "inductive" : "capacitive", x > 0 ? x / w * 1e6 : NaN, x < 0 ? -1e12 / (w * x) : NaN, std::fabs(f / f0MHz - 1) < 0.06};
        if (!(skipSame && r.nearRes)) out.push_back(r);
    }
    return out;
}
double stubCapPF(const Coax &coax, double lenIn, double fMHz)
{
    double l = lenIn * 0.0254, beta = 2 * PI * fMHz / (C_LIGHT * coax.vf), w = 2 * PI * fMHz * 1e6;
    return std::tan(beta * l) / (w * coax.z0) * 1e12;
}
double bandFrequency(const QString &bandId, const QString &place)
{
    for (const Band &b : bands()) if (b.id == bandId) return toFixedNum(place == "below" ? b.lo * 0.99 : (b.lo + b.hi) / 2, 3);
    return NaN;
}

/* ---------------------------------------------------------------------------------------- coax trap */
CoaxTrapInput normalizeCoaxTrap(const CoaxTrapInput &r)
{
    CoaxTrapInput o = r, D;
    if (o.coaxId.isEmpty()) o.coaxId = D.coaxId; if (o.formId.isEmpty()) o.formId = D.formId; if (o.duty.isEmpty()) o.duty = D.duty;
    o.fMHz = clampv(o.fMHz, 0.5, 60, 7.05); o.formOdIn = clampv(o.formOdIn, 0.25, 12, 2.375); o.spacingIn = clampv(o.spacingIn, 0, 2, 0);
    o.leadIn = clampv(o.leadIn, 0, 24, 1); o.trimIn = clampv(o.trimIn, 0, 12, 1); o.powerW = clampv(o.powerW, 0.1, 5000, 100); o.zEnd = clampv(o.zEnd, 100, 20000, 3000);
    bool known; double dp = dutyPercentFor(o.duty, &known); o.dutyPercent = known ? dp : clampv(o.dutyPercent, 1, 100, 30);
    if (o.model != "stub") o.model = "lumped";
    o.customPfFt = clampv(o.customPfFt, 5, 60, 30.8); o.customVf = clampv(o.customVf, 0.5, 1, 0.66); o.customOdIn = clampv(o.customOdIn, 0.05, 1, 0.195); o.customZ0 = clampv(o.customZ0, 25, 150, 50);
    return o;
}

CoaxTrapResult coaxTrap(const CoaxTrapInput &raw)
{
    CoaxTrapInput i = normalizeCoaxTrap(raw);
    Coax cx = i.coaxId == "custom"
        ? Coax{"custom", "Custom coax (" + jsNum(i.customPfFt) + " pF/ft, VF " + jsNum(i.customVf) + ", " + inchTxt(i.customOdIn, 3) + " OD)", i.customZ0, i.customPfFt, i.customVf,
               i.customOdIn, 0.9, i.customOdIn * 25.4 * 0.6, 1000, "custom", false, false, "user"}
        : *coaxById(i.coaxId);
    double t = cx.odIn, p = t + i.spacingIn, D = i.formOdIn + t, r = D / 2;
    struct At { double L, C, clen, len, f; };
    auto at = [&](double n, double fForStub) {
        double len = n * p, L = wheelerUH(r, len, n), clen = PI * n * D + i.leadIn;
        double C = i.model == "stub" ? stubCapPF(cx, clen, fForStub) : cx.pfFt * clen / 12;
        return At{L, C, clen, len, 1 / (2 * PI * std::sqrt(L * 1e-6 * C * 1e-12)) / 1e6};
    };
    double lo = 0.2, hi = 400;
    for (int k = 0; k < 80; k++) { double mid = std::sqrt(lo * hi); if (at(mid, i.fMHz).f > i.fMHz) lo = mid; else hi = mid; }
    double n = std::sqrt(lo * hi); At g = at(n, i.fMHz);
    double fs = i.fMHz; for (int k = 0; k < 60; k++) { double Cst = stubCapPF(cx, g.clen, fs); fs = 1 / (2 * PI * std::sqrt(g.L * 1e-6 * Cst * 1e-12)) / 1e6; }
    double lumpedF = 1 / (2 * PI * std::sqrt(g.L * 1e-6 * cx.pfFt * g.clen / 12 * 1e-12)) / 1e6;
    double Dmm = D * 25.4, pmm = p * 25.4;
    double lNag = nagaokaRosaUH(Dmm, pmm, cx.shieldMM, std::max(n, 1.01));
    double X = 2 * PI * i.fMHz * g.L;
    CoilQ qs = coilQ(g.L, Dmm, pmm, cx.shieldMM, i.fMHz, 1e9, std::max(n, 2.0), 0);
    double rCenter = wireRac(g.clen * 0.0254, cx.centerMM, i.fMHz) * (cx.ccs ? 1.3 : 1);
    double R = qs.rac * 1.5 + rCenter, Q = X / R, Rp = Q * X;
    TrapVoltage V = trapVoltage(i.powerW, i.zEnd); double pdKey = V.rms * V.rms / Rp, pdAvg = pdKey * i.dutyPercent / 100;
    double fQuarter = C_LIGHT * cx.vf / (4 * g.clen * 0.0254);
    double ld = g.len / D;
    CoaxTrapResult res; std::vector<Check> &checks = res.checks;
    auto add = [&](const QString &id, const QString &s, const QString &txt) { checks.push_back({id, s, txt}); };
    auto f2 = [](double v, int d = 1) { return toFixed(v, d); };
    add("res", "ok", "Resonance check: L " + f2(g.L, 3) + S(" µH with C ") + f2(g.C, 1) + S(" pF → ") + f2(g.f, 3) + " MHz (" + (i.model == "stub" ? "stub-corrected C" : "VE6YP/K7MEM lumped pF/ft model") + "). XL = XC = " + f2(X, 1) + S(" Ω."));
    if (i.model == "lumped") add("stub", std::fabs(fs / i.fMHz - 1) > 0.02 ? "warn" : "info", "Stub effect: the coax is a transmission-line stub, not a pure capacitor (W8JI measured 164 pF vs 138.5 pF predicted). Expect this winding to resonate near " + f2(fs, 3) + " MHz (" + f2((fs / i.fMHz - 1) * 100, 1) + S("%). Wind the predicted turns, then spread/compress turns or trim on the analyser — or select the stub-corrected model."));
    else add("stub", "info", "Stub-corrected: the plain pF/ft model would give " + f2(lumpedF, 3) + " MHz for this winding.");
    add("ld", ld < 0.25 || ld > 3 ? "warn" : (ld < 0.4 || ld > 2 ? "info" : "ok"), "Length/diameter " + f2(ld, 2) + S(":1 (K7MEM: 0.4–2, best ≈0.45; W4ULD: 0.25–3, best Q near 0.38)."));
    add("bend", i.formOdIn < 10 * t ? "warn" : "ok", "Bend: form OD " + inchTxt(i.formOdIn, 2) + " vs coax OD " + inchTxt(t, 3) + S(" — keep the form ≥ 10 × coax OD (≥ ") + inchTxt(10 * t, 2) + ") so the centre conductor does not migrate through the dielectric.");
    if (cx.foam) add("foam", "warn", cx.label + " has a foam/air dielectric. W4ULD: use solid-dielectric coax for traps — foam deforms on tight bends and absorbs moisture, so the trap drifts.");
    if (cx.ccs) add("ccs", "info", "Centre conductor is copper-clad steel/aluminium: fine at HF (skin depth is in the copper) but a little lossier; solder carefully.");
    add("q", "info", S("Estimated unloaded Q ≈ ") + rnd(Q) + S(" (Rp ≈ ") + toFixed(Rp / 1000, 1) + S(" kΩ). For scale: W8JI measured Q≈133 (Rp 17.8 kΩ) for an RG-58 7 MHz trap, versus ~465 for a #12-wire L/C trap; W4ULD estimated 215–348 for RG-58 traps."));
    add("volt", V.rms > cx.vMaxRms * 0.5 ? (V.rms > cx.vMaxRms ? "fail" : "warn") : "ok", S("Trap voltage ≈ ") + rnd(V.rms) + " V RMS (" + rnd(V.pk) + " V peak) at " + jsNum(i.powerW) + S(" W with Z_end ≈ ") + jsNum(i.zEnd) + S(" Ω. Coax dielectric sees up to this between centre and shield; typical rating ") + jsNum(cx.vMaxRms) + S(" V RMS (keep ≤ 50%)."));
    add("heat", pdAvg > 35 ? "fail" : (pdAvg > 10 ? "warn" : "ok"), S("Dissipation if you transmit right on the trap frequency: ≈ ") + f2(pdKey, 1) + " W key-down, " + f2(pdAvg, 1) + " W average at " + jsNum(i.dutyPercent) + S("% duty (ON5VL: RG-58/59 traps tolerate ≈ 35 W). W8JI: never put a trap's resonance inside the band you operate."));
    add("srf", fQuarter < 2.5 * i.fMHz ? "warn" : "info", "Coax stub self-resonance: the " + inchTxt(g.clen, 1) + S(" of coax is λ/4 near ") + f2(fQuarter, 1) + S(" MHz — above the trap frequency the trap is capacitive and behaves unpredictably near this frequency."));
    add("model", "info", "Inductance cross-check (Nagaoka + Rosa, Coil64 method, shield as round conductor): " + f2(lNag, 3) + S(" µH vs Wheeler ") + f2(g.L, 3) + S(" µH (") + f2((lNag / g.L - 1) * 100, 1) + "%).");
    res.steps = QStringList{
        "Cut " + inchTxt(g.clen + 2 * i.trimIn, 1) + " of " + cx.label + " (" + inchTxt(g.clen, 1) + " on the coil incl. " + inchTxt(i.leadIn, 1) + " of pigtails, plus " + inchTxt(i.trimIn, 1) + " each end for terminating).",
        "Drill two holes at each end of the " + inchTxt(i.formOdIn, 3) + " form, about " + inchTxt(g.len + 0.5, 2) + " apart (winding length " + inchTxt(g.len, 2) + ").",
        "Wind " + f2(n, 2) + " turns" + (i.spacingIn > 0 ? " with " + inchTxt(i.spacingIn, 3) + " gaps (pitch " + inchTxt(p, 3) + ")" : QString(" close-wound")) + "; pass each end into the form through its holes.",
        S("START end: SHIELD → antenna wire A (towards the feed point)."),
        S("START end: CENTRE conductor → jumper wire run INSIDE the form to the FINISH end, soldered to the SHIELD there (shield-to-centre cross connection)."),
        S("FINISH end: CENTRE conductor → antenna wire B (outer section)."),
        S("Before installing: check resonance with a dip meter or an analyser through a 1-turn link (trap not connected); spread or compress turns to tune — aim slightly below the band (W8JI)."),
        "Seal both coax ends (silicone or self-amalgamating tape) and keep centre/shield ends apart; strain-relieve the antenna wires on the form, not on the coax."};
    Units u = getUnits();
    res.parts = {
        {1, cx.label + ": " + inchTxt(g.clen + 2 * i.trimIn, 1) + (u.len == "mixed" ? " (" + rnd((g.clen + 2 * i.trimIn) * 25.4) + " mm)" : QString())},
        {1, "Form: " + inchTxt(i.formOdIn, 3) + " OD, about " + inchTxt(g.len + 2, 1) + " long (winding + " + inchTxt(1, 0) + " each end for wire anchors)"},
        {1, "Jumper wire inside the form, ~" + inchTxt(g.len + 3, 1) + " (insulated, " + (u.wire == "awg" ? S("AWG 18–14") : u.wire == "in" ? S("Ø0.040–0.064 in") : S("Ø1.0–1.6 mm")) + ")"},
        {2, "Stainless eye-bolts or holes for antenna wire strain relief"},
        {1, "Sealant / self-amalgamating tape, UV-resistant cable ties"}};
    res.input = i; res.coax = cx; res.n = n; res.pitchIn = p; res.meanDiaIn = D; res.windingLenIn = g.len; res.coaxLenIn = g.clen; res.cutLenIn = g.clen + 2 * i.trimIn;
    res.L_uH = g.L; res.C_pF = g.C; res.X = X; res.fCheckMHz = g.f; res.fStubMHz = fs; res.fLumpedMHz = lumpedF; res.lNagaokaUH = lNag; res.ldRatio = ld; res.q = Q; res.rpOhm = Rp;
    res.vTrap = V; res.pdKeyW = pdKey; res.pdAvgW = pdAvg; res.fQuarterMHz = fQuarter; res.bands = trapReactanceAtBands(g.L, g.C, i.fMHz, false);
    return res;
}

/* ---------------------------------------------------------------------------------------- LC trap */
LcTrapInput normalizeLcTrap(const LcTrapInput &r)
{
    LcTrapInput o = r, D;
    if (o.mode.isEmpty()) o.mode = D.mode; if (o.snap.isEmpty()) o.snap = D.snap; if (o.coreType.isEmpty()) o.coreType = D.coreType;
    if (o.wireId.isEmpty()) o.wireId = D.wireId; if (o.coreId.isEmpty()) o.coreId = D.coreId; if (o.duty.isEmpty()) o.duty = D.duty; if (o.capType.isEmpty()) o.capType = D.capType;
    o.fMHz = clampv(o.fMHz, 0.5, 60, 7.05); o.xOhm = clampv(o.xOhm, 10, 2000, 200); o.lUH = clampv(o.lUH, 0.05, 500, 5); o.cPF = clampv(o.cPF, 1, 20000, 100);
    if (o.mode != "X" && o.mode != "L" && o.mode != "C") o.mode = "X";
    if (!eSeries(o.snap)) o.snap = "none";
    o.formDiaMM = clampv(o.formDiaMM, 3, 300, 48.26); o.pitchMM = clampv(o.pitchMM, 0, 50, 0); o.stack = jsRound(clampv(o.stack, 1, 4, 1));
    o.powerW = clampv(o.powerW, 0.1, 5000, 100); o.zEnd = clampv(o.zEnd, 100, 20000, 3000); o.leadMM = clampv(o.leadMM, 0, 1000, 50); o.ambientC = clampv(o.ambientC, -20, 60, 30);
    bool known; double dp = dutyPercentFor(o.duty, &known); o.dutyPercent = known ? dp : clampv(o.dutyPercent, 1, 100, 30);
    if (o.coreType != "toroid") o.coreType = "air";
    if (!coreById(o.coreId)) o.coreId = "A:T-200-2";
    if (!capType(o.capType)) o.capType = "np0";
    return o;
}

LcTrapResult lcTrap(const LcTrapInput &raw)
{
    LcTrapInput i = normalizeLcTrap(raw); double f = i.fMHz, w = 2 * PI * f * 1e6, L, C;
    auto f2 = [](double v, int d = 1) { return toFixed(v, d); };
    if (i.mode == "L") { L = i.lUH; C = 1e12 / (w * w * L * 1e-6); }
    else if (i.mode == "C") { C = i.cPF; L = 1e6 / (w * w * C * 1e-12); }
    else { L = i.xOhm / w * 1e6; C = 1e12 / (w * i.xOhm); }
    double cIdeal = C, cNeed = C, fNom = f; bool snapped = false;
    if (i.snap != "none" && i.mode != "C") { C = snapE(C, i.snap); L = 1e6 / (w * w * C * 1e-12); snapped = true; }
    double X = w * L * 1e-6;
    LcTrapResult res; std::vector<Check> &checks = res.checks;
    auto add = [&](const QString &id, const QString &s, const QString &t) { checks.push_back({id, s, t}); };
    double Qcoil = 0, csPF = 0; const Wire *wire;
    if (i.coreType == "air") {
        wire = coilWireById(i.wireId);
        double p = std::max(i.pitchMM ? i.pitchMM : 2 * wire->od, wire->od * 1.0001), Dk = i.formDiaMM + wire->od;
        double lo = 0.5, hi = 400;
        for (int k = 0; k < 50; k++) { double mid = (lo + hi) / 2; Helical rs = helicalUH(mid, p, Dk, wire->dia, 3); if (rs.uH < L) lo = mid; else hi = mid; if (hi - lo < 1e-4) break; }
        double N = (lo + hi) / 2; Helical h = helicalUH(N, p, Dk, wire->dia, 4); double lk = N * p;
        csPF = coilCs_pF(p, Dk, lk);
        double srf = coilSRF_MHz(lk, Dk, h.lwM); CoilQ qq = coilQ(L, Dk, p, wire->dia, f, srf, N, csPF);
        Qcoil = qq.q;
        res.hasCoil = true;
        res.coil = {N, p, Dk, lk, h.lwM * 1000 + 2 * i.leadMM, h.uH, csPF, srf, qq.q, qq.esr, wheelerUH(Dk / 2 / 25.4, lk / 25.4, N), nagaokaRosaUH(Dk, p, wire->dia, N), wire};
        add("coil", "ok", "Air coil: " + f2(N, 2) + " turns of " + wireLabel(*wire) + " on a " + mmTxt(i.formDiaMM, 1) + " form, pitch " + mmTxt(p, 2) + ", length " + mmTxt(lk, 1) + S(" → ") + f2(h.uH, 3) + S(" µH (Coil64 helical method; Nagaoka+Rosa ") + f2(res.coil.nagaokaRosaUH, 3) + ", Wheeler " + f2(res.coil.wheelerUH, 3) + ").");
        add("srf", srf < 2 * f ? "warn" : "ok", S("Coil self-resonance ≈ ") + f2(srf, 1) + S(" MHz (Knight); self-capacitance ≈ ") + f2(csPF, 2) + " pF" + (srf < 2 * f ? S(" — too close to the trap frequency; use fewer turns (lower X) or a larger pitch.") : QString(".")));
        double ld = lk / Dk; add("ld", ld < 0.3 || ld > 4 ? "info" : "ok", "Length/diameter " + f2(ld, 2) + S(" (best Q for single-layer coils is around 0.5–1)."));
    } else {
        const Core &core = *coreById(i.coreId); const Material &mat = *material(core.mat);
        int Nt = (int)std::max(1.0, jsRound(std::sqrt(L * 1000 / (core.al * i.stack))));
        double Lt = core.al * i.stack * Nt * Nt / 1000;
        L = Lt; C = 1e12 / (w * w * L * 1e-6); cNeed = C; snapped = i.snap != "none"; if (snapped) { C = snapE(C, i.snap); fNom = f * std::sqrt(cNeed / C); } X = w * L * 1e-6;
        Complex2 mu = permeability(core.mat, f * 1e6); double qCore = mu.re / std::max(mu.im, 1e-9);
        wire = coilWireById(i.wireId);
        double tl = turnLengthMM(core, i.stack, wire->od), wl = Nt * tl + 2 * i.leadMM, qWire = X / wireRac(wl / 1000, wire->dia, f);
        Qcoil = 1 / (1 / qCore + 1 / qWire);
        TrapVoltage V0 = trapVoltage(i.powerW, i.zEnd); double bG = fluxDensity_mT(V0.rms, f * 1e6, Nt, core.ae * i.stack) * 10, lim = heatingFluxLimitGauss(f);
        double pCore = V0.rms * V0.rms / (qCore * X) * i.dutyPercent / 100, dT = tempRiseC(pCore, coreSurfaceCm2(core, i.stack));
        res.hasToroid = true;
        res.toroid = {&core, Nt, Lt, qCore, qWire, wl, bG, lim, pCore, dT, i.ambientC + dT, wire};
        add("toroid", "ok", QString::number(Nt) + " turns of " + wireLabel(*wire) + " on " + core.id + (i.stack > 1 ? S(" ×") + jsNum(i.stack) : QString()) + " (AL " + jsNum(core.al) + S(" nH/T²") + (core.hasAlAmidon ? " = " + jsNum(core.alAmidonValue) + " " + core.alAmidonUnit : QString()) + S(") → ") + f2(Lt, 3) + S(" µH; C re-solved to ") + f2(cNeed, 1) + " pF to keep " + jsNum(f) + " MHz" + (snapped ? "; the " + eSeriesLabel(i.snap) + " part is " + f2(C, 1) + S(" pF → ") + f2(fNom, 3) + " MHz" : QString()) + ".");
        if (mat.kind != "iron") add("ferrite", "fail", S("Ferrite is not recommended for traps: µ changes with temperature and flux, loss is high (Q≈") + rnd(qCore) + " here), so the trap detunes and heats exactly when it sees full voltage. Use iron powder (-2/-6/-10) or an air coil.");
        else {
            if (!mat.freq.isEmpty() && mat.freq.contains("MHz")) {
                QRegularExpressionMatch mm = QRegularExpression("([\\d.]+)\\s*MHz\\s*-\\s*([\\d.]+)\\s*MHz").match(mat.freq);
                QString m2 = core.mat; int dash = m2.indexOf('-'); if (dash >= 0) m2.remove(dash, 1);
                if (mm.hasMatch() && (f < mm.captured(1).toDouble() || f > mm.captured(2).toDouble())) add("range", "warn", "Material -" + m2 + " is specified for " + mat.freq + "; " + jsNum(f) + " MHz is outside that range (Q drops).");
            }
            add("flux", bG > lim ? "warn" : "ok", "Flux at trap voltage " + f2(bG, 1) + " G vs Amidon heating guideline " + f2(lim, 0) + " G at " + jsNum(f) + " MHz.");
            add("heat", res.toroid.coreTempC > 75 ? "warn" : "ok", S("Core loss ≈ ") + f2(pCore, 2) + S(" W average if you transmit on the trap frequency → ≈ ") + rnd(res.toroid.coreTempC) + S(" °C (iron-powder binder: keep ≤ 75 °C long-term, Micrometals thermal-aging note)."));
            add("qcore", "info", S("Q: material ≈ ") + rnd(qCore) + S(" (nominal, rough), wire ≈ ") + rnd(qWire) + ".");
        }
    }
    SnapInfo sn; if (snapped) sn = snapInfo(i.snap, cNeed, C, fNom);
    const CapType &cap = *capType(i.capType); double Q = 1 / (1 / Qcoil + 1 / cap.q), Rp = Q * X; TrapVoltage V = trapVoltage(i.powerW, i.zEnd);
    double rating = capRating(V.pk, 2), Ic = V.rms / X, pd = V.rms * V.rms / Rp, pdAvg = pd * i.dutyPercent / 100;
    double cNet = C - csPF, cFit = i.snap != "none" || !(cNet > 0) ? C : cNet;
    QString capTxt = "Capacitor " + f2(C, 1) + " pF" + (snapped ? " (" + sn.label + " value; exact " + f2(sn.exactPF, 1) + " pF, " + pctSigned(sn.errPct) + ")" : QString());
    if (csPF > 0) capTxt += i.snap != "none" ? S(" — the coil adds ≈ ") + f2(csPF, 1) + " pF self-capacitance; fit the " + f2(C, 1) + " pF part and spread the turns slightly to retune"
                                             : S(" — the coil adds ≈ ") + f2(csPF, 1) + S(" pF self-capacitance, so fit ≈ ") + f2(cNet, 1) + " pF";
    capTxt += S(", rated ≥ ") + jsNum(rating) + S(" V (2 × ") + rnd(V.pk) + S(" V peak). RF current ≈ ") + f2(Ic, 2) + " A RMS (" + f2(V.rms * Ic / 1000, 2) + " kVAR). " + cap.label + ": " + cap.note;
    add("cap", "ok", capTxt);
    if (snapped) {
        double dev = (fNom / f - 1) * 100;
        add("tol", std::fabs(dev) > 2 ? "warn" : "info", "Standard-value capacitor: " + sn.label + " " + f2(C, 1) + " pF for an exact " + f2(sn.exactPF, 1) + " pF (snap error " + pctSigned(sn.errPct) + ")" +
            (std::fabs(dev) >= 0.05 ? "; with " + QString::number(res.toroid.N) + " whole turns the trap resonates at " + f2(fNom, 3) + " MHz (" + pctSigned(dev) + S(") — spread or compress the turns to bring it back") : "; L is re-solved, so the trap still resonates at " + jsNum(f) + " MHz") +
            ". " + sn.hint + S(" → resonance ") + spreadTxt(sn) + S(" with L fixed. Tune it on a dip meter / analyser, or use a ±5 % / ±1 % C0G or mica part to narrow the spread."));
    } else if (i.snap != "none") add("tol", "info", "Snap to " + eSeriesLabel(i.snap) + " not applied: you chose the capacitor value, so L is solved for it. " + eSeriesHint(i.snap) + S(" → resonance ") + spreadTxt(snapInfo(i.snap, C, C, f)) + " with L fixed.");
    add("x", X < 100 ? "info" : (X > 400 ? "warn" : "ok"), "Trap reactance X = " + f2(X, 0) + S(" Ω. Guidance (ARRL Antenna Book, W8JI): ~100–300 Ω. Higher X → higher Rp (better isolation) but more loading/shortening and loss on the lower band and a narrower bandwidth; lower X → broader, less loading, less isolation."));
    add("q", "info", S("Unloaded trap Q ≈ ") + rnd(Q) + " (coil " + rnd(Qcoil) + S(", capacitor ≈ ") + jsNum(cap.q) + S(") → Rp ≈ ") + toFixed(Rp / 1000, 1) + S(" kΩ. W8JI measured 100 kΩ for 100 pF + #12-wire coil at 7 MHz, 250–300 kΩ with copper tubing and vacuum/15 kV caps."));
    add("heat2", pdAvg > 20 ? "warn" : "ok", "Dissipation on the trap frequency: " + f2(pd, 1) + " W key-down, " + f2(pdAvg, 1) + " W average (" + jsNum(i.dutyPercent) + S("% duty) at ≈ ") + rnd(V.rms) + S(" V RMS across the trap (√(P·Z_end), Z_end ") + jsNum(i.zEnd) + S(" Ω)."));
    double fTrue = csPF > 0 ? 1 / (2 * PI * std::sqrt(L * 1e-6 * (C + csPF) * 1e-12)) / 1e6 : f;
    if (csPF > 0) add("fcs", "info", "With the coil's self-capacitance in parallel, L with the full " + f2(C, 1) + " pF resonates at " + f2(fTrue, 3) + " MHz.");
    if (i.coreType == "air") res.steps = QStringList{
        "Wind " + f2(res.coil.N, 2) + " turns of " + wireLabel(*res.coil.wire) + " on a " + mmTxt(i.formDiaMM, 1) + " form (or self-supporting), pitch " + mmTxt(res.coil.pitchMM, 2) + ", winding length " + mmTxt(res.coil.lenMM, 1) + ". Wire: " + mmTxt(res.coil.wireLenMM) + " incl. leads.",
        "Mount the " + f2(cFit, 1) + S(" pF / ≥ ") + jsNum(rating) + " V capacitor directly across the coil ends (shortest leads).",
        S("Antenna wire A → one coil end; antenna wire B → the other. Take the wire tension on the form, not on the capacitor."),
        "Tune before installing: grid-dip or analyser with a 1-turn link; squeeze/spread turns to set the resonance slightly below the band (W8JI).",
        "Weatherproof: enclose or coat; moisture lowers Q and shifts resonance."};
    else res.steps = QStringList{
        "Wind " + QString::number(res.toroid.N) + " turns of " + wireLabel(*res.toroid.wire) + " evenly around " + res.toroid.core->id + (i.stack > 1 ? S(" ×") + jsNum(i.stack) + " (stacked, taped)" : QString()) + "; wire " + mmTxt(res.toroid.wireLenMM) + " incl. leads.",
        "Solder the " + f2(C, 1) + S(" pF / ≥ ") + jsNum(rating) + " V capacitor across the winding ends.",
        "Antenna wires to the two winding ends via a strain-relieved insulator (the core must not carry tension).",
        S("Tune with a dip meter/analyser by spreading or compressing turns (±3–5%)."),
        S("Weatherproof; iron-powder binders age above ~75–100 °C — avoid transmitting on the trap frequency.")};
    res.parts = {{1, "Capacitor " + f2(i.coreType == "air" ? cFit : C, 1) + " pF" + (snapped ? " " + sn.tolText + " (" + sn.series + ")" : QString()) + S(", ≥ ") + jsNum(rating) + " V, " + cap.label + S(" (RF current ≥ ") + f2(Ic * 1.5, 1) + " A)"}};
    if (res.hasCoil) { res.parts.push_back({1, wireLabel(*res.coil.wire) + ": " + mmTxt(res.coil.wireLenMM)}); res.parts.push_back({1, "Coil form " + mmTxt(i.formDiaMM, 1) + S(" OD (PVC/PTFE/acrylic), ≈ ") + mmTxt(res.coil.lenMM + 40) + " long"}); }
    else { res.parts.push_back({i.stack, res.toroid.core->id + " (" + res.toroid.core->pn + ", " + dimsTxt(*res.toroid.core) + (!res.toroid.core->color.isEmpty() ? ", colour " + res.toroid.core->color : QString()) + ")"}); res.parts.push_back({1, wireLabel(*res.toroid.wire) + ": " + mmTxt(res.toroid.wireLenMM)}); }
    res.parts.push_back({1, "Insulator / enclosure, strain relief, sealant"});
    res.input = i; res.L_uH = L; res.C_pF = C; res.cIdealPF = cIdeal; res.cNeedPF = cNeed; res.fNomMHz = fNom; res.hasSnap = snapped; res.snap = sn; res.X = X; res.q = Q; res.qCoil = Qcoil; res.rpOhm = Rp; res.vTrap = V; res.capRatingV = rating; res.capCurrentA = Ic;
    res.pdKeyW = pd; res.pdAvgW = pdAvg; res.fWithCsMHz = fTrue; res.bands = trapReactanceAtBands(L, C, f, false);
    return res;
}

} // namespace ke9esu
