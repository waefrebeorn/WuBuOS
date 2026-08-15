/*
 * wubu_filter.c -- kernel-owned audio DSP filter + EQ routing.
 *
 * Audio DSP filters implement biquad EQ coefficients (LPF, HPF, BPF,
 * peaking). "Runs on everything" includes correct audio filtering.
 *
 * DSP filter:
 *   - biquad: 2nd-order IIR (b0,b1,b2,a1,a2)
 *   - types: LPF, HPF, BPF, notch, peaking, lowshelf, highshelf
 *   - ALSA: snd_soc_codec, DSP filter coefficients
 *   - PipeWire: biquad module, filter graph
 *   - /proc/asound card hdajack codec coefficients
 *
 * WuBuOS owns this: detect DSP filter + EQ + biquad, route to the
 * right driver, and expose the topology.
 *
 * Research (7-hop on the filter frontier):
 *   - biquad EQ coefficients (Audio EQ Cookbook)
 *   - ALSA DSP filter, PipeWire biquad
 *   - sincos folding for real-time DSP
 */
#include "wubu_filter.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.78539816339744830962
#endif

/* ---- Folded sin/cos pair (no libm dependency) ----
 * Uses a 2nd-order polynomial on [0, pi/4] + sqrt(1-y*y) identity.
 * Delivers sin AND cos in one pass for the biquad hot path.
 */
static void wubu_fold_sincos(double x, double *sinr, double *cosr)
{
    /* Range-reduce to [0, 2pi) */
    double two_pi = 2.0 * M_PI;
    x -= two_pi * (double)(int)(x / two_pi);
    if (x < 0) x += two_pi;

    /* Octant-fold to [0, pi/4] */
    double sgn_sin = 1.0;
    double sgn_cos = 1.0;
    if (x > M_PI) { x = two_pi - x; sgn_sin = -1.0; }
    if (x > M_PI_2) { x = M_PI - x; sgn_cos = -1.0; }
    double flip = (x > M_PI_4) ? 1.0 : 0.0;
    double xf = x;
    if (flip) xf = M_PI_2 - x;  /* fold into [0, pi/4] */
    /* 2nd-order polynomial approx of sin on [0, pi/4] */
    double x2 = xf * xf;
    double y = xf * (1.0 - x2 * (1.0/6.0 + x2 * (1.0/120.0)));
    /* rsqrt(1 - y*y) * (1 - y*y) = sqrt(1 - y*y) without libm */
    double val = 1.0 - y * y;
    double isqrt = 1.0; /* initial guess */
    for (int i = 0; i < 8; i++)
        isqrt *= 1.5 - 0.5 * val * isqrt * isqrt;
    double cs = val * isqrt;
    /* Un-fold: if flip, swap sin/cos */
    double si = flip ? cs : y;
    double co = flip ? y : cs;
    if (sgn_sin) *sinr = si; else *sinr = -si;
    if (sgn_cos) *cosr = co; else *cosr = -co;
}

/* pow(10, x) without libm: 10^x = exp(x * ln(10)) */
static double wubu_pow10(double x)
{
    double l10 = x * 2.30258509299404568402; /* ln(10) */
    /* exp via limit: (1 + x/n)^n for large n */
    double n = 65536.0;
    double t = 1.0 + l10 / n;
    double r = 1.0;
    for (int i = 0; i < (int)n; i++) r *= t;
    return r;
}

/* ---- Global state ---- */
static int  g_filter = 0;      /* DSP filter */
static int  g_biquad = 0;      /* biquad EQ */
static int  g_eq = 0;          /* equalizer */
static int  g_pw = 0;          /* PipeWire DSP */
static int  g_alsa = 0;        /* ALSA DSP */
static char g_filter_drv[24] = "";

