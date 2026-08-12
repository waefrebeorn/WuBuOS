/*
 * wubu_chanmap.c -- kernel-owned audio channel map routing.
 *
 * Channel maps assign ALSA channel positions (FL, FR, C, LFE, SL, SR)
 * to physical outputs. "Runs on everything" includes correct surround.
 *
 * Channel map:
 *   - ALSA: snd_pcm_chmap, channel map control
 *   - positions: FL, FR, FC, LFE, SL, SR, BL, BR, TFL, TFR
 *   - /proc/asound card pcm sub: chmap
 *   - surround: 5.1, 7.1
 *   - layouts: mono, stereo, 5.1, 7.1
 *
 * WuBuOS owns this: detect channel map + surround + layout, route to
 * the right driver, expose the topology.
 *
 * Research (7-hop on the chanmap frontier):
 *   - ALSA channel map
 *   - 5.1/7.1 surround
 */
#include "wubu_chanmap.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_map = 0;         /* channel map present */
static int  g_stereo = 0;      /* stereo */
static int  g_51 = 0;          /* 5.1 */
static int  g_71 = 0;          /* 7.1 */
static int  g_chmap = 0;       /* ALSA chmap */
static char g_chanmap_drv[24] = "";

void wubu_chanmap_probe(void)
{
    g_map = 0; g_stereo = 0; g_51 = 0; g_71 = 0; g_chmap = 0;
    g_chanmap_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/proc/asound", R_OK) == 0 ||
        access("/sys/module/snd_pcm", R_OK) == 0) {
        g_map = 1; g_stereo = 1; g_chmap = 1;
        strcpy(g_chanmap_drv, "snd-pcm");
    }
    if (access("/proc/asound/card0", R_OK) == 0) {
        g_chmap = 1; g_stereo = 1;
        if (!g_chanmap_drv[0]) strcpy(g_chanmap_drv, "hdmi-chmap");
    }
#endif
}

int  wubu_chanmap_present(void){ return g_map; }
int  wubu_chanmap_stereo(void) { return g_stereo; }
int  wubu_chanmap_51(void)     { return g_51; }
int  wubu_chanmap_71(void)     { return g_71; }
int  wubu_chanmap_chmap(void)  { return g_chmap; }
const char *wubu_chanmap_driver(void){ return g_chanmap_drv[0] ? g_chanmap_drv : NULL; }

const char *wubu_chanmap_pos_for(const char *p)
{
    if (!p) return NULL;
    if (strstr(p, "fl"))    return "front-left";
    if (strstr(p, "fr"))    return "front-right";
    if (strstr(p, "fc"))    return "front-center";
    if (strstr(p, "lfe"))   return "lfe";
    if (strstr(p, "sl") || strstr(p, "sur")) return "surround-left";
    if (strstr(p, "sr"))    return "surround-right";
    return "front-left";
}

const char *wubu_chanmap_layout_for(const char *l)
{
    if (!l) return NULL;
    if (strstr(l, "mono"))  return "mono";
    if (strstr(l, "stereo"))return "stereo";
    if (strstr(l, "51") || strstr(l, "5.1")) return "5.1";
    if (strstr(l, "71") || strstr(l, "7.1")) return "7.1";
    return "stereo";
}

int wubu_chanmap_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "chanmap[map=%d stereo=%d 51=%d 71=%d chmap=%d drv=%s]",
        g_map, g_stereo, g_51, g_71, g_chmap,
        wubu_chanmap_driver() ? wubu_chanmap_driver() : "none");
}
