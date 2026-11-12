/*
 * wubuglott_math.h -- deterministic math for WuBuGLOTT.
 *
 * Slermed from chimera-vocal-synth (crates/chimera/src/math.rs).
 * Every transcendental goes through <math.h> so the engine is
 * bit-reproducible for a given libm; operation order is the original's.
 *
 * The js_* helpers reproduce JavaScript semantics the engine relied on
 * (NaN-propagating min/max, Math.round rounding halves up).
 *
 * C11. Header-only, no allocation.
 */
#ifndef WUBUGLOTT_MATH_H
#define WUBUGLOTT_MATH_H

#include <math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WG_PI  3.14159265358979323846
#define WG_TAU (WG_PI * 2.0)

/* NaN-propagating min/max (JavaScript Math.min/Math.max). */
static inline double wg_js_min(double a, double b)
{
    if (isnan(a) || isnan(b)) return NAN;
    if (a < b) return a;
    if (b < a) return b;
    return signbit(a) ? a : b;
}

static inline double wg_js_max(double a, double b)
{
    if (isnan(a) || isnan(b)) return NAN;
    if (a > b) return a;
    if (b > a) return b;
    return signbit(a) ? b : a;
}

/* Math.round: nearest integer, halves toward +infinity. */
static inline double wg_js_round(double x)
{
    double r = floor(x);
    return (x - r >= 0.5) ? r + 1.0 : r;
}

static inline double wg_js_sign(double x)
{
    if (x > 0.0) return 1.0;
    if (x < 0.0) return -1.0;
    return x;
}

static inline double wg_clamp(double x, double lo, double hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline double wg_lerp(double a, double b, double t)
{
    return a + (b - a) * t;
}

static inline double wg_smoothstep(double e0, double e1, double x)
{
    double t = wg_clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

static inline double wg_db_to_gain(double db)  { return pow(10.0, db / 20.0); }
static inline double wg_gain_to_db(double g)    { return 20.0 * log10(wg_js_max(1e-12, g)); }
static inline double wg_midi_to_hz(double m)    { return 440.0 * pow(2.0, (m - 69.0) / 12.0); }
static inline double wg_hz_to_midi(double f)    { return 69.0 + 12.0 * log2(wg_js_max(1e-6, f) / 440.0); }
static inline double wg_cents_to_ratio(double c){ return pow(2.0, c / 1200.0); }

/* -- deterministic PRNG (mulberry32 core, 32-bit state) ---------------- */
typedef struct {
    uint32_t s;
} wg_rng;

static inline void wg_rng_init(wg_rng *r, uint32_t seed)
{
    r->s = (seed == 0) ? 1u : seed;
}

static inline double wg_rng_next(wg_rng *r)
{
    r->s += 0x6d2b79f5u;
    uint32_t t = r->s;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= (t + ((t ^ (t >> 7)) * (t | 61u)));
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}

/* Uniform in [-1, 1). */
static inline double wg_rng_bipolar(wg_rng *r)
{
    return wg_rng_next(r) * 2.0 - 1.0;
}

/* Approximately standard normal: sum of 4 uniforms, scaled. Bounded. */
static inline double wg_rng_gauss(wg_rng *r)
{
    double a = wg_rng_next(r);
    double b = wg_rng_next(r);
    double c = wg_rng_next(r);
    double d = wg_rng_next(r);
    return (a + b + c + d - 2.0) * 1.7320508;
}

/* -- one-pole smoother, time constant in samples ----------------------- */
typedef struct {
    double value;
    double a;
} wg_smoother;

static inline void wg_smoother_init(wg_smoother *s, double initial, double tau_samples)
{
    s->value = initial;
    s->a = (tau_samples > 0.0) ? exp(-1.0 / tau_samples) : 0.0;
}

static inline double wg_smoother_step(wg_smoother *s, double target)
{
    s->value = target + (s->value - target) * s->a;
    return s->value;
}

/* -- 1/f-ish slow random drift ---------------------------------------- */
/* Sum of three filtered noises at octave-spaced rates. Called at control
 * rate; roughly unit variance. */
typedef struct {
    double y[3];
    double a[3];
    wg_rng rng;
} wg_drift;

static inline void wg_drift_init(wg_drift *d, double control_rate, double base_hz, uint32_t seed)
{
    double f[3] = { base_hz, base_hz * 3.0, base_hz * 9.0 };
    for (int i = 0; i < 3; i++) {
        d->y[i] = 0.0;
        d->a[i] = exp((-WG_TAU * f[i]) / control_rate);
    }
    wg_rng_init(&d->rng, seed);
}

static inline double wg_drift_step(wg_drift *d)
{
    double s = 0.0;
    for (int i = 0; i < 3; i++) {
        double a = d->a[i];
        d->y[i] = a * d->y[i] + sqrt(1.0 - a * a) * wg_rng_gauss(&d->rng);
        s += d->y[i] / (double)(i + 1);
    }
    return s / 1.4;
}

#ifdef __cplusplus
}
#endif

#endif /* WUBUGLOTT_MATH_H */
