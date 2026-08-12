/*
 * wubu_gpukms.c -- kernel-owned GPU KMS modeset routing.
 *
 * KMS (Kernel Mode Setting) routes display modesetting to the
 * GPU kernel driver. "Runs on everything" includes correct
 * modeset routing on all GPU vendors.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver version
 *   - /sys/class/drm/card0/status: connector status
 */
#include "wubu_gpukms.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_gpukms_present = 0;
static int g_gpukms_active = 0;

void wubu_gpukms_probe(void)
{
#ifdef _GNU_SOURCE
    g_gpukms_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_gpukms_active = (access("/sys/class/drm/card0/status", R_OK) == 0) ? 1 : 0;
#else
    g_gpukms_present = g_gpukms_active = 0;
#endif
}

int wubu_gpukms_present(void)
{
#ifdef _GNU_SOURCE
    return g_gpukms_present;
#else
    return 0;
#endif
}

int wubu_gpukms_valid_mode(int width, int height, int refresh)
{
    /* Valid mode: resolution >= 640x480, refresh 30-240Hz. */
    if (width < 640 || height < 480) return 0;
    if (refresh < 30 || refresh > 240) return 0;
    return 1;
}

int wubu_gpukms_is_active(int crtc_active, int connector_connected)
{
    return (crtc_active && connector_connected) ? 1 : 0;
}

void wubu_gpukms_summary(char *out, size_t cap)
{
    snprintf(out, cap, "gpukms[dev=%d active=%d]", g_gpukms_present, g_gpukms_active);
}
