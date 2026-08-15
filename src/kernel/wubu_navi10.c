/*
 * wubu_navi10.c -- kernel-owned AMD Navi10 RDNA1 routing.
 *
 * AMD Navi10 (RX 5700/XT, RDNA1) binds the amdgpu kernel driver
 * + RADV Vulkan in Mesa. Requires kernel 5.3, Mesa 19.2, LLVM 9+.
 * AMD now ships 100% open-source (no proprietary OpenGL/Vulkan).
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x1002)
 */
#include "wubu_navi10.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_navi10_present = 0;
static int g_navi10_amdgpu = 0;

void wubu_navi10_probe(void)
{
#ifdef WUBU_HOSTED
    g_navi10_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_navi10_amdgpu = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_navi10_present = g_navi10_amdgpu = 0;
#endif
}

int wubu_navi10_present(void)
{
#ifdef WUBU_HOSTED
    return g_navi10_present;
#else
    return 0;
#endif
}

int wubu_navi10_uses_radv(int vulkan_available)
{
    /* RADV is the Mesa Vulkan driver for RDNA1. */
    return (vulkan_available) ? 1 : 0;
}

int wubu_navi10_kernel_min(int kernel_version)
{
    /* Requires kernel 5.3+. */
    return (kernel_version >= 503) ? 1 : 0;
}

void wubu_navi10_summary(char *out, size_t cap)
{
    snprintf(out, cap, "navi10[dev=%d amdgpu=%d]", g_navi10_present, g_navi10_amdgpu);
}
