/*
 * wubuglott_voice.h -- the instrument: lungs -> glottis -> airway -> sound.
 *
 * Slermed from chimera-vocal-synth: voice/human_voice.rs (the render loop)
 * and voice/human_airway.rs (the network layout). The voice renders at an
 * oversampled internal rate and decimates, so a host can ask for any block
 * size and get the same samples.
 *
 * C11, no allocation once warm.
 */
#ifndef WUBUGLOTT_VOICE_H
#define WUBUGLOTT_VOICE_H

#include <stdint.h>

#include "wubuglott_network.h"
#include "wubuglott_glottis.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Source parameters (VoiceSource in human_voice.rs). */
typedef struct {
    double f0;             /* target fundamental frequency (Hz) */
    double glide;          /* pitch glide time constant (s) */
    double pressure;       /* lung pressure while breathing out (cmH2O) */
    double breathiness;    /* 0..1 breathy (abducted, chink) */
    double tension;        /* 0 lax .. 1 pressed */
    double fry;            /* vocal fry 0..1 */
    double roughness;      /* jitter / shimmer / period doubling 0..1 */
    double vibrato_depth;  /* cents */
    double vibrato_rate;   /* Hz */
    double drift;          /* random pitch drift (cents) */
    int    voiced;         /* 0 -> whisper (no fold vibration) */
    double brightness;     /* 0 dark .. 1 bright (closure abruptness) */
} wg_source;

#define WG_SOURCE_DEFAULT { \
    .f0 = 110.0, .glide = 0.06, .pressure = 8.0, .breathiness = 0.15, \
    .tension = 0.5, .fry = 0.0, .roughness = 0.1, .vibrato_depth = 0.0, \
    .vibrato_rate = 6.0, .drift = 5.0, .voiced = 1, .brightness = 0.25 \
}

/* Body: male / female / child, or a blend of the last two. */
typedef struct {
    double scale;      /* overall size */
    double pharynx;    /* pharynx length factor */
    double female;     /* 0..1 blend toward female */
    double child;      /* 0..1 blend toward child */
} wg_body;

#define WG_BODY_MALE   ((wg_body){ 1.00, 1.00, 0.0, 0.0 })
#define WG_BODY_FEMALE ((wg_body){ 0.88, 0.92, 1.0, 0.0 })
#define WG_BODY_CHILD  ((wg_body){ 0.72, 0.85, 0.0, 1.0 })

/* Articulation targets. Values are the engine's own units. */
typedef struct {
    double jaw;        /* 0 closed .. 1 open (mapped to the engine's range) */
    double velum;      /* 0 closed .. 1.2 fully open (nasal port area) */
    double larynx;     /* larynx height (cm, scaled) */
    double tongue_x;   /* tongue body forward 0..1 */
    double tongue_y;   /* tongue body high 0..1 */
    double lip_round;  /* 0 spread .. 1 rounded */
} wg_artic;

#define WG_ARTIC_DEFAULT { .jaw = 0.5, .velum = 0.0, .larynx = 0.0, \
                           .tongue_x = 0.5, .tongue_y = 0.5, .lip_round = 0.0 }

typedef struct {
    double sample_rate;   /* output rate (Hz) */
    double fs;            /* internal rate (Hz) */
    int    oversample;

    wg_network *net;
    wg_glottis  glottis;

    wg_source source;
    wg_artic  artic;

    /* airway layout indices */
    int lungs, glottis_port, velum_port;
    int n_ph, n_or;
    wg_tube trachea, pharynx, oral, nasal, fossa;

    /* area targets */
    double *ph_areas, *or_areas, *ph_min, *or_min;

    /* lung pressure state */
    double p_lung_now, p_lung_step;

    /* control-rate counter */
    int ctrl_every, ctrl_counter;
    double clock;

    /* pitch glide */
    double f0_current;

    /* decimator (simple box average for now) */
    double dec_acc;
    int    dec_n, dec_phase;

    /* DC blocker */
    double dc_x, dc_y, dc_coef;

    /* output limiter */
    double lim_env, lim_hold, lim_hold_n, lim_release;
    double output_gain;
} wg_voice;

/* Create a voice at `sample_rate` for `body`. Returns NULL on failure. */
wg_voice *wg_voice_new(double sample_rate, wg_body body);
void      wg_voice_free(wg_voice *v);

void wg_voice_set_source(wg_voice *v, const wg_source *s);
void wg_voice_set_artic(wg_voice *v, const wg_artic *a);
void wg_voice_set_gate(wg_voice *v, double level);

/* Render `n` floats into `out`. */
void wg_voice_render(wg_voice *v, float *out, int n);

/* Telemetry. */
double wg_voice_cycle_phase(const wg_voice *v);
double wg_voice_last_area(const wg_voice *v);

#ifdef __cplusplus
}
#endif

#endif /* WUBUGLOTT_VOICE_H */
