#ifdef __cplusplus
extern "C" {
#endif
#ifndef _MATH_H
#define _MATH_H

#include <stdint.h>

#define M_E        2.71828182845904523536
#define M_LOG2E    1.44269504088896340736
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530942
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_1_PI     0.31830988618379067154
#define M_2_PI     0.63661977236758134308
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2    1.41421356237309504880
#define M_SQRT1_2  0.70710678118654752440

#define HUGE_VAL   (__builtin_huge_val())
#define HUGE_VALF  (__builtin_huge_valf())
#define INFINITY   (__builtin_inff())
#define NAN        (__builtin_nanf(""))

#define isnan(x)      __builtin_isnan(x)
#define isinf(x)      __builtin_isinf(x)
#define isfinite(x)   __builtin_isfinite(x)
#define signbit(x)    __builtin_signbit(x)

typedef double double_t;
typedef float  float_t;

// Basic & Remainder (double & float)
double fabs(double x);
float  fabsf(float x);
double fmod(double x, double y);
float  fmodf(float x, float y);
double remainder(double x, double y);
float  remainderf(float x, float y);
double fmax(double x, double y);
float  fmaxf(float x, float y);
double fmin(double x, double y);
float  fminf(float x, float y);
double fdim(double x, double y);
float  fdimf(float x, float y);
double fma(double x, double y, double z);
float  fmaf(float x, float y, float z);
double copysign(double x, double y);
float  copysignf(float x, float y);

// Power & Root (double & float)
double sqrt(double x);
float  sqrtf(float x);
double cbrt(double x);
float  cbrtf(float x);
double hypot(double x, double y);
float  hypotf(float x, float y);
double pow(double x, double y);
float  powf(float x, float y);

// Exponential & Logarithmic (double & float)
double exp(double x);
float  expf(float x);
double exp2(double x);
float  exp2f(float x);
double expm1(double x);
float  expm1f(float x);
double log(double x);
float  logf(float x);
double log2(double x);
float  log2f(float x);
double log10(double x);
float  log10f(float x);
double log1p(double x);
float  log1pf(float x);

// Trigonometric (double & float)
double sin(double x);
float  sinf(float x);
double cos(double x);
float  cosf(float x);
double tan(double x);
float  tanf(float x);
double asin(double x);
float  asinf(float x);
double acos(double x);
float  acosf(float x);
double atan(double x);
float  atanf(float x);
double atan2(double y, double x);
float  atan2f(float y, float x);

// Hyperbolic (double & float)
double sinh(double x);
float  sinhf(float x);
double cosh(double x);
float  coshf(float x);
double tanh(double x);
float  tanhf(float x);
double asinh(double x);
float  asinhf(float x);
double acosh(double x);
float  acoshf(float x);
double atanh(double x);
float  atanhf(float x);

// Rounding & IEEE-754 Decomposition (double & float)
double    ceil(double x);
float     ceilf(float x);
double    floor(double x);
float     floorf(float x);
double    trunc(double x);
float     truncf(float x);
double    round(double x);
float     roundf(float x);
long      lround(double x);
long      lroundf(float x);
long long llround(double x);
long long llroundf(float x);
double    rint(double x);
float     rintf(float x);
long      lrint(double x);
long      lrintf(float x);
long long llrint(double x);
long long llrintf(float x);
double    nearbyint(double x);
float     nearbyintf(float x);
double    frexp(double x, int *exp);
float     frexpf(float x, int *exp);
double    ldexp(double x, int exp);
float     ldexpf(float x, int exp);
double    scalbn(double x, int n);
float     scalbnf(float x, int n);
double    modf(double x, double *iptr);
float     modff(float x, float *iptr);
int       ilogb(double x);
int       ilogbf(float x);
double    logb(double x);
float     logbf(float x);

#endif

#ifdef __cplusplus
}
#endif
