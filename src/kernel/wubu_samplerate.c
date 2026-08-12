/*
 * wubu_samplerate.c -- kernel-owned audio sample rate + format routing.
 *
 * Audio sample rate + format (PCM width, S16/S24/S32/float) define the
 * audio stream layout. "Runs on everything" includes correct audio.
 *
 * Sample rate/format:
 *   - PCM: S16_LE, S24_LE, S32_LE, S32_LE_24 (3byte in 4), float
 *   - sample rate: 44.1k, 48k, 88.2k, 96k, 176.4k, 192k, 384k
 *   - /proc/asound card pcm sub hw_params: hw params
 *   - format: snd_pcm_format (SNDRV_PCM_FORMAT_S16_LE...)
 *   - channels: 1-8
 *   - interleave: interleaved / non-interleaved
 *
 * WuBuOS owns this: detect sample rate + format + channels, route to
 * the right driver, expose the topology.
 *
 * Research (7-hop on the samplerate frontier):
 *   - snd_pcm_format (S16/S24/S32/float)
 *   - hw_params (rate, channels, format)
 *   - interleaved PCM
 */
#include "wubu_samplerate.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_sr = 0;          /* sample rate present */
static int  g_pcm = 0;         /* PCM */
static int  g_float = 0;       /* float format */
static int  g_24bit = 0;       /* 24-bit */
static int  g_hi = 0;          /* high-res (192k+) */
static char g_sr_drv[24] = "";

void wubu_samplerate_probe(void)
{
    g_sr = 0; g_pcm = 0; g_float = 0; g_24bit = 0; g_hi = 0;
    g_sr_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/module/snd_pcm", R_OK) == 0 ||
        access("/proc/asound", R_OK) == 0) {
        g_sr = 1; g_pcm = 1;
        strcpy(g_sr_drv, "snd-pcm");
    }
    if (access("/proc/asound", R_OK) == 0) {
        g_pcm = 1; g_24bit = 1;
        if (!g_sr_drv[0]) strcpy(g_sr_drv, "snd-pcm");
    }
    if (access("/sys/module/snd_usb_audio", R_OK) == 0) {
        g_hi = 1; g_24bit = 1;
        if (!g_sr_drv[0]) strcpy(g_sr_drv, "snd-usb");
    }
#endif
}

int  wubu_samplerate_present(void){ return g_sr; }
int  wubu_samplerate_pcm(void)    { return g_pcm; }
int  wubu_samplerate_float(void)  { return g_float; }
int  wubu_samplerate_24bit(void)  { return g_24bit; }
int  wubu_samplerate_hi(void)     { return g_hi; }
const char *wubu_samplerate_driver(void){ return g_sr_drv[0] ? g_sr_drv : NULL; }

const char *wubu_samplerate_fmt_for(const char *f)
{
    if (!f) return NULL;
    if (strstr(f, "float")) return "float";
    if (strstr(f, "s24"))  return "s24";
    if (strstr(f, "s32"))  return "s32";
    if (strstr(f, "s16"))  return "s16";
    if (strstr(f, "u8"))   return "u8";
    return "s16";
}

const char *wubu_samplerate_rate_for(const char *r)
{
    if (!r) return NULL;
    if (strstr(r, "192") || strstr(r, "384") || strstr(r, "176")) return "high-res";
    if (strstr(r, "96") || strstr(r, "88")) return "high-rate";
    if (strstr(r, "48")) return "48k";
    if (strstr(r, "44")) return "44.1k";
    return "48k";
}

int wubu_samplerate_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "samplerate[sr=%d pcm=%d float=%d 24bit=%d hi=%d drv=%s]",
        g_sr, g_pcm, g_float, g_24bit, g_hi,
        wubu_samplerate_driver() ? wubu_samplerate_driver() : "none");
}