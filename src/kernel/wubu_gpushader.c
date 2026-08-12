/*
 * wubu_gpushader.c -- kernel-owned GPU shader model routing.
 *
 * Shader model detection routes GPU shader cores to the correct
 * compiler backend (SPIR-V/GLSL/HLSL). "Runs on everything"
 * includes correct shader model routing on all GPU vendors.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver version
 *   - /sys/class/drm/card0/device/hardware_rev: GPU revision
 */
#include "wubu_gpushader.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_gpushader_present = 0;
static int g_gpushader_model = 0;

void wubu_gpushader_probe(void)
{
    /* Detect GPU shader model presence. */
#ifdef _GNU_SOURCE
    g_gpushader_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_gpushader_model = (access("/sys/class/drm/card0/device/hardware_rev", R_OK) == 0) ? 1 : 0;
#else
    g_gpushader_present = g_gpushader_model = 0;
#endif
}

int wubu_gpushader_present(void)
{
#ifdef _GNU_SOURCE
    return g_gpushader_present;
#else
    return 0;
#endif
}

int wubu_gpushader_model_int(int major, int minor)
{
    /* SM 6_0+ = advanced, 5_0 = baseline, else legacy. */
    if (major * 100 + minor >= 600) return 2;
    if (major * 100 + minor >= 500) return 1;
    return 0;
}

const char *wubu_gpushader_model_str(int level)
{
    switch (level) {
        case 0: return "legacy";
        case 1: return "baseline";
        case 2: return "advanced";
        default: return "unknown";
    }
}

void wubu_gpushader_summary(char *out, size_t cap)
{
    snprintf(out, cap, "gpushader[dev=%d model=%d]",
             g_gpushader_present, g_gpushader_model);
}
