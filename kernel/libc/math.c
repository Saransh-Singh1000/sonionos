#include "math.h"
#include "errno.h"

typedef union { double d; uint64_t u; } f64_bits;
typedef union { float  f; uint32_t u; } f32_bits;

double fabs(double x) {
    f64_bits b = { x };
    b.u &= 0x7FFFFFFFFFFFFFFFULL;
    return b.d;
}

float fabsf(float x) {
    f32_bits b = { x };
    b.u &= 0x7FFFFFFFU;
    return b.f;
}

double copysign(double x, double y) {
    f64_bits bx = { x }, by = { y };
    bx.u = (bx.u & 0x7FFFFFFFFFFFFFFFULL) | (by.u & 0x8000000000000000ULL);
    return bx.d;
}

float copysignf(float x, float y) {
    f32_bits bx = { x }, by = { y };
    bx.u = (bx.u & 0x7FFFFFFFU) | (by.u & 0x80000000U);
    return bx.f;
}

double fmod(double x, double y) {
    if (y == 0.0 || isnan(x) || isnan(y) || isinf(x)) { errno = EDOM; return NAN; }
    if (isinf(y)) return x;
    return x - trunc(x / y) * y;
}

float fmodf(float x, float y) { return (float)fmod((double)x, (double)y); }

double remainder(double x, double y) {
    if (y == 0.0 || isnan(x) || isnan(y) || isinf(x)) { errno = EDOM; return NAN; }
    if (isinf(y)) return x;
    return x - round(x / y) * y;
}

float remainderf(float x, float y) { return (float)remainder((double)x, (double)y); }

double fmax(double x, double y) { if (isnan(x)) return y; if (isnan(y)) return x; return x > y ? x : y; }
float  fmaxf(float x, float y)   { if (isnan(x)) return y; if (isnan(y)) return x; return x > y ? x : y; }
double fmin(double x, double y) { if (isnan(x)) return y; if (isnan(y)) return x; return x < y ? x : y; }
float  fminf(float x, float y)   { if (isnan(x)) return y; if (isnan(y)) return x; return x < y ? x : y; }
double fdim(double x, double y) { return (isnan(x) || isnan(y)) ? NAN : (x > y ? x - y : 0.0); }
float  fdimf(float x, float y)   { return (isnan(x) || isnan(y)) ? NAN : (x > y ? x - y : 0.0f); }
double fma(double x, double y, double z) { return x * y + z; }
float  fmaf(float x, float y, float z)   { return x * y + z; }
double trunc(double x) {
    if (isnan(x) || isinf(x) || fabs(x) >= 4503599627370496.0) return x;
    return (double)(long long)x;
}

float truncf(float x) {
    if (isnan(x) || isinf(x) || fabsf(x) >= 8388608.0f) return x;
    return (float)(long long)x;
}

double floor(double x) {
    double t = trunc(x);
    return (x < t) ? (t - 1.0) : t;
}

float floorf(float x) {
    float t = truncf(x);
    return (x < t) ? (t - 1.0f) : t;
}

double ceil(double x) {
    double t = trunc(x);
    return (x > t) ? (t + 1.0) : t;
}

float ceilf(float x) {
    float t = truncf(x);
    return (x > t) ? (t + 1.0f) : t;
}

double round(double x) {
    if (isnan(x) || isinf(x)) return x;
    return (x >= 0.0) ? floor(x + 0.5) : ceil(x - 0.5);
}

float roundf(float x) {
    if (isnan(x) || isinf(x)) return x;
    return (x >= 0.0f) ? floorf(x + 0.5f) : ceilf(x - 0.5f);
}

