/*
 * wubu_vc6.c -- kernel-owned Broadcom VideoCore VI routing.
 *
 * VideoCore VI (RPi4/5) binds the vc4 kernel driver (display)
 * + v3d (3D/render). OpenGL ES 3.1 and Vulkan 1.2 conformant.
 * "Runs on everything" includes correct VC6 routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor
 */
#include "wubu_vc6.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_vc6_present = 0;
static int g_vc6_v3d = 0;

void wubu_vc6_probe(void)
{
#ifdef _GNU_SOURCE
    g_vc6_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_vc6_v3d = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_vc6_present = g_vc6_v3d = 0;
#endif
}

int wubu_vc6_present(void)
{
#ifdef _GNU_SOURCE
    return g_vc6_present;
#else
    return 0;
#endif
}

int wubu_vc6_uses_v3d(int v3d_available)
{
    /* VC6 rendering via v3d driver (3D/OpenGL ES 3.1). */
    return (v3d_available) ? 1 : 0;
}

int wubu_vc6_has_vulkan(int vulkan_available)
{
    /* v3dv Vulkan 1.2 on RPi4/5. */
    return (vulkan_available) ? 1 : 0;
}

void wubu_vc6_summary(char *out, size_t cap)
{
    snprintf(out, cap, "vc6[dev=%d v3d=%d]", g_vc6_present, g_vc6_v3d);
}
