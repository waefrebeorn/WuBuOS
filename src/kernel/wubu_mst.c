/*
 * wubu_mst.c -- kernel-owned DisplayPort MST + audio SRC routing.
 *
 * Two capabilities:
 *   - DisplayPort MST (Multi-Stream Transport): one DP cable carries
 *     multiple display streams (daisy-chain, DP hubs).
 *   - Audio SRC (sample-rate conversion): resample audio between device
 *     rates (44.1/48/96/192kHz) in the graph.
 *
 * DP MST:
 *   - drm dp_mst: the DP MST core (topology manager)
 *   - MST topology: branches (hubs), payloads (VC payload allocation)
 *   - /sys/class/drm/card*-DP-*: connectors; topology: /sys/kernel/debug
 *   - DP 1.2+ MST, DSC over MST
 *
 * Audio SRC:
 *   - PipeWire/Pulse resample: SRC between device + graph rates
 *   - ALSA dmix rate conversion, SRC plugin
 *   - quality: SRC_SINC_BEST_QUALITY (PipeWire default)
 *
 * WuBuOS owns this: detect DP MST topology + audio SRC, route to the
 * right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the MST/SRC frontier):
 *   - drm dp_mst: topology + payload allocation
 *   - DP 1.2 MST, DSC over MST
 *   - PipeWire SRC: resample quality
 *   - ALSA dmix/SRC: rate conversion
 */
#include "wubu_mst.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_mst = 0;         /* DP MST core */
static int  g_mst_top = 0;     /* MST topology present */
static int  g_dsc = 0;         /* DSC over MST */
static int  g_src = 0;         /* audio SRC */
static int  g_resample = 0;    /* resample quality */
static char g_mst_drv[24] = "";

/* ---- W1: probe the MST/SRC topology ---- */
void wubu_mst_probe(void)
{
    g_mst = 0; g_mst_top = 0; g_dsc = 0; g_src = 0; g_resample = 0;
    g_mst_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* DP MST core (drm_kms_helper)? */
    if (access("/sys/module/drm_kms_helper", R_OK) == 0 ||
        access("/sys/module/drm", R_OK) == 0) {
        g_mst = 1;
        strcpy(g_mst_drv, "dp-mst");
    }
    /* MST topology (DP connectors)? */
    if (access("/sys/class/drm", R_OK) == 0) {
        DIR *d = opendir("/sys/class/drm");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (strstr(e->d_name, "DP")) { g_mst_top = 1; break; }
            }
            closedir(d);
        }
    }
    /* DSC (DP 1.4 compression)? */
    if (access("/sys/module/i915", R_OK) == 0 ||
        access("/sys/module/amdgpu", R_OK) == 0 ||
        access("/sys/module/xe", R_OK) == 0) {
        g_dsc = 1;
    }
    /* Audio SRC (PipeWire resample)? */
    if (access("/usr/share/pipewire", R_OK) == 0 ||
        access("/usr/lib/pipewire", R_OK) == 0) {
        g_src = 1; g_resample = 1;
        if (!g_mst_drv[0]) strcpy(g_mst_drv, "pw-src");
    }
    /* ALSA SRC plugin? */
    if (access("/usr/share/alsa", R_OK) == 0) {
        g_src = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_mst_dp(void)      { return g_mst; }
int  wubu_mst_topology(void){ return g_mst_top; }
int  wubu_mst_dsc(void)     { return g_dsc; }
int  wubu_mst_src(void)     { return g_src; }
int  wubu_mst_resample(void){ return g_resample; }
const char *wubu_mst_driver(void){ return g_mst_drv[0] ? g_mst_drv : NULL; }

/* ---- W3: MST/SRC routing ---- */
const char *wubu_mst_payload_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "single"))  return "single-stream";
    if (strstr(mode, "multi") || strstr(mode, "mst")) return "multi-stream";
    if (strstr(mode, "dsc"))     return "dsc-compressed";
    return "dp";
}

const char *wubu_mst_src_for(const char *rate)
{
    if (!rate) return NULL;
    if (strstr(rate, "44"))   return "44100-src";
    if (strstr(rate, "48"))   return "48000-src";
    if (strstr(rate, "96"))   return "96000-src";
    if (strstr(rate, "192"))  return "192000-src";
    if (strstr(rate, "best")) return "src-best-quality";
    return "src";
}

/* ---- W4: summary ---- */
int wubu_mst_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "mst[dp=%d top=%d dsc=%d src=%d resample=%d drv=%s]",
        g_mst, g_mst_top, g_dsc, g_src, g_resample,
        wubu_mst_driver() ? wubu_mst_driver() : "none");
}
