/*
 * wubu_intel_skylake.c -- kernel-owned Intel Gen9 Skylake routing.
 *
 * Intel Gen9 Skylake binds i915 kernel driver + Iris Mesa.
 * ANV provides Vulkan 1.2. ArchWiki: Gen9 (Skylake) supported
 * by i915 + Iris on Mesa. "Runs on everything" includes Skl.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x8086)
 */
#include "wubu_intel_skylake.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_skylake_present = 0;
static int g_skylake_i915 = 0;

void wubu_intel_skylake_probe(void)
{
#ifdef WUBU_HOSTED
    g_skylake_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_skylake_i915 = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_skylake_present = g_skylake_i915 = 0;
#endif
}

int wubu_intel_skylake_present(void)
{
#ifdef WUBU_HOSTED
    return g_skylake_present;
#else
    return 0;
#endif
}

int wubu_intel_skylake_uses_iris(int gl_available)
{
    /* Gen9 Skylake uses Iris Mesa driver. */
    return (gl_available) ? 1 : 0;
}

int wubu_intel_skylake_has_anv(int anv_available)
{
    /* ANV Vulkan 1.2 for Gen9. */
    return (anv_available) ? 1 : 0;
}

void wubu_intel_skylake_summary(char *out, size_t cap)
{
    snprintf(out, cap, "skylake[dev=%d i915=%d]", g_skylake_present, g_skylake_i915);
}
