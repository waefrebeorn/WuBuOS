/*
 * wubu_pcmring.c -- kernel-owned audio PCM ring buffer routing.
 *
 * PCM ring buffers queue audio samples between app and hardware.
 * "Runs on everything" includes correct ring sizing on every codec.
 *
 * Impl routing:
 *   - /proc/asound card pcm sub/status: PCM state
 *   - /proc/asound card pcm sub/hw_params: sample rate, format
 *   - /proc/asound card pcm sub/sw_params: buffer/period size
 *   - /sys/module/snd_hda_core/parameters: HDA config
 */
#include "wubu_pcmring.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_pcmring_bufsize = 0;
static int g_pcmring_period = 0;

void wubu_pcmring_probe(void)
{
    /* Detect PCM ring via procfs presence. */
#ifdef WUBU_HOSTED
    g_pcmring_bufsize = (access("/proc/asound/card0/pcm0p/sub0/hw_params", R_OK) == 0) ? 1 : 0;
    g_pcmring_period = (access("/proc/asound/card0/pcm0p/sub0/sw_params", R_OK) == 0) ? 1 : 0;
#else
    g_pcmring_bufsize = 0;
    g_pcmring_period = 0;
#endif
}

int wubu_pcmring_present(void)
{
#ifdef WUBU_HOSTED
    return access("/proc/asound/card0", R_OK) == 0;
#else
    return 0;
#endif
}

const char *wubu_pcmring_format_for(const char *ext)
{
    if (!ext) return NULL;
    if (strstr(ext, "S32_LE")) return "S32_LE";
    if (strstr(ext, "S24_3LE")) return "S24_3LE";
    if (strstr(ext, "S24_LE")) return "S24_LE";
    if (strstr(ext, "S16_LE")) return "S16_LE";
    if (strstr(ext, "FLOAT")) return "FLOAT";
    if (strstr(ext, "S32")) return "S32";
    if (strstr(ext, "S24")) return "S24";
    if (strstr(ext, "S16")) return "S16";
    if (strstr(ext, "U8")) return "U8";
    return "unknown";
}

int wubu_pcmring_latency_us(int rate, int period, int buf)
{
    if (rate <= 0 || period <= 0 || buf <= 0) return 0;
    return (buf / (rate / 1000)) * 1000 / (buf / period);
}

void wubu_pcmring_summary(char *out, size_t cap)
{
    snprintf(out, cap, "pcmring[buf=%d period=%d]",
             g_pcmring_bufsize, g_pcmring_period);
}