long lround(double x) { return (long)round(x); }
long lroundf(float x) { return (long)roundf(x); }
long long llround(double x) { return (long long)round(x); }
long long llroundf(float x) { return (long long)roundf(x); }
double rint(double x) { return round(x); }
float  rintf(float x) { return roundf(x); }
long lrint(double x) { return (long)rint(x); }
long lrintf(float x) { return (long)rintf(x); }
long long llrint(double x) { return (long long)rint(x); }
long long llrintf(float x) { return (long long)rintf(x); }
double nearbyint(double x) { return rint(x); }
float  nearbyintf(float x) { return rintf(x); }

double frexp(double x, int *exp) {
    f64_bits b = { x };
    int e = (int)((b.u >> 52) & 0x7FF);
    if (e == 0) {
        if (x == 0.0) { *exp = 0; return x; }
        double norm = frexp(x * 18014398509481984.0, exp);
        *exp -= 54;
        return norm;
    }
    if (e == 0x7FF) { *exp = 0; return x; }
    *exp = e - 1022;
    b.u = (b.u & 0x800FFFFFFFFFFFFFULL) | 0x3FE0000000000000ULL;
    return b.d;
}

float frexpf(float x, int *exp) { return (float)frexp((double)x, exp); }

double ldexp(double x, int exp) {
    if (x == 0.0 || isnan(x) || isinf(x) || exp == 0) return x;
    while (exp > 1023)  { x *= 8.98846567431158e307; exp -= 1023; }
    while (exp < -1022) { x *= 2.2250738585072014e-308; exp += 1022; }
    f64_bits scale;
    scale.u = (uint64_t)(exp + 1023) << 52;
    return x * scale.d;
}

float ldexpf(float x, int exp) { return (float)ldexp((double)x, exp); }
double scalbn(double x, int n) { return ldexp(x, n); }
float  scalbnf(float x, int n) { return ldexpf(x, n); }

double modf(double x, double *iptr) {
    double i = trunc(x);
    *iptr = i;
    return isinf(x) ? copysign(0.0, x) : (x - i);
}

float modff(float x, float *iptr) {
    double di;
    double rem = modf((double)x, &di);
    *iptr = (float)di;
    return (float)rem;
}

int ilogb(double x) {
    int e;
    if (x == 0.0) return -2147483647 - 1;
    if (isnan(x) || isinf(x)) return 2147483647;
    frexp(x, &e);
    return e - 1;
}

int ilogbf(float x) { return ilogb((double)x); }
double logb(double x) { return (x == 0.0) ? -INFINITY : (double)ilogb(x); }
float  logbf(float x) { return (float)logb((double)x); }
double sqrt(double x) {
    if (x < 0.0) { errno = EDOM; return NAN; }
    if (x == 0.0 || isnan(x) || isinf(x)) return x;
    double res;
    __asm__ volatile ("sqrtsd %1, %0" : "=x"(res) : "x"(x));
    return res;
}

float sqrtf(float x) {
    if (x < 0.0f) { errno = EDOM; return NAN; }
    if (x == 0.0f || isnan(x) || isinf(x)) return x;
    float res;
    __asm__ volatile ("sqrtss %1, %0" : "=x"(res) : "x"(x));
    return res;
}

double cbrt(double x) {
    if (x == 0.0 || isnan(x) || isinf(x)) return x;
    int neg = x < 0.0;
    double ax = fabs(x);
    f64_bits b = { ax };
    b.u = b.u / 3 + 0x2A9F7893E41F8000ULL;
    double y = b.d;
    for (int i = 0; i < 6; i++) y = (2.0 * y + ax / (y * y)) / 3.0;
    return neg ? -y : y;
}

float cbrtf(float x) { return (float)cbrt((double)x); }

double hypot(double x, double y) {
    x = fabs(x); y = fabs(y);
    if (isinf(x) || isinf(y)) return INFINITY;
    double mx = fmax(x, y), mn = fmin(x, y);
    if (mx == 0.0) return 0.0;
    double r = mn / mx;
    return mx * sqrt(1.0 + r * r);
}

float hypotf(float x, float y) { return (float)hypot((double)x, (double)y); }

