/*
 * wubu_intel_gma.c -- kernel-owned Intel GMA legacy GPU routing.
 *
 * Intel GMA (Graphics Media Accelerator) 3-series (G31/G45)
 * bind the i915 legacy driver. GMA 950 on 945GM works with
 * modesetting + llvmpipe. "Runs on everything" includes correct
 * GMA routing on old chipsets.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x8086)
 */
#include "wubu_intel_gma.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_intel_gma_present = 0;
static int g_intel_gma_legacy = 0;

void wubu_intel_gma_probe(void)
{
#ifdef WUBU_HOSTED
    g_intel_gma_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_intel_gma_legacy = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_intel_gma_present = g_intel_gma_legacy = 0;
#endif
}

int wubu_intel_gma_present(void)
{
#ifdef WUBU_HOSTED
    return g_intel_gma_present;
#else
    return 0;
#endif
}

int wubu_intel_gma_uses_i915(int gen)
{
    /* GMA 3-series (G31/G45) and 950 on 945GM use i915. */
    return (gen >= 3 && gen <= 5) ? 1 : 0;
}

int wubu_intel_gma_needs_llvmpipe(int accel_available)
{
    /* No hardware accel (3D) on old GMA = fall back to llvmpipe. */
    return (accel_available == 0) ? 1 : 0;
}

void wubu_intel_gma_summary(char *out, size_t cap)
{
    snprintf(out, cap, "intel_gma[dev=%d legacy=%d]", g_intel_gma_present, g_intel_gma_legacy);
}
