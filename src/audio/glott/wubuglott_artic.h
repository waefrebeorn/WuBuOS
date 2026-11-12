/*
 * wubuglott_artic.h -- the articulatory model: parameters and bodies.
 *
 * Slermed from chimera-vocal-synth, crates/chimera/src/anatomy/human.rs
 * (HumanArtic, ArticKey, NEUTRAL, HumanBody, BodyName, BodyMix).
 *
 * Units are centimetres in the head frame, except the normalized
 * 0..1 keys. The tract's `compute` turns these into area functions; this
 * header is the control surface.
 *
 * C11.
 */
#ifndef WUBUGLOTT_ARTIC_H
#define WUBUGLOTT_ARTIC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WG_N_ARTIC 17

typedef enum {
    WG_ARTIC_JAW = 0,
    WG_ARTIC_TB_X,
    WG_ARTIC_TB_Y,
    WG_ARTIC_TT_X,
    WG_ARTIC_TT_Y,
    WG_ARTIC_TT_CURL,
    WG_ARTIC_ROOT,
    WG_ARTIC_LIP_AP,
    WG_ARTIC_LIP_PROT,
    WG_ARTIC_LIP_WIDTH,
    WG_ARTIC_LABIODENTAL,
    WG_ARTIC_VELUM,
    WG_ARTIC_LARYNX,
    WG_ARTIC_EPILARYNX,
    WG_ARTIC_VENTRICULAR,
    WG_ARTIC_LATERAL,
    WG_ARTIC_DORSUM
} wg_artic_key;

typedef struct {
    double jaw;          /* jaw opening at the incisors (cm) */
    double tb_x, tb_y;   /* tongue body centre (cm, head frame) */
    double tt_x, tt_y;   /* tongue tip (cm, head frame) */
    double tt_curl;      /* retroflex curl 0..1 */
    double root;         /* tongue root advance (+) / retraction (-), cm */
    double lip_ap;       /* lip aperture (cm); negative = compressed */
    double lip_prot;     /* lip protrusion (cm) */
    double lip_width;    /* lip spreading 0 (rounded) .. 1 (spread) */
    double labiodental;  /* lower lip to upper teeth 0..1 */
    double velum;        /* velopharyngeal port area (cm^2) */
    double larynx;       /* larynx height offset (cm, + raised) */
    double epilarynx;    /* epilaryngeal narrowing 0..1 (twang) */
    double ventricular;  /* ventricular-fold narrowing 0..1 */
    double lateral;      /* lateral channel opening 0..1 (for /l/) */
    double dorsum;       /* tongue dorsum depression between closures 0..1 */
} wg_artic;

#define WG_ARTIC_NEUTRAL { \
    .jaw = 0.6, .tb_x = 2.9, .tb_y = 6.85, .tt_x = 6.45, .tt_y = 7.3, \
    .tt_curl = 0.0, .root = 0.0, .lip_ap = 0.7, .lip_prot = 0.2, \
    .lip_width = 0.5, .labiodental = 0.0, .velum = -0.15, .larynx = 0.0, \
    .epilarynx = 0.0, .ventricular = 0.0, .lateral = 0.0, .dorsum = 0.0 \
}

double wg_artic_get(const wg_artic *a, int key);
void   wg_artic_set(wg_artic *a, int key, double x);
void   wg_artic_to_array(const wg_artic *a, double *out, int cap);
void   wg_artic_from_array(wg_artic *a, const double *in, int n);

/* ---- bodies ---------------------------------------------------------- */

typedef struct {
    const char *name;
    double scale;          /* overall size (1 = adult male) */
    double pharynx;        /* pharynx length factor */
    double tongue_radius;  /* cm, before scale */
    double lip_width;      /* cm, before scale */
} wg_body;

#define WG_BODY_MALE   ((wg_body){ "Adult male",   1.00, 1.00, 2.25, 4.6 })
#define WG_BODY_FEMALE ((wg_body){ "Adult female", 0.90, 0.88, 2.20, 4.4 })
#define WG_BODY_CHILD  ((wg_body){ "Child",        0.74, 0.80, 2.15, 4.0 })

/* A body or a blend of female and child over the male base. */
typedef struct {
    wg_body base;
    double female;
    double child;
} wg_body_mix;

#define WG_BODY_MIX_MALE   ((wg_body_mix){ WG_BODY_MALE,   0.0, 0.0 })
#define WG_BODY_MIX_FEMALE ((wg_body_mix){ WG_BODY_FEMALE, 1.0, 0.0 })
#define WG_BODY_MIX_CHILD  ((wg_body_mix){ WG_BODY_CHILD,  0.0, 1.0 })

/* Resolve a mix to a concrete body. The presets' factors blend
 * geometrically. */
wg_body wg_body_mix_resolve(wg_body_mix m);

#ifdef __cplusplus
}
#endif

#endif /* WUBUGLOTT_ARTIC_H */
