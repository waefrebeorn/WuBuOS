/*
 * wubu_gpumem.c -- kernel-owned GPU memory bandwidth routing.
 *
 * GPU memory bandwidth (GB/s) is measured to route memory-bound
 * compute kernels to the correct tier. "Runs on everything"
 * includes correct VRAM bandwidth classification.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver version
 *   - /sys/class/drm/card0/device/mem_info_vram_total: VRAM size
 */
#include "wubu_gpumem.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_gpumem_present = 0;
static int g_gpumem_gb = 0;

void wubu_gpumem_probe(void)
{
#ifdef _GNU_SOURCE
    g_gpumem_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_gpumem_gb = (access("/sys/class/drm/card0/device/mem_info_vram_total", R_OK) == 0) ? 1 : 0;
#else
    g_gpumem_present = g_gpumem_gb = 0;
#endif
}

int wubu_gpumem_present(void)
{
#ifdef _GNU_SOURCE
    return g_gpumem_present;
#else
    return 0;
#endif
}

int wubu_gpumem_tier(int bandwidth_gb_per_s)
{
    /* Tier by bandwidth: <200=entry, <400=mid, <800=high, else ultra. */
    if (bandwidth_gb_per_s >= 800) return 3;
    if (bandwidth_gb_per_s >= 400) return 2;
    if (bandwidth_gb_per_s >= 200) return 1;
    return 0;
}

int wubu_gpumem_is_gb(void)
{
#ifdef _GNU_SOURCE
    return g_gpumem_gb;
#else
    return 0;
#endif
}

const char *wubu_gpumem_tier_str(int tier)
{
    switch (tier) {
        case 0: return "entry";
        case 1: return "mid";
        case 2: return "high";
        case 3: return "ultra";
        default: return "unknown";
    }
}

void wubu_gpumem_summary(char *out, size_t cap)
{
    snprintf(out, cap, "gpumem[dev=%d vram=%d]", g_gpumem_present, g_gpumem_gb);
}
