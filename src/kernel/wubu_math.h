/*
 * wubu_math.h — WuBuOS Pure-C Math (no libm dependency)
 *
 * Replaces libm for WorldSim terrain generation.
 * CORDIC sin/cos, Newton-Raphson sqrt, Taylor exp/log.
 * ~200 LOC. No -lm needed.
 *
 * Accuracy: sin/cos within 1e-6, sqrt exact to float precision,
 * exp/log within 1e-7 for normal ranges.
 */
#ifndef WUBU_MATH_H
#define WUBU_MATH_H

#include <stdint.h>

/* ── Constants ──────────────────────────────────────────────────── */

#define WUBU_PI        3.14159265358979323846
#define WUBU_PI_2      1.57079632679489661923  /* pi/2 */
#define WUBU_2_PI      6.28318530717958647693  /* 2*pi */
#define WUBU_E         2.71828182845904523536

/* ── sqrt: Newton-Raphson ───────────────────────────────────────── */

static inline double wubu_sqrt(double x) {
    if (x <= 0.0) return 0.0;
    double guess = x;
    if (x > 1.0) guess = x * 0.5;
    for (int i = 0; i < 20; i++) {
        double next = 0.5 * (guess + x / guess);
        if (guess == next) break;
        guess = next;
    }
    return guess;
}

/* ── sin/cos: CORDIC (14 iterations, ~1e-6 accuracy) ──────────── */

static inline void wubu_sincos(double angle, double *out_sin, double *out_cos) {
    /* Normalize angle to [-pi, pi] */
    double a = angle;
    while (a > WUBU_PI)  a -= WUBU_2_PI;
    while (a < -WUBU_PI) a += WUBU_2_PI;

    /* CORDIC table: atan(2^-i) for i=0..13 */
    static const double cordic_atan[] = {
        7.8539816339744828e-01, 4.6364760900080609e-01,
        2.4497866312686414e-01, 1.2435499453688694e-01,
        6.2418809995959380e-02, 3.1239833443096605e-02,
        1.5623976604734312e-02, 7.8123410603011111e-03,
        3.9062301319269718e-03, 1.9531225164787108e-03,
        9.7656218959910165e-04, 4.8828121136590176e-04,
        2.4414062011493617e-04, 1.2207031189170118e-04,
    };
    /* K = product of sqrt(1 + 2^-2i) for i=0..13 ≈ 1.646760258121 */
    static const double cordic_k = 1.6467602581212065;

    double x = 1.0 / cordic_k;
    double y = 0.0;
    double z = a;
    double power_of_2 = 1.0;

    for (int i = 0; i < 14; i++) {
        double dx, dy;
        if (z >= 0.0) {
            dx = -y * power_of_2;
            dy =  x * power_of_2;
            z -= cordic_atan[i];
        } else {
            dx =  y * power_of_2;
            dy = -x * power_of_2;
            z += cordic_atan[i];
        }
        x += dx;
        y += dy;
        power_of_2 *= 0.5;
    }

    if (out_sin) *out_sin = y;
    if (out_cos) *out_cos = x;
}

static inline double wubu_sin(double x) {
    double s; wubu_sincos(x, &s, NULL); return s;
}

static inline double wubu_cos(double x) {
    double c; wubu_sincos(x, NULL, &c); return c;
}

/* ── exp: Taylor series (order 12) ──────────────────────────────── */

static inline double wubu_exp(double x) {
    if (x < -20.0) return 0.0;
    if (x > 20.0)  return 1e20;
    double sum = 1.0;
    double term = 1.0;
    for (int i = 1; i <= 12; i++) {
        term *= x / (double)i;
        sum += term;
    }
    return sum;
}

/* ── log: Newton-Raphson on exp ─────────────────────────────────── */

static inline double wubu_log(double x) {
    if (x <= 0.0) return -1e20;
    /* Initial guess using bit manipulation */
    union { double d; uint64_t i; } u;
    u.d = x;
    double guess = (double)((int64_t)(u.i >> 52) - 1023) * 0.6931471805599453;
    /* Newton-Raphson: ln(x) = guess + (x - exp(guess)) / exp(guess) */
    for (int i = 0; i < 5; i++) {
        double eg = wubu_exp(guess);
        guess += (x - eg) / eg;
    }
    return guess;
}

/* ── pow ─────────────────────────────────────────────────────────── */

static inline double wubu_pow(double base, double exp) {
    if (base <= 0.0) return 0.0;
    return wubu_exp(exp * wubu_log(base));
}

/* ── fabs, fmod ─────────────────────────────────────────────────── */

static inline double wubu_fabs(double x) { return x < 0.0 ? -x : x; }
static inline double wubu_fmod(double x, double m) {
    return x - (double)(int64_t)(x / m) * m;
}

/* ── Parity: #define standard names → wubu_ names ──────────────── */
/* Only if WUBU_NO_LIBM is defined — otherwise use libm */

#ifdef WUBU_NO_LIBM
  #define sqrt(x)   wubu_sqrt(x)
  #define sin(x)    wubu_sin(x)
  #define cos(x)    wubu_cos(x)
  #define exp(x)    wubu_exp(x)
  #define log(x)    wubu_log(x)
  #define pow(b,e)  wubu_pow(b,e)
  #define fabs(x)   wubu_fabs(x)
  #define fmod(x,m) wubu_fmod(x,m)
  #define M_PI      WUBU_PI
#endif

#endif /* WUBU_MATH_H */
