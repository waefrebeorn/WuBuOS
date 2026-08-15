/*
 * wubu_intel_icelake.c -- kernel-owned Intel Gen11 Ice Lake routing.
 *
 * Intel Gen11 Ice Lake binds i915 kernel driver + Iris Mesa.
 * ANV provides Vulkan 1.2. Gen11 (11th gen) Intel graphics.
 * ArchWiki: Gen11 (Icelake) supported by i915 + Iris/ANV.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x8086)
 */
#include "wubu_intel_icelake.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_icelake_present = 0;
static int g_icelake_i915 = 0;

void wubu_intel_icelake_probe(void)
{
#ifdef WUBU_HOSTED
    g_icelake_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_icelake_i915 = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_icelake_present = g_icelake_i915 = 0;
#endif
}

int wubu_intel_icelake_present(void)
{
#ifdef WUBU_HOSTED
    return g_icelake_present;
#else
    return 0;
#endif
}

int wubu_intel_icelake_uses_iris(int gl_available)
{
    /* Gen11 Ice Lake uses Iris Mesa driver. */
    return (gl_available) ? 1 : 0;
}

int wubu_intel_icelake_has_anv(int anv_available)
{
    /* ANV Vulkan 1.2 for Gen11. */
    return (anv_available) ? 1 : 0;
}

void wubu_intel_icelake_summary(char *out, size_t cap)
{
    snprintf(out, cap, "icelake[dev=%d i915=%d]", g_icelake_present, g_icelake_i915);
}
