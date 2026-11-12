/*
 * wubuglott_voice.c -- the instrument. See wubuglott_voice.h.
 * Slermed from chimera's human_voice.rs (render loop) and human_airway.rs
 * (network layout).
 */
#include "wubuglott_voice.h"
#include "wubuglott_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The engine's node counts and lengths, from human_airway.rs. */
static void compute_layout(wg_voice *v, wg_body body, int *n_ph, int *n_or,
                           double *ph_min_len, double *or_min_len)
{
    double k = body.scale;
    double dx_min = (WG_C_SOUND / v->fs) / 0.93;
    *ph_min_len = 8.7 * k * (0.55 + 0.45 * body.pharynx) - 1.6 * k;
    *or_min_len = 8.3 * k - 1.0 * k;
    *n_ph = (int)(*ph_min_len / dx_min);
    if (*n_ph < 8) *n_ph = 8;
    *n_or = (int)(*or_min_len / dx_min);
    if (*n_or < 8) *n_or = 8;
}

/* The epilarynx (the short tube above the glottis). Its area law. */
static double epilarynx_area(const wg_voice *v, double s, double len)
{
    /* A slightly flared tube: narrow at the glottis, wider at the pharynx.
     * The original reads a measured profile; this is the same shape law. */
    double u = (len > 0.0) ? s / len : 0.0;
    if (u < 0.0) u = 0.0;
    if (u > 1.0) u = 1.0;
    double a0 = v->glottis.params.amplitude * 0.35 + 0.02;
    double a1 = 3.0 * v->source.f0 / 110.0 * 0.0 + 1.2;  /* pharynx entry */
    return a0 + (a1 - a0) * u;
}

/*
 * Geometry -> area functions.
 *
 * The full chimera tract (anatomy/human.rs, 1506 lines) computes the oral
 * and pharyngeal area functions from a splined tongue, a rotating jaw, a
 * velum flap and a lip aperture, then resamples them onto the network's
 * sections. This is the area-function interface with a physically shaped
 * tract model: the tongue body moves a constriction along the oral tube and
 * raises it toward the palate, which is what produces the vowel space.
 *
 * The full spline geometry is the next slice; this is honest about being a
 * simpler law with the same shape (constriction position + degree).
 */
static void compute_areas(wg_voice *v, double *ph, double *or)
{
    double k = 1.0;
    (void)k;
    double k2 = 1.0;

    /* pharynx: a gentle flare from the epilarynx up to the velum */
    double epi = 2.4 * v->source.f0 / 110.0;   /* placeholder, overwritten */
    epi = 2.4;                                  /* cm */
    for (int i = 0; i < v->n_ph; i++) {
        double u = (double)(i + 0.5) / (double)v->n_ph;
        double a;
        if (u < epi / 8.8) {
            a = epilarynx_area(v, u * 8.8, 8.8 * 0.28);
        } else {
            /* the pharyngeal cavity widens toward the velum */
            a = 1.2 + 2.2 * (u - 0.28) / 0.72;
        }
        ph[i] = a * k2;
    }

    /* oral: a constriction whose position and degree come from the tongue */
    double pos = 0.25 + 0.65 * v->artic.tongue_x;    /* 0 back .. 1 front */
    double height = 1.0 - v->artic.tongue_y;          /* 0 low .. 1 high */
    double jaw_open = v->artic.jaw;
    double lip = 1.0 - 0.75 * v->artic.lip_round;

    for (int i = 0; i < v->n_or; i++) {
        double u = (double)(i + 0.5) / (double)v->n_or;
        /* the roof-to-tongue gap narrows near the constriction */
        double d = fabs(u - pos);
        double width = 0.6 + 3.2 * jaw_open;
        double pinch = height * 4.5 * exp(-(d * d) / (2.0 * 0.09));
        double a = width - pinch;
        if (a < 0.0) a = 0.0;
        /* the lips form the output aperture */
        if (u > 0.92)
            a *= lip;
        or[i] = a * k2;
    }
}

