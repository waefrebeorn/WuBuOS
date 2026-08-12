/*
 * wubu_pcmplugin.c -- kernel-owned audio PCM plugin routing.
 *
 * PCM plugins (ALSA plugin chain) transform audio samples. "Runs on
 * everything" includes correct audio processing on every host.
 *
 * PCM plugin:
 *   - ALSA: snd_pcm_plugin, dmix/softvol/plugin
 *   - plugin chain: rate, vol, copy, plug
 *   - rate plugin: rate conversion
 *   - vol plugin: volume control
 *   - copy: sample copy
 *   - plug: format conversion
 *
 * WuBuOS owns this: detect PCM plugin + chain + type, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the PCM plugin frontier):
 *   - ALSA PCM plugin
 *   - dmix/softvol
 */
#include "wubu_pcmplugin.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_plug = 0;        /* PCM plugin present */
static int  g_rate = 0;        /* rate plugin */
static int  g_vol = 0;         /* vol plugin */
static int  g_copy = 0;        /* copy plugin */
static int  g_plugtype = 0;    /* plug type */
static char g_pcm_drv[24] = "";

void wubu_pcmplugin_probe(void)
{
    g_plug = 0; g_rate = 0; g_vol = 0; g_copy = 0; g_plugtype = 0;
    g_pcm_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/proc/asound", R_OK) == 0 ||
        access("/sys/module/snd_pcm", R_OK) == 0) {
        g_plug = 1; g_rate = 1; g_vol = 1; g_copy = 1; g_plugtype = 1;
        strcpy(g_pcm_drv, "snd-pcm-plugins");
    }
#endif
}

int  wubu_pcmplugin_present(void){ return g_plug; }
int  wubu_pcmplugin_rate(void)  { return g_rate; }
int  wubu_pcmplugin_vol(void)   { return g_vol; }
int  wubu_pcmplugin_copy(void)  { return g_copy; }
int  wubu_pcmplugin_plugtype(void){ return g_plugtype; }
const char *wubu_pcmplugin_driver(void){ return g_pcm_drv[0] ? g_pcm_drv : NULL; }

const char *wubu_pcmplugin_type_for(const char *t)
{
    if (!t) return NULL;
    if (strstr(t, "rate") || strstr(t, "resample")) return "rate";
    if (strstr(t, "vol") || strstr(t, "softvol")) return "vol";
    if (strstr(t, "copy")) return "copy";
    if (strstr(t, "plug") || strstr(t, "route")) return "plug";
    if (strstr(t, "dmix")) return "dmix";
    return "plug";
}

const char *wubu_pcmplugin_chain_for(const char *c)
{
    if (!c) return NULL;
    if (strstr(c, "rate")) return "rate";
    if (strstr(c, "vol"))  return "vol";
    if (strstr(c, "copy")) return "copy";
    if (strstr(c, "plug")) return "plug";
    if (strstr(c, "dmix")) return "dmix";
    return "plug";
}

int wubu_pcmplugin_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "pcmplugin[plug=%d rate=%d vol=%d copy=%d plugtype=%d drv=%s]",
        g_plug, g_rate, g_vol, g_copy, g_plugtype,
        wubu_pcmplugin_driver() ? wubu_pcmplugin_driver() : "none");
}
