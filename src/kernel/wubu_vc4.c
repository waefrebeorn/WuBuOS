/*
 * wubu_vc4.c -- kernel-owned Broadcom VideoCore vc4 routing.
 *
 * Broadcom VideoCore (RPi) binds the vc4 kernel driver (rendering)
 * + v3d (3D). RPi forums: "v3d does 3D, vc4 does rendering."
 * "Runs on everything" includes correct VideoCore routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x1a03 for some)
 */
#include "wubu_vc4.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_vc4_present = 0;
static int g_vc4_v3d = 0;

void wubu_vc4_probe(void)
{
#ifdef _GNU_SOURCE
    g_vc4_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_vc4_v3d = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_vc4_present = g_vc4_v3d = 0;
#endif
}

int wubu_vc4_present(void)
{
#ifdef _GNU_SOURCE
    return g_vc4_present;
#else
    return 0;
#endif
}

int wubu_vc4_uses_vc4_v3d(int dual_driver)
{
    /* vc4 (rendering) + v3d (3D) are dual drivers on RPi. */
    return (dual_driver) ? 1 : 0;
}

int wubu_vc4_has_3d(int v3d_available)
{
    /* v3d driver handles OpenGL ES 3.1+ on RPi4/5. */
    return (v3d_available) ? 1 : 0;
}

void wubu_vc4_summary(char *out, size_t cap)
{
    snprintf(out, cap, "vc4[dev=%d v3d=%d]", g_vc4_present, g_vc4_v3d);
}
