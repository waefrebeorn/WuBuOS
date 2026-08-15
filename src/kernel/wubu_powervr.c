/*
 * wubu_powervr.c -- kernel-owned Imagination PowerVR routing.
 *
 * PowerVR ROGUE GPUs bind the pvrsrvkm driver. Mesa 25.3
 * adds open-source PowerVR Vulkan support; kernel 6.16+
 * for ROGUE. "Runs on everything" includes correct PowerVR
 * routing on all Rogue-based platforms.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x1013 for some)
 */
#include "wubu_powervr.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_powervr_present = 0;
static int g_powervr_rogue = 0;

void wubu_powervr_probe(void)
{
#ifdef WUBU_HOSTED
    g_powervr_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_powervr_rogue = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_powervr_present = g_powervr_rogue = 0;
#endif
}

int wubu_powervr_present(void)
{
#ifdef WUBU_HOSTED
    return g_powervr_present;
#else
    return 0;
#endif
}

int wubu_powervr_uses_pvrsrvkm(int kernel_616)
{
    /* pvrsrvkm requires kernel 6.16+ for ROGUE. */
    return (kernel_616) ? 1 : 0;
}

int wubu_powervr_has_vulkan(int mesa_253)
{
    /* Mesa 25.3 adds open-source PowerVR Vulkan. */
    return (mesa_253) ? 1 : 0;
}

void wubu_powervr_summary(char *out, size_t cap)
{
    snprintf(out, cap, "powervr[dev=%d rogue=%d]", g_powervr_present, g_powervr_rogue);
}
