/*
 * wubuglott_glottis.h -- parametric vocal-fold source.
 *
 * Generates the glottal AREA waveform, which drives the glottal port of the
 * airway network. Flow is computed by the network (Bernoulli + inertance),
 * so pulse skewing, F1 ripple and devoicing under oral closure emerge from
 * the physics rather than being scripted.
 *
 * Voicing is gated physiologically:
 *   - transglottal pressure must exceed the phonation threshold (~2-3 cmH2O)
 *   - wide abduction (voiceless consonants, /h/) stops vibration
 *
 * Slermed from chimera-vocal-synth, crates/chimera/src/sources/glottis.rs.
 * C11, no allocation after init.
 */
#ifndef WUBUGLOTT_GLOTTIS_H
#define WUBUGLOTT_GLOTTIS_H

#include <stdint.h>

#include "wubuglott_math.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double f0;                /* fundamental frequency (Hz) */
    double amplitude;         /* peak vibratory area (cm^2) at full effort */
    double abduction;         /* static opening between vocal processes (cm^2) */
    double chink;             /* posterior chink leakage (cm^2), always open */
    double open_quotient;     /* 0..1 */
    double skew;              /* fraction of the open phase spent opening */
    double closure_sharpness; /* 0 smooth closure .. 1 abrupt slope */
    double effort;            /* vocal effort 0..1 */
    double jitter;            /* cycle-to-cycle period perturbation (fraction) */
    double shimmer;           /* cycle-to-cycle amplitude perturbation */
    double vibrato_depth;     /* cents */
    double vibrato_rate;      /* Hz */
    double drift;             /* slow random pitch drift (cents) */
    double fry;               /* vocal fry 0..1 */
    double diplophonia;       /* period doubling 0..1 */
    double closure;           /* extra glottal closure force 0..1 */
    double chaos;             /* logistic-map bifurcation 0..1 */
} wg_glottis_params;

#define WG_GLOTTIS_DEFAULT { \
    .f0 = 110.0, .amplitude = 0.16, .abduction = 0.0, .chink = 0.006, \
    .open_quotient = 0.62, .skew = 0.62, .closure_sharpness = 0.6, \
    .effort = 0.7, .jitter = 0.004, .shimmer = 0.03, \
    .vibrato_depth = 0.0, .vibrato_rate = 6.0, .drift = 6.0, \
    .fry = 0.0, .diplophonia = 0.0, .closure = 0.0, .chaos = 0.0 \
}

typedef struct {
    double fs;
    wg_glottis_params params;

    double phase;
    double cycle_amp;
    double cycle_period_scale;
    uint64_t cycle_index;
    double chaos_x;
    double vib_phase;

    wg_rng rng;
    wg_drift drift;
    double drift_value;
    int drift_counter;
    double pitch_factor;

    double voicing;      /* smoothed voicing gate 0..1 */
    double voicing_sm;
    double dp_sm;        /* smoothed transglottal pressure (dyn/cm^2) */
    double dp_coef;
    double voice_coef;

    double area;         /* last area (telemetry) */
    double f0_now;       /* instantaneous f0 used (telemetry) */
    double threshold;    /* phonation threshold pressure (dyn/cm^2) */

    double static_sm;
    double closure_sm;
    double amp_sm;
    double sm_coef;
} wg_glottis;

void wg_glottis_init(wg_glottis *g, double fs);
void wg_glottis_reset(wg_glottis *g);

/* Advance one sample. dp = instantaneous transglottal pressure
 * (p_sub - p_supra) in dyn/cm^2. Returns the glottal area in cm^2. */
double wg_glottis_next(wg_glottis *g, double dp);

double wg_glottis_cycle_phase(const wg_glottis *g);

/* The normalised area pulse (0..1) at cycle phase ph. Exposed for the UI
 * (the fold display) and for the gate. */
double wg_glottis_shape(double ph, double oq, double skew, double sharp);

#ifdef __cplusplus
}
#endif

#endif /* WUBUGLOTT_GLOTTIS_H */