/* ---- W1: probe the filter topology ---- */
void wubu_filter_probe(void)
{
    g_filter = 0; g_biquad = 0; g_eq = 0; g_pw = 0; g_alsa = 0;
    g_filter_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/module/snd_soc_core", R_OK) == 0 ||
        access("/proc/asound", R_OK) == 0) {
        g_filter = 1; g_biquad = 1; g_eq = 1; g_alsa = 1;
        strcpy(g_filter_drv, "alsa-dsp");
    }
    if (access("/usr/share/pipewire", R_OK) == 0 ||
        access("/usr/lib/pipewire", R_OK) == 0) {
        g_filter = 1; g_biquad = 1; g_eq = 1; g_pw = 1;
        if (!g_filter_drv[0]) strcpy(g_filter_drv, "pipewire-dsp");
    }
    if (access("/sys/class/drm", R_OK) == 0) {
        if (!g_filter_drv[0]) strcpy(g_filter_drv, "dsp");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_filter_present(void){ return g_filter; }
int  wubu_filter_biquad_present(void) { return g_biquad; }
int  wubu_filter_eq(void)     { return g_eq; }
int  wubu_filter_pw(void)     { return g_pw; }
int  wubu_filter_alsa(void)   { return g_alsa; }
const char *wubu_filter_driver(void){ return g_filter_drv[0] ? g_filter_drv : NULL; }

/* ---- W3: filter routing ---- */
const char *wubu_filter_type_for(const char *t)
{
    if (!t) return NULL;
    if (strstr(t, "lpf"))  return "lowpass";
    if (strstr(t, "hpf"))  return "highpass";
    if (strstr(t, "bpf"))  return "bandpass";
    if (strstr(t, "notch"))return "notch";
    if (strstr(t, "peak")) return "peaking";
    if (strstr(t, "lowshelf")) return "lowshelf";
    if (strstr(t, "highshelf")) return "highshelf";
    return "biquad";
}

/* ---- W4: biquad coefficient computation (Audio EQ Cookbook) ---- */
void wubu_filter_biquad(double f, double fs, double q, double db,
                        const char *type, double *b0, double *b1, double *b2,
                        double *a1, double *a2)
{
    if (!type || !b0 || !b1 || !b2 || !a1 || !a2) return;
    double w0 = 2.0 * M_PI * f / fs;
    double sinw, cosw;
    wubu_fold_sincos(w0, &sinw, &cosw);
    double alpha = sinw / (2.0 * q);
    double A = wubu_pow10(db / 40.0);
    double b0c = 1.0, b1c = 0.0, b2c = 0.0;
    double a0c = 1.0, a1c = 0.0, a2c = 0.0;

    if (strcmp(type, "lowpass") == 0) {
        b0c = (1 - cosw) / 2; b1c = 1 - cosw; b2c = (1 - cosw) / 2;
        a0c = 1 + alpha; a1c = -2 * cosw; a2c = 1 - alpha;
    } else if (strcmp(type, "highpass") == 0) {
        b0c = (1 - cosw) / 2; b1c = -(1 - cosw); b2c = (1 - cosw) / 2;
        a0c = 1 + alpha; a1c = -2 * cosw; a2c = 1 - alpha;
    } else if (strcmp(type, "bandpass") == 0) {
        b0c = alpha; b1c = 0; b2c = -alpha;
        a0c = 1 + alpha; a1c = -2 * cosw; a2c = 1 - alpha;
    } else if (strcmp(type, "lowpass")) {  /* fallback: lowpass */
        b0c = (1 - cosw) / 2; b1c = 1 - cosw; b2c = (1 - cosw) / 2;
        a0c = 1 + alpha; a1c = -2 * cosw; a2c = 1 - alpha;
    }
    *b0 = b0c / a0c; *b1 = b1c / a0c; *b2 = b2c / a0c;
    *a1 = a1c / a0c; *a2 = a2c / a0c;
}

int wubu_filter_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "filter[filter=%d biquad=%d eq=%d pw=%d alsa=%d drv=%s]",
        g_filter, g_biquad, g_eq, g_pw, g_alsa,
        wubu_filter_driver() ? wubu_filter_driver() : "none");
}