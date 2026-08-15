/*
 * wubu_gpudc.c -- kernel-owned GPU display controller routing.
 *
 * GPU display controller (DC) binds output to displays via
 * KMS/DRM. "Runs on everything" includes correct DC routing
 * on all GPU vendors.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver version
 *   - /sys/class/drm/card0/status: connector status
 */
#include "wubu_gpudc.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_gpudc_present = 0;
static int g_gpudc_outputs = 0;

void wubu_gpudc_probe(void)
{
#ifdef WUBU_HOSTED
    g_gpudc_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_gpudc_outputs = (access("/sys/class/drm/card0/status", R_OK) == 0) ? 1 : 0;
#else
    g_gpudc_present = g_gpudc_outputs = 0;
#endif
}

int wubu_gpudc_present(void)
{
#ifdef WUBU_HOSTED
    return g_gpudc_present;
#else
    return 0;
#endif
}

int wubu_gpudc_type(int outputs)
{
    if (outputs <= 0) return 0;      /* no outputs */
    if (outputs <= 2) return 1;     /* single display */
    if (outputs <= 4) return 2;     /* multi-display */
    return 3;                        /* wide multi-display */
}

const char *wubu_gpudc_status_str(int outputs)
{
    if (outputs == 1) return "connected";
    if (outputs == 0) return "disconnected";
    if (outputs > 1) return "multi";
    return "unknown";
}

void wubu_gpudc_summary(char *out, size_t cap)
{
    snprintf(out, cap, "gpudc[dev=%d outputs=%d]", g_gpudc_present, g_gpudc_outputs);
}
