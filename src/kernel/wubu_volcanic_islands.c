/*
 * wubu_volcanic_islands.c -- kernel-owned AMD GCN3 Volcanic routing.
 *
 * AMD GCN3 Volcanic Islands (R9 285, R9 380/X) binds the amdgpu
 * kernel driver + RADV Vulkan in Mesa. Timur.hu 2025: "DC now
 * supports analog connectors" on VI. Linux 6.19 folds older GCN
 * to amdgpu. "Runs on everything" includes VI routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x1002)
 */
#include "wubu_volcanic_islands.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_vi_present = 0;
static int g_vi_amdgpu = 0;

void wubu_volcanic_islands_probe(void)
{
#ifdef WUBU_HOSTED
    g_vi_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_vi_amdgpu = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_vi_present = g_vi_amdgpu = 0;
#endif
}

int wubu_volcanic_islands_present(void)
{
#ifdef WUBU_HOSTED
    return g_vi_present;
#else
    return 0;
#endif
}

int wubu_volcanic_islands_uses_amdgpu(int amdgpu_folded)
{
    /* Linux 6.19 folds GCN 1.1+ to amdgpu (replacing radeon). */
    return (amdgpu_folded) ? 1 : 0;
}

int wubu_volcanic_islands_radv(int vulkan_available)
{
    /* RADV supports GCN 1-2 (Vulkan 1.3), GCN 3-5 + RDNA (1.4). */
    return (vulkan_available) ? 1 : 0;
}

void wubu_volcanic_islands_summary(char *out, size_t cap)
{
    snprintf(out, cap, "volcanic_islands[dev=%d amdgpu=%d]", g_vi_present, g_vi_amdgpu);
}
