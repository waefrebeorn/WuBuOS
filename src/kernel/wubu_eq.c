/*
 * wubu_eq.c -- kernel-owned audio equalizer DSP coefficients routing.
 *
 * Audio EQ (equalization) shapes the frequency response. DSP engines
 * (ALSA softvol, PulseAudio/PipeWire EQ, hardware codec EQ) apply filter
 * coefficients (biquad: b0..b2, a1..a2). "Runs on everything" includes
 * correct audio shaping.
 *
 * Audio EQ:
 *   - biquad: 2nd-order IIR filter, coefficients b0,b1,b2,a1,a2
 *   - ALSA controls: hardware EQ (e.g. ALC codec, cs35l41 amp EQ)
 *   - PipeWire/Pulse: software EQ (easyeffects, pulseeffects)
 *   - DSP engine: SOF/firmware coefficient programming
 *   - loudness/DRC: dynamic range compression
 *
 * WuBuOS owns this: detect the EQ capability (ALSA hw EQ, software EQ,
 * DSP engine), route to the right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the audio-EQ frontier):
 *   - biquad coefficients: b0-b2, a1-a2 (2nd-order IIR)
 *   - ALSA controls: hardware codec EQ
 *   - PipeWire/EasyEffects: software parametric EQ
 *   - SOF DSP: firmware coefficient programming
 */
#include "wubu_eq.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_alsa_eq = 0;     /* ALSA hardware EQ */
static int  g_sw_eq = 0;       /* software EQ (pw/pulse) */
static int  g_dsp = 0;         /* DSP engine (SOF) */
static int  g_biquad = 0;      /* biquad filter support */
static int  g_loudness = 0;    /* loudness/DRC */
static char g_eq_drv[24] = "";

/* ---- W1: probe the EQ topology ---- */
void wubu_eq_probe(void)
{
    g_alsa_eq = 0; g_sw_eq = 0; g_dsp = 0; g_biquad = 0; g_loudness = 0;
    g_eq_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* ALSA hardware EQ (codec control)? */
    if (access("/proc/asound", R_OK) == 0 &&
        (access("/usr/bin/alsamixer", R_OK) == 0 ||
         access("/usr/bin/amixer", R_OK) == 0)) {
        g_alsa_eq = 1;
        strcpy(g_eq_drv, "alsa-eq");
    }
    /* Software EQ (PipeWire/Pulse)? */
    if (access("/usr/share/easyeffects", R_OK) == 0 ||
        access("/usr/share/ladspa", R_OK) == 0 ||
        access("/usr/bin/easyeffects", R_OK) == 0) {
        g_sw_eq = 1;
        if (!g_eq_drv[0]) strcpy(g_eq_drv, "pw-eq");
    }
    /* DSP engine (SOF)? */
    if (access("/sys/module/snd_sof", R_OK) == 0 ||
        access("/sys/module/snd_sof_intel_hda_common", R_OK) == 0) {
        g_dsp = 1;
        if (!g_eq_drv[0]) strcpy(g_eq_drv, "sof-dsp");
    }
    /* Biquad filter support (ladspa/filter)? */
    if (access("/usr/share/ladspa", R_OK) == 0) {
        g_biquad = 1;
    }
    /* Loudness/DRC? */
    if (access("/usr/share/ladspa", R_OK) == 0 &&
        access("/usr/lib/ladspa", R_OK) == 0) {
        g_loudness = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_eq_alsa(void)      { return g_alsa_eq; }
int  wubu_eq_software(void)  { return g_sw_eq; }
int  wubu_eq_dsp(void)       { return g_dsp; }
int  wubu_eq_biquad(void)    { return g_biquad; }
int  wubu_eq_loudness(void)  { return g_loudness; }
const char *wubu_eq_driver(void){ return g_eq_drv[0] ? g_eq_drv : NULL; }

/* ---- W3: EQ driver routing ---- */
const char *wubu_eq_driver_for(const char *eq)
{
    if (!eq) return NULL;
    if (strstr(eq, "sof") || strstr(eq, "dsp")) return "sof-dsp";
    if (strstr(eq, "pw") || strstr(eq, "pipewire") || strstr(eq, "easy"))
        return "pw-eq";
    if (strstr(eq, "pulse"))  return "pulse-eq";
    if (strstr(eq, "alsa"))   return "alsa-eq";
    if (strstr(eq, "loud"))   return "loudness-drc";
    return "eq-core";
}

/* ---- W4: summary ---- */
int wubu_eq_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "eq[alsa=%d sw=%d dsp=%d biquad=%d loudness=%d drv=%s]",
        g_alsa_eq, g_sw_eq, g_dsp, g_biquad, g_loudness,
        wubu_eq_driver() ? wubu_eq_driver() : "none");
}
