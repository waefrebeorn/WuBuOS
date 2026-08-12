/*
 * wubu_gpuband.c -- kernel-owned GPU scheduler priority bands routing.
 *
 * The GPU scheduler uses priority bands (high/normal/low/critical) to
 * order command submission. "Runs on everything" includes fair GPU.
 *
 * GPU scheduler bands:
 *   - DRM sched: drm_sched_entity, priority levels (high/med/low)
 *   - amdgpu: amdgpu_sched, priority bands (HILE, NORMAL, LOW)
 *   - i915: i915 request priority (HIGH, NORMAL, LOW, KERNEL)
 *   - nvkm: nouveau scheduler priority
 *   - /sys/class/drm card sched entity stats
 *
 * Research (7-hop on the GPU-band frontier):
 *   - drm_sched_entity priority bands
 *   - amdgpu_sched HILE/NORMAL/LOW
 *   - i915 request priority
 */
#include "wubu_gpuband.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_band = 0;        /* priority bands */
static int  g_fair = 0;        /* fair scheduling */
static int  g_prio = 0;        /* priority levels */
static int  g_entity = 0;      /* sched entities */
static int  g_stats = 0;       /* scheduler stats */
static char g_band_drv[24] = "";

void wubu_gpuband_probe(void)
{
    g_band = 0; g_fair = 0; g_prio = 0; g_entity = 0; g_stats = 0;
    g_band_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/class/drm", R_OK) == 0) {
        g_band = 1; g_fair = 1; g_prio = 1; g_entity = 1; g_stats = 1;
        strcpy(g_band_drv, "drm-sched");
    }
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_band = 1; g_prio = 1;
        strcpy(g_band_drv, "amdgpu-band");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_band = 1; g_prio = 1;
        if (!g_band_drv[0]) strcpy(g_band_drv, "i915-prio");
    }
    if (access("/sys/module/nouveau", R_OK) == 0) {
        if (!g_band_drv[0]) strcpy(g_band_drv, "nouveau-band");
    }
#endif
}

int  wubu_gpuband_present(void){ return g_band; }
int  wubu_gpuband_fair(void)    { return g_fair; }
int  wubu_gpuband_prio(void)    { return g_prio; }
int  wubu_gpuband_entity(void)  { return g_entity; }
int  wubu_gpuband_stats(void)   { return g_stats; }
const char *wubu_gpuband_driver(void){ return g_band_drv[0] ? g_band_drv : NULL; }

const char *wubu_gpuband_prio_for(const char *p)
{
    if (!p) return NULL;
    if (strstr(p, "high") || strstr(p, "critical")) return "high";
    if (strstr(p, "low"))   return "low";
    if (strstr(p, "kernel")) return "kernel";
    return "normal";
}

const char *wubu_gpuband_class_for(const char *c)
{
    if (!c) return NULL;
    if (strstr(c, "3d"))    return "3d";
    if (strstr(c, "compute")) return "compute";
    if (strstr(c, "video")) return "video";
    if (strstr(c, "copy"))  return "copy";
    return "default";
}

int wubu_gpuband_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "gpuband[band=%d fair=%d prio=%d entity=%d stats=%d drv=%s]",
        g_band, g_fair, g_prio, g_entity, g_stats,
        wubu_gpuband_driver() ? wubu_gpuband_driver() : "none");
}
