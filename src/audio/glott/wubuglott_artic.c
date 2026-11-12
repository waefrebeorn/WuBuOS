/*
 * wubuglott_artic.c -- the articulatory model.
 * See wubuglott_artic.h. Slermed from chimera's anatomy/human.rs.
 */
#include "wubuglott_artic.h"
#include "wubuglott_math.h"

#include <math.h>
#include <string.h>

double wg_artic_get(const wg_artic *a, int key)
{
    switch (key) {
    case WG_ARTIC_JAW:         return a->jaw;
    case WG_ARTIC_TB_X:        return a->tb_x;
    case WG_ARTIC_TB_Y:        return a->tb_y;
    case WG_ARTIC_TT_X:        return a->tt_x;
    case WG_ARTIC_TT_Y:        return a->tt_y;
    case WG_ARTIC_TT_CURL:     return a->tt_curl;
    case WG_ARTIC_ROOT:        return a->root;
    case WG_ARTIC_LIP_AP:      return a->lip_ap;
    case WG_ARTIC_LIP_PROT:    return a->lip_prot;
    case WG_ARTIC_LIP_WIDTH:   return a->lip_width;
    case WG_ARTIC_LABIODENTAL: return a->labiodental;
    case WG_ARTIC_VELUM:       return a->velum;
    case WG_ARTIC_LARYNX:      return a->larynx;
    case WG_ARTIC_EPILARYNX:   return a->epilarynx;
    case WG_ARTIC_VENTRICULAR: return a->ventricular;
    case WG_ARTIC_LATERAL:     return a->lateral;
    case WG_ARTIC_DORSUM:      return a->dorsum;
    default:                   return 0.0;
    }
}

void wg_artic_set(wg_artic *a, int key, double x)
{
    switch (key) {
    case WG_ARTIC_JAW:         a->jaw = x; break;
    case WG_ARTIC_TB_X:        a->tb_x = x; break;
    case WG_ARTIC_TB_Y:        a->tb_y = x; break;
    case WG_ARTIC_TT_X:        a->tt_x = x; break;
    case WG_ARTIC_TT_Y:        a->tt_y = x; break;
    case WG_ARTIC_TT_CURL:     a->tt_curl = x; break;
    case WG_ARTIC_ROOT:        a->root = x; break;
    case WG_ARTIC_LIP_AP:      a->lip_ap = x; break;
    case WG_ARTIC_LIP_PROT:    a->lip_prot = x; break;
    case WG_ARTIC_LIP_WIDTH:   a->lip_width = x; break;
    case WG_ARTIC_LABIODENTAL: a->labiodental = x; break;
    case WG_ARTIC_VELUM:       a->velum = x; break;
    case WG_ARTIC_LARYNX:      a->larynx = x; break;
    case WG_ARTIC_EPILARYNX:   a->epilarynx = x; break;
    case WG_ARTIC_VENTRICULAR: a->ventricular = x; break;
    case WG_ARTIC_LATERAL:     a->lateral = x; break;
    case WG_ARTIC_DORSUM:      a->dorsum = x; break;
    default: break;
    }
}

void wg_artic_to_array(const wg_artic *a, double *out, int cap)
{
    for (int i = 0; i < WG_N_ARTIC && i < cap; i++)
        out[i] = wg_artic_get(a, i);
}

void wg_artic_from_array(wg_artic *a, const double *in, int n)
{
    wg_artic neutral = WG_ARTIC_NEUTRAL;
    *a = neutral;
    for (int i = 0; i < WG_N_ARTIC && i < n; i++)
        wg_artic_set(a, i, in[i]);
}

/* ---- bodies ---------------------------------------------------------- */

/* Clamp to 0..1; non-finite counts as 0. */
static double mix_clamp(double x)
{
    if (!isfinite(x)) return 0.0;
    return wg_clamp(x, 0.0, 1.0);
}

/* Weights of male, female and child; they sum to 1. */
static void mix_weights(wg_body_mix m, double *wm, double *wf, double *wc)
{
    double f = mix_clamp(m.female);
    double c = mix_clamp(m.child);
    *wm = (1.0 - c) * (1.0 - f);
    *wf = (1.0 - c) * f;
    *wc = c;
}

/* The preset this is exactly, if any (returns -1 when not). */
static int mix_preset(wg_body_mix m)
{
    double wm, wf, wc;
    mix_weights(m, &wm, &wf, &wc);
    if (wm == 1.0) return 0;   /* male */
    if (wf == 1.0) return 1;   /* female */
    if (wc == 1.0) return 2;   /* child */
    return -1;
}

/* The preset with the largest weight. */
static int mix_nearest(wg_body_mix m)
{
    double wm, wf, wc;
    mix_weights(m, &wm, &wf, &wc);
    if (wf > wm && wf > wc) return 1;
    if (wc > wm && wc > wf) return 2;
    return 0;
}

wg_body wg_body_mix_resolve(wg_body_mix m)
{
    int p = mix_preset(m);
    if (p == 0) return WG_BODY_MALE;
    if (p == 1) return WG_BODY_FEMALE;
    if (p == 2) return WG_BODY_CHILD;

    double wm, wf, wc;
    mix_weights(m, &wm, &wf, &wc);
    wg_body bm = WG_BODY_MALE, bf = WG_BODY_FEMALE, bc = WG_BODY_CHILD;

    wg_body out;
    out.name = (mix_nearest(m) == 1) ? bf.name : (mix_nearest(m) == 2) ? bc.name : bm.name;
    out.scale         = wm * bm.scale         + wf * bf.scale         + wc * bc.scale;
    out.pharynx       = wm * bm.pharynx       + wf * bf.pharynx       + wc * bc.pharynx;
    out.tongue_radius = wm * bm.tongue_radius + wf * bf.tongue_radius + wc * bc.tongue_radius;
    out.lip_width     = wm * bm.lip_width     + wf * bf.lip_width     + wc * bc.lip_width;
    return out;
}
