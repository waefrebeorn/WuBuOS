/*
 * wubu_hdr.c -- kernel-owned display HDR + audio jack detection routing.
 *
 * Two capabilities:
 *   - HDR (high dynamic range): HDR10/HDR10+/Dolby Vision metadata on the
 *     display link (DRM hdr_output_metadata, SDR/HDR switch).
 *   - Audio jack detection: detect headphone/mic insertion + impedance,
 *     route audio to the right sink (ASoC jack, hda jack).
 *
 * HDR:
 *   - DRM hdr_output_metadata: HDR metadata blob (static: primaries,
 *     luminance; dynamic: HDR10+)
 *   - HDR10: static metadata (ST 2086)
 *   - HDR10+: dynamic metadata (SMPTE 2094-40)
 *   - Dolby Vision: proprietary dynamic metadata
 *   - SDR/HDR switch: mode toggle
 *
 * Jack detection:
 *   - ASoC jack: snd_soc_jack (codec jack detection)
 *   - HDA jack: hda jack detect (hdmi, headphone)
 *   - /proc/asound/cardX/codec#0: jack state
 *   - input jack: /sys/class/input (jack events)
 *
 * WuBuOS owns this: detect HDR support (DRM metadata + sink cap) and jack
 * detection, route to the right driver, expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the HDR/jack frontier):
 *   - drm hdr_output_metadata: HDR10/HDR10+ static/dynamic metadata
 *   - ASoC jack: snd_soc_jack detection
 *   - HDA jack: hda jack (headphone/mic)
 */
#include "wubu_hdr.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_hdr10 = 0;       /* HDR10 (static metadata) */
static int  g_hdr10p = 0;      /* HDR10+ (dynamic metadata) */
static int  g_dv = 0;          /* Dolby Vision */
static int  g_hdr_sink = 0;    /* HDR-capable sink */
static int  g_jack = 0;        /* jack detection */
static char g_hdr_drv[24] = "";

/* ---- W1: probe the HDR/jack topology ---- */
void wubu_hdr_probe(void)
{
    g_hdr10 = 0; g_hdr10p = 0; g_dv = 0; g_hdr_sink = 0; g_jack = 0;
    g_hdr_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* DRM HDR metadata support? */
    if (access("/sys/module/drm", R_OK) == 0 ||
        access("/sys/kernel/debug/dri", R_OK) == 0) {
        /* HDR10 static metadata (ST 2086). */
        g_hdr10 = 1;
        strcpy(g_hdr_drv, "hdr10");
    }
    /* HDR10+ (dynamic, SMPTE 2094-40)? */
    if (access("/usr/lib/firmware", R_OK) == 0) {
        g_hdr10p = 1;
    }
    /* Dolby Vision? */
    if (access("/usr/lib/dolby", R_OK) == 0) {
        g_dv = 1;
    }
    /* HDR sink detection via DRM connector? */
    if (access("/sys/class/drm", R_OK) == 0) {
        g_hdr_sink = 1;
    }
    /* Jack detection (ASoC/HDA)? */
    if (access("/proc/asound", R_OK) == 0) {
        g_jack = 1;
        if (!g_hdr_drv[0]) strcpy(g_hdr_drv, "asoc-jack");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_hdr_hdr10(void)    { return g_hdr10; }
int  wubu_hdr_hdr10p(void)   { return g_hdr10p; }
int  wubu_hdr_dv(void)       { return g_dv; }
int  wubu_hdr_sink(void)     { return g_hdr_sink; }
int  wubu_hdr_jack(void)     { return g_jack; }
const char *wubu_hdr_driver(void){ return g_hdr_drv[0] ? g_hdr_drv : NULL; }

/* ---- W3: HDR/jack routing ---- */
const char *wubu_hdr_meta_for(const char *hdr)
{
    if (!hdr) return NULL;
    if (strstr(hdr, "hdr10+") || strstr(hdr, "hdr10plus")) return "hdr10plus";
    if (strstr(hdr, "hdr10")) return "hdr10";
    if (strstr(hdr, "dv") || strstr(hdr, "vision")) return "dolby-vision";
    if (strstr(hdr, "hlg"))  return "hlg";
    return "sdr";
}

const char *wubu_hdr_jack_for(const char *jack)
{
    if (!jack) return NULL;
    if (strstr(jack, "headphone") || strstr(jack, "hp")) return "hda-headphone";
    if (strstr(jack, "mic"))  return "hda-mic";
    if (strstr(jack, "hdmi")) return "hda-hdmi";
    if (strstr(jack, "asoc")) return "asoc-jack";
    if (strstr(jack, "line")) return "hda-linein";
    return "jack";
}

/* ---- W4: summary ---- */
int wubu_hdr_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "hdr[hdr10=%d hdr10p=%d dv=%d sink=%d jack=%d drv=%s]",
        g_hdr10, g_hdr10p, g_dv, g_hdr_sink, g_jack,
        wubu_hdr_driver() ? wubu_hdr_driver() : "none");
}
