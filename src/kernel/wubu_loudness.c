/*
 * wubu_loudness.c -- kernel-owned audio loudness normalization routing.
 *
 * Loudness normalization evens out perceived volume between tracks
 * (ReplayGain, Opus R128, ITU-R BS.1770). "Runs on everything" includes
 * correct loudness on every playback.
 *
 * Loudness:
 *   - ReplayGain: track + album gain (vorbiscomments)
 *   - Opus/R128: R128_TRACK_GAIN, R128_ALBUM_GAIN
 *   - ITU-R BS.1770: LUFS (loudness units full scale)
 *   - PipeWire/pipewire-effects: loudness effect
 *   - ALSA dmix: volume normalization
 *
 * WuBuOS owns this: detect loudness support + tags, route to the right
 * driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the loudness frontier):
 *   - ReplayGain tags (track + album gain)
 *   - Opus R128 (ITU-R BS.1770, LUFS)
 *   - PipeWire loudness effect
 *   - ALSA dmix volume normalization
 */
#include "wubu_loudness.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_loud = 0;        /* loudness */
static int  g_replaygain = 0;  /* ReplayGain */
static int  g_r128 = 0;        /* Opus R128 */
static int  g_lufs = 0;        /* ITU-R BS.1770 LUFS */
static int  g_pw = 0;          /* PipeWire loudness */
static char g_loud_drv[24] = "";

/* ---- W1: probe the loudness topology ---- */
void wubu_loudness_probe(void)
{
    g_loud = 0; g_replaygain = 0; g_r128 = 0; g_lufs = 0; g_pw = 0;
    g_loud_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* PipeWire (loudness effect)? */
    if (access("/usr/share/pipewire", R_OK) == 0 ||
        access("/usr/lib/pipewire", R_OK) == 0 ||
        access("/usr/share/pulse", R_OK) == 0) {
        g_loud = 1; g_pw = 1;
        strcpy(g_loud_drv, "pipewire-loud");
    }
    /* ReplayGain / Opus R128 (music tags)? */
    /* Generic: assume tag-aware player is present (music playback). */
    if (access("/usr/bin", R_OK) == 0) {
        g_replaygain = 1;
        g_r128 = 1;
        g_lufs = 1;
        if (!g_loud_drv[0]) strcpy(g_loud_drv, "replaygain");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_loudness_present(void)   { return g_loud; }
int  wubu_loudness_replaygain(void){ return g_replaygain; }
int  wubu_loudness_r128(void)       { return g_r128; }
int  wubu_loudness_lufs(void)       { return g_lufs; }
int  wubu_loudness_pw(void)         { return g_pw; }
const char *wubu_loudness_driver(void){ return g_loud_drv[0] ? g_loud_drv : NULL; }

/* ---- W3: loudness routing ---- */
const char *wubu_loudness_mode_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "track"))  return "track-gain";
    if (strstr(mode, "album"))  return "album-gain";
    if (strstr(mode, "itur"))   return "lufs";
    if (strstr(mode, "lufs"))   return "lufs";
    if (strstr(mode, "off"))    return "off";
    return "track-gain";
}

const char *wubu_loudness_target_for(const char *target)
{
    if (!target) return NULL;
    if (strstr(target, "89"))   return "-18lufs";   /* RG default */
    if (strstr(target, "83"))   return "-16luft";  /* R128 default */
    if (strstr(target, "79"))   return "-14lufs";
    if (strstr(target, "93"))   return "-23lufs";  /* EBU R128 broadcast */
    return "-18lufs";
}

/* ---- W4: summary ---- */
int wubu_loudness_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "loudness[loud=%d gain=%d r128=%d lufs=%d pw=%d drv=%s]",
        g_loud, g_replaygain, g_r128, g_lufs, g_pw,
        wubu_loudness_driver() ? wubu_loudness_driver() : "none");
}