double exp(double x) {
    if (isnan(x)) return x;
    if (x > 709.782712893) { errno = ERANGE; return HUGE_VAL; }
    if (x < -745.133219102) return 0.0;

    int k = (int)round(x * M_LOG2E);
    double r = x - (double)k * M_LN2;

    double term = 1.0, sum = 1.0;
    for (int i = 1; i <= 14; i++) {
        term *= r / (double)i;
        sum += term;
    }
    return ldexp(sum, k);
}

float expf(float x) { return (float)exp((double)x); }
double exp2(double x) { return exp(x * M_LN2); }
float  exp2f(float x) { return (float)exp2((double)x); }
double expm1(double x) { return (fabs(x) < 1e-5) ? (x + 0.5 * x * x + (x * x * x) / 6.0) : (exp(x) - 1.0); }
float  expm1f(float x) { return (float)expm1((double)x); }

double log(double x) {
    if (isnan(x)) return x;
    if (x < 0.0) { errno = EDOM; return NAN; }
    if (x == 0.0) { errno = ERANGE; return -HUGE_VAL; }
    if (isinf(x)) return INFINITY;

    int e;
    double m = frexp(x, &e);
    if (m < M_SQRT1_2) { m *= 2.0; e--; }

    double u = (m - 1.0) / (m + 1.0);
    double u2 = u * u;
    double term = u, sum = u;
    for (int k = 3; k <= 21; k += 2) {
        term *= u2;
        sum += term / (double)k;
    }
    return 2.0 * sum + (double)e * M_LN2;
}

float logf(float x) { return (float)log((double)x); }
double log2(double x) { return log(x) * M_LOG2E; }
float  log2f(float x) { return (float)log2((double)x); }
double log10(double x) { return log(x) * M_LOG10E; }
float  log10f(float x) { return (float)log10((double)x); }
double log1p(double x) { return (fabs(x) < 1e-5) ? (x - 0.5 * x * x + (x * x * x) / 3.0) : log(1.0 + x); }
float  log1pf(float x) { return (float)log1p((double)x); }

double pow(double x, double y) {
    if (y == 0.0 || x == 1.0) return 1.0;
    if (isnan(x) || isnan(y)) return NAN;
    if (x == 0.0) {
        if (y < 0.0) { errno = ERANGE; return HUGE_VAL; }
        return 0.0;
    }
    double iy;
    int is_int = (modf(y, &iy) == 0.0);
    if (x < 0.0 && !is_int) { errno = EDOM; return NAN; }

    double ax = fabs(x);
    double res = exp(y * log(ax));
    if (x < 0.0 && ((long long)iy & 1LL)) res = -res;
    return res;
}

float powf(float x, float y) { return (float)pow((double)x, (double)y); }
static double reduce_trig(double x, int *quadrant) {
    double k = round(x * M_2_PI);
    *quadrant = ((int)(long long)k) & 3;
    return x - k * M_PI_2;
}

static double sin_kernel(double r) {
    double r2 = r * r;
    return r * (1.0 - r2 / 6.0 * (1.0 - r2 / 20.0 * (1.0 - r2 / 42.0 * (1.0 - r2 / 72.0 * (1.0 - r2 / 110.0 * (1.0 - r2 / 156.0 * (1.0 - r2 / 210.0)))))));
}

static double cos_kernel(double r) {
    double r2 = r * r;
    return 1.0 - r2 / 2.0 * (1.0 - r2 / 12.0 * (1.0 - r2 / 30.0 * (1.0 - r2 / 56.0 * (1.0 - r2 / 90.0 * (1.0 - r2 / 132.0 * (1.0 - r2 / 182.0))))));
}

double sin(double x) {
    if (isnan(x) || isinf(x)) { errno = EDOM; return NAN; }
    int q;
    double r = reduce_trig(x, &q);
    switch (q) {
        case 0: return  sin_kernel(r);
        case 1: return  cos_kernel(r);
        case 2: return -sin_kernel(r);
        default: return -cos_kernel(r);
    }
}