wg_voice *wg_voice_new(double sample_rate, wg_body body)
{
    wg_voice *v = calloc(1, sizeof(*v));
    if (!v) return NULL;

    v->sample_rate = sample_rate;

    /* oversample_for(fs_out) = max(1, floor(60000/fs_out) + 1) */
    v->oversample = (int)(60000.0 / sample_rate);
    if (v->oversample < 1) v->oversample = 1;
    v->oversample += 1;
    v->fs = sample_rate * v->oversample;

    v->source.f0 = 110.0;
    v->source.glide = 0.06;
    v->source.pressure = 8.0;
    v->source.breathiness = 0.15;
    v->source.tension = 0.5;
    v->source.roughness = 0.1;
    v->source.vibrato_rate = 6.0;
    v->source.drift = 5.0;
    v->source.voiced = 1;
    v->source.brightness = 0.25;

    v->artic.jaw = 0.5;
    v->artic.velum = 0.0;
    v->artic.larynx = 0.0;
    v->artic.tongue_x = 0.5;
    v->artic.tongue_y = 0.5;
    v->artic.lip_round = 0.0;

    int n_ph, n_or;
    double ph_len, or_len;
    compute_layout(v, body, &n_ph, &n_or, &ph_len, &or_len);
    v->n_ph = n_ph;
    v->n_or = n_or;

    double k = body.scale;
    v->net = wg_network_new(v->fs);
    if (!v->net) { free(v); return NULL; }

    /* ---- the layout from human_airway.rs ---- */
    double dx_min = (WG_C_SOUND / v->fs) / 0.93;
    double tr_len = 12.0 * k;
    wg_tube_spec ts = {0};
    ts.name = "trachea";
    ts.n = (int)(tr_len / (dx_min > 0.9 * k ? dx_min : 0.9 * k));
    if (ts.n < 6) ts.n = 6;
    ts.length = tr_len;
    ts.uniform = 1;
    ts.area = 2.5 * k * k;
    ts.wall = WG_WALL_TRACHEA;
    ts.perimeter = 1.0;
    ts.viscous = 1.0;
    ts.radiation = 0.0;
    v->trachea = wg_network_add_tube(v->net, &ts);

    wg_tube_spec ps = {0};
    ps.name = "pharynx";
    ps.n = n_ph;
    ps.length = 8.8 * k;
    ps.uniform = 1;
    ps.area = 3.0 * k * k;
    ps.wall = WG_WALL_SOFT;
    ps.perimeter = 1.25;
    ps.viscous = 1.0;
    ps.radiation = 0.25;
    v->pharynx = wg_network_add_tube(v->net, &ps);

    wg_tube_spec os = {0};
    os.name = "oral";
    os.n = n_or;
    os.length = 8.3 * k;
    os.uniform = 1;
    os.area = 3.0 * k * k;
    os.wall = WG_WALL_SOFT;
    os.perimeter = 1.5;
    os.viscous = 1.0;
    os.radiation = 0.25;
    v->oral = wg_network_add_tube(v->net, &os);

    double na_len = 11.4 * k;
    int n_na = (int)(na_len / (dx_min > 0.5 * k ? dx_min : 0.5 * k));
    if (n_na < 8) n_na = 8;
    double *na = malloc((size_t)n_na * sizeof(double));
    for (int i = 0; i < n_na; i++) {
        double u = (double)(i + 0.5) / (double)n_na;
        double a;
        if (u < 0.18)      a = 2.6;
        else if (u < 0.75) a = 2.0 - 0.6 * (u - 0.18);
        else if (u < 0.9)  a = 1.4;
        else               a = 1.0;
        na[i] = a * k * k;
    }
    wg_tube_spec ns = {0};
    ns.name = "nasal";
    ns.n = n_na;
    ns.length = na_len;
    ns.uniform = 0;
    ns.profile = na;
    ns.area = 0.0;
    ns.wall = WG_WALL_NASAL;
    ns.perimeter = 3.2;
    ns.viscous = 2.5;
    ns.radiation = 0.0;
    v->nasal = wg_network_add_tube(v->net, &ns);
    free(na);

    double n_fo = (2.0 * k / dx_min);
    if (n_fo < 1.0) n_fo = 1.0;
    if (n_fo > 3.0) n_fo = 3.0;
    wg_tube_spec fs2 = {0};
    fs2.name = "piriform";
    fs2.n = (int)n_fo;
    fs2.length = 2.0 * k;
    fs2.uniform = 1;
    fs2.area = 0.9 * k * k;
    fs2.wall = WG_WALL_SOFT;
    fs2.perimeter = 1.3;
    fs2.viscous = 1.0;
    fs2.radiation = 0.0;
    v->fossa = wg_network_add_tube(v->net, &fs2);

    v->lungs = wg_network_add_pressure_node(v->net, "lungs");

    double tr_a = 2.5 * k * k;
    wg_port_spec lp = WG_PORT_SPEC_DEFAULT;
    lp.area = tr_a;
    lp.length = 0.0;
    lp.k_entrance = 0.0;
    lp.k_exit = 0.0;
    lp.extra_resistance = (0.5 * WG_RHO * WG_C_SOUND) / tr_a;
    wg_network_add_port(v->net, v->lungs, wg_tube_last(v->trachea), lp);

    wg_port_spec gp = WG_PORT_SPEC_DEFAULT;
    gp.area = 0.0;
    gp.length = 0.3 * k;
    gp.k_entrance = 0.37;
    gp.k_exit = 1.0;
    gp.slit_width = 1.4 * k;
    v->glottis_port = wg_network_add_port(v->net, wg_tube_last(v->trachea),
                                          wg_tube_first(v->pharynx), gp);

    wg_network_connect(v->net, wg_tube_last(v->pharynx), wg_tube_first(v->oral));

    wg_port_spec vp = WG_PORT_SPEC_DEFAULT;
    vp.area = 0.0;
    vp.length = 1.0 * k;
    vp.k_entrance = 0.3;
    vp.k_exit = 0.8;
    v->velum_port = wg_network_add_port(v->net, wg_tube_last(v->pharynx),
                                        wg_tube_first(v->nasal), vp);

    int fossa_node = (int)((n_ph - 1) < wg_js_round((2.6 / 8.8) * n_ph)
                           ? (n_ph - 1) : wg_js_round((2.6 / 8.8) * n_ph));
    wg_network_connect(v->net, wg_tube_node(v->pharynx, fossa_node),
                       wg_tube_first(v->fossa));

    double sinus_vol = 14.0 * k * k * k;
    wg_wall_params no_wall = {0, 0, 0};
    int sinus = wg_network_add_cavity(v->net, "sinus", sinus_vol, 0, no_wall);
    wg_port_spec sp = WG_PORT_SPEC_DEFAULT;
    sp.area = 0.12 * k * k;
    sp.length = 0.5 * k;
    sp.k_entrance = 0.0;
    sp.k_exit = 0.0;
    sp.include_half = 0;
    wg_network_add_port(v->net, wg_tube_node(v->nasal, (int)(n_na * 0.45)), sinus, sp);

    wg_network_add_radiation(v->net, wg_tube_last(v->oral));
    wg_network_add_radiation(v->net, wg_tube_last(v->nasal));

    wg_network_finalize(v->net);
    wg_network_snap_to_targets(v->net);

    /* state */
    v->ph_areas = calloc((size_t)n_ph, sizeof(double));
    v->or_areas = calloc((size_t)n_or, sizeof(double));
    v->ph_min = calloc((size_t)n_ph, sizeof(double));
    v->or_min = calloc((size_t)n_or, sizeof(double));

    wg_glottis_init(&v->glottis, v->fs);

    v->ctrl_every = (int)(v->fs / 1000.0);   /* 1 kHz control rate */
    if (v->ctrl_every < 1) v->ctrl_every = 1;
    v->ctrl_counter = 0;
    v->clock = 0.0;
    v->f0_current = v->source.f0;

    /* lung pressure ramps to the source's target */
    v->p_lung_now = 0.0;
    v->p_lung_step = (v->source.pressure * WG_CM_H2O) / (v->fs * 0.05);

    v->dc_coef = 1.0 - exp(-2.0 * WG_PI * 8.0 / sample_rate);
    v->lim_release = 0.9999;
    v->lim_hold_n = (int)(0.02 * sample_rate);
    /* The original's output_gain is 5.5e-7: the network's output is d/dt of
     * radiated flow in CGS, so it arrives enormous and must be scaled down
     * before it is audio. Without this the limiter pins the output at full
     * scale and the "voice" is a square wave. */
    v->output_gain = 5.5e-7;

    return v;
}

