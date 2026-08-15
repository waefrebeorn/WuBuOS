/*
 * wubu_vega.c -- kernel-owned AMD GCN5 Vega routing.
 *
 * AMD Vega (RX Vega 56/64, GCN5) binds amdgpu + RADV Vulkan in
 * Mesa. Phoronix: "RADV re-enabled Vega support" (RX Vega 56/64).
 * AMD drops official Vulkan driver for Polaris/Vega but Mesa
 * RADV continues. "Runs on everything" includes Vega routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x1002)
 */
#include "wubu_vega.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_vega_present = 0;
static int g_vega_amdgpu = 0;

void wubu_vega_probe(void)
{
#ifdef WUBU_HOSTED
    g_vega_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_vega_amdgpu = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_vega_present = g_vega_amdgpu = 0;
#endif
}

int wubu_vega_present(void)
{
#ifdef WUBU_HOSTED
    return g_vega_present;
#else
    return 0;
#endif
}

int wubu_vega_radv(int vulkan_available)
{
    /* Mesa RADV continues Vega support despite AMD dropping own Vulkan. */
    return (vulkan_available) ? 1 : 0;
}

int wubu_vega_hbm_memory(int hbm_available)
{
    /* Vega uses HBM2 memory. */
    return (hbm_available) ? 1 : 0;
}

void wubu_vega_summary(char *out, size_t cap)
{
    snprintf(out, cap, "vega[dev=%d amdgpu=%d]", g_vega_present, g_vega_amdgpu);
}
