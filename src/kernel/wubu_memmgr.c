/*
 * wubu_memmgr.c -- kernel-owned GPU memory manager routing.
 *
 * The GPU memory manager (GEM/TTM) allocates VRAM/GTT. "Runs on
 * everything" includes correct GPU memory on every device.
 *
 * Memory manager:
 *   - GEM: Graphics Execution Manager (drm GEM)
 *   - TTM: Translation Table Maps (drm TTM)
 *   - amdgpu: TTM backend + VRAM
 *   - i915: GEM + GGTT
 *   - nvidia: VRAM, GART, BAR
 *   - heap: VRAM, GTT, shared
 *
 * WuBuOS owns this: detect GEM/TTM + VRAM + heap, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the memmgr frontier):
 *   - DRM GEM/TTM
 *   - VRAM allocation
 */
#include "wubu_memmgr.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_mm = 0;          /* memory manager present */
static int  g_gem = 0;         /* GEM */
static int  g_ttm = 0;         /* TTM */
static int  g_vram = 0;        /* VRAM */
static int  g_gtt = 0;         /* GTT */
static char g_mm_drv[24] = "";

void wubu_memmgr_probe(void)
{
    g_mm = 0; g_gem = 0; g_ttm = 0; g_vram = 0; g_gtt = 0;
    g_mm_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_mm = 1; g_ttm = 1; g_vram = 1; g_gtt = 1;
        strcpy(g_mm_drv, "amdgpu-ttm");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_mm = 1; g_gem = 1; g_gtt = 1;
        if (!g_mm_drv[0]) strcpy(g_mm_drv, "i915-gem");
    }
    if (access("/sys/module/nvidia", R_OK) == 0 ||
        access("/sys/module/nvidia_drm", R_OK) == 0) {
        g_mm = 1; g_vram = 1; g_gtt = 1;
        if (!g_mm_drv[0]) strcpy(g_mm_drv, "nvidia-vram");
    }
#endif
}

int  wubu_memmgr_present(void){ return g_mm; }
int  wubu_memmgr_gem(void)    { return g_gem; }
int  wubu_memmgr_ttm(void)    { return g_ttm; }
int  wubu_memmgr_vram(void)   { return g_vram; }
int  wubu_memmgr_gtt(void)    { return g_gtt; }
const char *wubu_memmgr_driver(void){ return g_mm_drv[0] ? g_mm_drv : NULL; }

const char *wubu_memmgr_heap_for(const char *h)
{
    if (!h) return NULL;
    if (strstr(h, "vram") || strstr(h, "vram")) return "vram";
    if (strstr(h, "gtt")) return "gtt";
    if (strstr(h, "shared") || strstr(h, "shm")) return "shared";
    if (strstr(h, "stolen")) return "stolen";
    return "vram";
}

const char *wubu_memmgr_type_for(const char *t)
{
    if (!t) return NULL;
    if (strstr(t, "gem")) return "gem";
    if (strstr(t, "ttm")) return "ttm";
    if (strstr(t, "bo"))  return "bo";
    return "gem";
}

int wubu_memmgr_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "memmgr[mm=%d gem=%d ttm=%d vram=%d gtt=%d drv=%s]",
        g_mm, g_gem, g_ttm, g_vram, g_gtt,
        wubu_memmgr_driver() ? wubu_memmgr_driver() : "none");
}