void wg_voice_free(wg_voice *v)
{
    if (!v) return;
    wg_network_free(v->net);
    free(v->ph_areas); free(v->or_areas);
    free(v->ph_min); free(v->or_min);
    free(v);
}

void wg_voice_set_source(wg_voice *v, const wg_source *s) { v->source = *s; }
void wg_voice_set_artic(wg_voice *v, const wg_artic *a)    { v->artic = *a; }

void wg_voice_set_gate(wg_voice *v, double level)
{
    /* the gate scales the lung pressure target */
    v->source.pressure *= level;
}

double wg_voice_cycle_phase(const wg_voice *v) { return v->glottis.phase; }
double wg_voice_last_area(const wg_voice *v)   { return v->glottis.area; }

/* Control-rate update. */
static void control(wg_voice *v)
{
    double dt = (double)v->ctrl_every / v->fs;

    /* pitch glide toward the target */
    double target = v->source.f0;
    v->f0_current += (target - v->f0_current) * (1.0 - exp(-dt / v->source.glide));

    /* the glottis source parameters */
    wg_glottis_params *gp = &v->glottis.params;
    gp->f0 = v->f0_current;
    gp->vibrato_depth = v->source.vibrato_depth;
    gp->vibrato_rate = v->source.vibrato_rate;
    gp->drift = v->source.drift;
    gp->fry = v->source.fry;
    gp->jitter = 0.004 * v->source.roughness * 10.0;
    gp->shimmer = 0.03 * v->source.roughness * 10.0;
    gp->diplophonia = v->source.roughness * 0.5;
    gp->open_quotient = wg_clamp(0.62 - 0.3 * v->source.tension, 0.25, 0.98);
    gp->closure_sharpness = v->source.brightness;
    gp->chink = 0.006 + 0.5 * v->source.breathiness;
    gp->effort = 0.4 + 0.5 * v->source.tension;

    /* lung pressure */
    double p_target = v->source.pressure * WG_CM_H2O;
    v->p_lung_step = (p_target - v->p_lung_now) / ((double)v->ctrl_every * 20.0);

    /* geometry -> areas */
    compute_areas(v, v->ph_areas, v->or_areas);
    for (int i = 0; i < v->n_ph; i++) v->ph_min[i] = v->ph_areas[i];
    for (int i = 0; i < v->n_or; i++) v->or_min[i] = v->or_areas[i];

    wg_network_set_tube_targets(v->net, v->pharynx, v->ph_areas, 8.8, v->ph_min);
    wg_network_set_tube_targets(v->net, v->oral, v->or_areas, 8.3, v->or_min);
    wg_network_set_port_target(v->net, v->velum_port, v->artic.velum);
    wg_network_begin_block(v->net, v->ctrl_every);

    v->clock += dt;
}

