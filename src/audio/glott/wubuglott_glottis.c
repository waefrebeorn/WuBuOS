/*
 * wubuglott_glottis.c -- parametric vocal-fold source.
 * See wubuglott_glottis.h for the model. Slermed from chimera-vocal-synth.
 */
#include "wubuglott_glottis.h"
#include "wubuglott_math.h"

/* 1 cmH2O in dyn/cm^2 (physics.rs: CM_H2O). */
#define WG_CM_H2O 980.665

/* Normalised area pulse (0..1) at cycle phase ph. */
double wg_glottis_shape(double ph, double oq, double skew, double sharp)
{
    if (ph >= oq)
        return 0.0;
    double t = ph / oq;
    double s = wg_clamp(skew, 0.3, 0.9);
    if (t < s)
        return 0.5 - 0.5 * cos((WG_PI * t) / s);
    double u = (t - s) / (1.0 - s);
    double soft = 0.5 + 0.5 * cos(WG_PI * u);
    double hard = cos(0.5 * WG_PI * u);
    return soft + (hard - soft) * sharp;
}

void wg_glottis_init(wg_glottis *g, double fs)
{
    g->fs = fs;
    g->params.f0 = 110.0;
    g->params.amplitude = 0.16;
    g->params.abduction = 0.0;
    g->params.chink = 0.006;
    g->params.open_quotient = 0.62;
    g->params.skew = 0.62;
    g->params.closure_sharpness = 0.6;
    g->params.effort = 0.7;
    g->params.jitter = 0.004;
    g->params.shimmer = 0.03;
    g->params.vibrato_depth = 0.0;
    g->params.vibrato_rate = 6.0;
    g->params.drift = 6.0;
    g->params.fry = 0.0;
    g->params.diplophonia = 0.0;
    g->params.closure = 0.0;
    g->params.chaos = 0.0;

    g->phase = 0.0;
    g->cycle_amp = 1.0;
    g->cycle_period_scale = 1.0;
    g->cycle_index = 0;
    g->chaos_x = 0.37;
    g->vib_phase = 0.0;
    wg_rng_init(&g->rng, 777);
    wg_drift_init(&g->drift, fs / 32.0, 0.3, 4242);
    g->drift_value = 0.0;
    g->drift_counter = 0;
    g->pitch_factor = 1.0;
    g->voicing = 0.0;
    g->voicing_sm = 0.0;
    g->dp_sm = 0.0;
    g->dp_coef = 1.0 - exp(-WG_TAU * 30.0 / fs);
    g->voice_coef = 1.0 - exp(-WG_TAU * 25.0 / fs);
    g->area = 0.0;
    g->f0_now = 110.0;
    g->threshold = 2.5 * WG_CM_H2O;
    g->static_sm = 0.0;
    g->closure_sm = 0.0;
    g->amp_sm = 0.0;
    g->sm_coef = 1.0 - exp(-1.0 / (0.004 * fs));
}

void wg_glottis_reset(wg_glottis *g)
{
    g->phase = 0.0;
    g->voicing_sm = 0.0;
    g->dp_sm = 0.0;
}

double wg_glottis_cycle_phase(const wg_glottis *g) { return g->phase; }

double wg_glottis_next(wg_glottis *g, double dp)
{
    wg_glottis_params p = g->params;
    g->dp_sm += g->dp_coef * (dp - g->dp_sm);

    /* drift and vibrato evaluated at a decimated rate (pitch modulation is slow) */
    g->drift_counter -= 1;
    if (g->drift_counter <= 0) {
        g->drift_counter = 32;
        g->drift_value = wg_drift_step(&g->drift);
        g->vib_phase += ((p.vibrato_rate * 32.0) / g->fs) * (1.0 + 0.04 * g->drift_value);
        if (g->vib_phase > 1.0)
            g->vib_phase -= 1.0;
        double cents = p.vibrato_depth * sin(WG_TAU * g->vib_phase) + p.drift * g->drift_value;
        g->pitch_factor = pow(2.0, cents / 1200.0);
    }
    double f0 = p.f0 * g->pitch_factor;
    /* fry pulls the rate down toward ~45 Hz */
    if (p.fry > 0.0)
        f0 = f0 * (1.0 - p.fry) + wg_js_min(f0, 48.0) * p.fry;
    g->f0_now = f0;

    /* voicing gate: pressure above threshold, adduction sufficient, not closed */
    double pressure_gate = wg_smoothstep(g->threshold * 0.8, g->threshold * 1.6, g->dp_sm);
    double adduct_gate = 1.0 - wg_smoothstep(0.08, 0.22, g->static_sm - p.chink);
    double target = pressure_gate * adduct_gate * (1.0 - g->closure_sm);
    g->voicing_sm += g->voice_coef * (target - g->voicing_sm);
    g->voicing = g->voicing_sm;

    /* phase advance with per-cycle perturbations */
    g->phase += (f0 / g->fs) / g->cycle_period_scale;
    if (g->phase >= 1.0) {
        g->phase -= 1.0;
        g->cycle_index += 1;
        double alt = ((g->cycle_index & 1u) == 1u) ? 1.0 : -1.0;
        double chaos_p = 0.0;
        double chaos_a = 0.0;
        if (p.chaos > 0.0) {
            double r = 2.9 + 1.1 * wg_js_min(1.0, p.chaos);
            g->chaos_x = r * g->chaos_x * (1.0 - g->chaos_x);
            if (!(g->chaos_x > 0.0 && g->chaos_x < 1.0))
                g->chaos_x = 0.37;
            double dev = g->chaos_x - 0.5;
            chaos_p = 0.45 * p.chaos * dev;
            chaos_a = 1.1 * p.chaos * dev;
        }
        g->cycle_period_scale = 1.0 + p.jitter * wg_rng_gauss(&g->rng)
                             + p.diplophonia * 0.08 * alt + chaos_p;
        g->cycle_period_scale = wg_clamp(g->cycle_period_scale, 0.55, 1.8);
        g->cycle_amp = wg_js_max(0.12,
            1.0 + p.shimmer * wg_rng_gauss(&g->rng)
            - p.diplophonia * 0.35 * ((alt > 0.0) ? 1.0 : 0.0) + chaos_a);
    }

    /* pulse shape within the cycle */
    double oq = wg_clamp(p.open_quotient - 0.18 * (p.effort - 0.5), 0.25, 0.98);
    if (p.fry > 0.0)
        oq = oq * (1.0 - p.fry) + 0.28 * p.fry;
    double pulse = wg_glottis_shape(g->phase, oq, p.skew, p.closure_sharpness);
    if (p.fry > 0.3) {
        /* second, weaker pulse shortly after the first (fry double pulsing) */
        double ph2 = g->phase - 0.38;
        if (ph2 > 0.0)
            pulse += 0.55 * wg_smoothstep(0.3, 0.8, p.fry)
                   * wg_glottis_shape(ph2, oq * 0.8, p.skew, p.closure_sharpness);
    }

    double effort_scale = 0.45 + 0.75 * p.effort;
    double sc = g->sm_coef;
    g->amp_sm += sc * (p.amplitude * effort_scale - g->amp_sm);
    g->closure_sm += sc * (p.closure - g->closure_sm);
    g->static_sm += sc * (p.abduction + p.chink - g->static_sm);

    double vib = g->amp_sm * g->cycle_amp * g->voicing_sm * pulse;
    double statics = g->static_sm * (1.0 - 0.92 * g->closure_sm);
    double a = wg_js_max(0.0, vib + statics);
    g->area = a;
    return a;
}
