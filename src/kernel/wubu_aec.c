/*
 * wubu_aec.c -- kernel-owned audio AEC + noise suppression routing.
 *
 * AEC (acoustic echo cancellation) + NS (noise suppression) are real-time
 * audio DSP that remove echo + background noise from mic capture. "Runs
 * on everything" includes clean voice on every device.
 *
 * AEC/NS:
 *   - WebRTC APM: audio_processing module (echo, noise, gain control)
 *   - PipeWire: AEC/NS via node props + aec-method
 *   - PulseAudio: module-echo-cancel (webrtc/speex)
 *   - ALSA: dmix + dsnoop + snd_hda_codec (jack detection + AMP)
 *   - algorithms: speexDSP, WebRTC, RNNoise, ooura (FFT)
 *
 * WuBuOS owns this: detect AEC/NS engines + algorithm, route to the
 * right driver, and expose the topology.
 *
 * Research (7-hop on the AEC frontier):
 *   - WebRTC APM audio_processing
 *   - PipeWire aec-method
 *   - PulseAudio module-echo-cancel
 *   - algorithms: speexDSP, RNNoise
 */
#include "wubu_aec.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_aec = 0;         /* AEC engine */
static int  g_ns = 0;          /* noise suppression */
static int  g_webrtc = 0;      /* WebRTC APM */
static int  g_pw = 0;          /* PipeWire */
static int  g_pa = 0;          /* PulseAudio */
static char g_aec_drv[24] = "";

/* ---- W1: probe the AEC topology ---- */
void wubu_aec_probe(void)
{
    g_aec = 0; g_ns = 0; g_webrtc = 0; g_pw = 0; g_pa = 0;
    g_aec_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* PipeWire? */
    if (access("/usr/share/pipewire", R_OK) == 0 ||
        access("/usr/lib/pipewire", R_OK) == 0) {
        g_aec = 1; g_ns = 1; g_pw = 1; g_webrtc = 1;
        strcpy(g_aec_drv, "pipewire-aec");
    }
    /* PulseAudio module-echo-cancel? */
    if (access("/usr/share/pulseaudio", R_OK) == 0 ||
        access("/usr/lib/pulse", R_OK) == 0) {
        g_aec = 1; g_ns = 1; g_pa = 1;
        if (!g_aec_drv[0]) strcpy(g_aec_drv, "pulse-aec");
    }
    /* ALSA? */
    if (access("/proc/asound", R_OK) == 0) {
        g_ns = 1;
        if (!g_aec_drv[0]) strcpy(g_aec_drv, "alsa-ns");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_aec_present(void){ return g_aec; }
int  wubu_aec_ns(void)     { return g_ns; }
int  wubu_aec_webrtc(void) { return g_webrtc; }
int  wubu_aec_pw(void)     { return g_pw; }
int  wubu_aec_pa(void)     { return g_pa; }
const char *wubu_aec_driver(void){ return g_aec_drv[0] ? g_aec_drv : NULL; }

/* ---- W3: AEC routing ---- */
const char *wubu_aec_method_for(const char *method)
{
    if (!method) return NULL;
    if (strstr(method, "webrtc"))  return "webrtc";
    if (strstr(method, "speex"))   return "speex";
    if (strstr(method, "rnnoise")) return "rnnoise";
    if (strstr(method, "ooura"))   return "ooura";
    return "webrtc";
}

const char *wubu_aec_level_for(const char *level)
{
    if (!level) return NULL;
    if (strstr(level, "aggressive")) return "aggressive";
    if (strstr(level, "moderate"))   return "moderate";
    if (strstr(level, "light"))      return "light";
    return "moderate";
}

/* ---- W4: summary ---- */
int wubu_aec_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "aec[aec=%d ns=%d webrtc=%d pw=%d pa=%d drv=%s]",
        g_aec, g_ns, g_webrtc, g_pw, g_pa,
        wubu_aec_driver() ? wubu_aec_driver() : "none");
}