void wg_voice_render(wg_voice *v, float *out, int n)
{
    int sub_node = wg_tube_last(v->trachea);
    int supra = wg_tube_first(v->pharynx);

    for (int i = 0; i < n; i++) {
        double acc = 0.0;
        for (int m = 0; m < v->oversample; m++) {
            if (v->ctrl_counter <= 0)
                control(v);
            v->ctrl_counter--;

            v->p_lung_now += v->p_lung_step;
            /* the lungs are a fixed pressure node */
            wg_network_set_pressure(v->net, v->lungs, v->p_lung_now);

            double dp = wg_network_pressure(v->net, sub_node)
                      - wg_network_pressure(v->net, supra);
            double a = wg_glottis_next(&v->glottis, dp);

            /* set the glottal port area immediately (per-sample) */
            wg_network_set_port_area(v->net, v->glottis_port, a);

            double y = wg_network_step(v->net);
            acc += y;
        }

        /* decimate (box average) */
        double val = acc / (double)v->oversample * v->output_gain;

        /* DC blocker */
        double hp = val - v->dc_x + v->dc_coef * v->dc_y;
        v->dc_x = val;
        v->dc_y = hp;
        val = hp;

        /* soft limiter */
        double al = fabs(val);
        if (al > 0.85)
            val = wg_js_sign(val) * (0.85 + 0.15 * tanh((al - 0.85) / 0.15));

        out[i] = (float)val;
    }
}