float sinf(float x) { return (float)sin((double)x); }

double cos(double x) {
    if (isnan(x) || isinf(x)) { errno = EDOM; return NAN; }
    int q;
    double r = reduce_trig(x, &q);
    switch (q) {
        case 0: return  cos_kernel(r);
        case 1: return -sin_kernel(r);
        case 2: return -cos_kernel(r);
        default: return  sin_kernel(r);
    }
}

float cosf(float x) { return (float)cos((double)x); }

double tan(double x) {
    double c = cos(x);
    if (c == 0.0) return copysign(HUGE_VAL, sin(x));
    return sin(x) / c;
}

float tanf(float x) { return (float)tan((double)x); }

double atan(double x) {
    if (isnan(x)) return x;
    int neg = x < 0.0;
    double ax = fabs(x);
    int inv = ax > 1.0;
    if (inv) ax = 1.0 / ax;

    int reduced = 0;
    const double TAN_PI_6  = 0.57735026918962576451;
    const double TAN_PI_12 = 0.26794919243112270647;
    if (ax > TAN_PI_12) {
        ax = (ax - TAN_PI_6) / (1.0 + ax * TAN_PI_6);
        reduced = 1;
    }

    double a2 = ax * ax;
    double term = ax, sum = ax;
    for (int k = 3; k <= 19; k += 2) {
        term = -term * a2;
        sum += term / (double)k;
    }

    if (reduced) sum += M_PI / 6.0;
    if (inv) sum = M_PI_2 - sum;
    return neg ? -sum : sum;
}

float atanf(float x) { return (float)atan((double)x); }

double atan2(double y, double x) {
    if (isnan(x) || isnan(y)) return NAN;
    if (x > 0.0) return atan(y / x);
    if (x < 0.0 && y >= 0.0) return atan(y / x) + M_PI;
    if (x < 0.0 && y < 0.0)  return atan(y / x) - M_PI;
    if (x == 0.0 && y > 0.0) return  M_PI_2;
    if (x == 0.0 && y < 0.0) return -M_PI_2;
    return 0.0;
}

float atan2f(float y, float x) { return (float)atan2((double)y, (double)x); }

double asin(double x) {
    if (x < -1.0 || x > 1.0) { errno = EDOM; return NAN; }
    if (x == 1.0) return M_PI_2;
    if (x == -1.0) return -M_PI_2;
    return atan2(x, sqrt(1.0 - x * x));
}

float asinf(float x) { return (float)asin((double)x); }

double acos(double x) {
    if (x < -1.0 || x > 1.0) { errno = EDOM; return NAN; }
    return M_PI_2 - asin(x);
}

float acosf(float x) { return (float)acos((double)x); }

double sinh(double x) { return 0.5 * (exp(x) - exp(-x)); }
float  sinhf(float x) { return (float)sinh((double)x); }
double cosh(double x) { return 0.5 * (exp(x) + exp(-x)); }
float  coshf(float x) { return (float)cosh((double)x); }
double tanh(double x) {
    if (x > 20.0) return 1.0;
    if (x < -20.0) return -1.0;
    double e2 = exp(2.0 * x);
    return (e2 - 1.0) / (e2 + 1.0);
}
float  tanhf(float x) { return (float)tanh((double)x); }
double asinh(double x) { return log(x + sqrt(x * x + 1.0)); }
float  asinhf(float x) { return (float)asinh((double)x); }
double acosh(double x) {
    if (x < 1.0) { errno = EDOM; return NAN; }
    return log(x + sqrt(x * x - 1.0));
}
float  acoshf(float x) { return (float)acosh((double)x); }
double atanh(double x) {
    if (x <= -1.0 || x >= 1.0) { errno = EDOM; return NAN; }
    return 0.5 * log((1.0 + x) / (1.0 - x));
}
float  atanhf(float x) { return (float)atanh((double)x); }

