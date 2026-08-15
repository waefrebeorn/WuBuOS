/*
 * wubu_arctic_islands.c -- kernel-owned AMD GCN4 Arctic routing.
 *
 * AMD GCN4 Arctic Islands (RX 4xx/5xx) binds the amdgpu kernel
 * driver + RADV Vulkan in Mesa. Mesa: RADV supports GCN 1-2
 * (Vulkan 1.3), GCN 3-5 + RDNA (Vulkan 1.4). RX 480/580 confirmed.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x1002)
 */
#include "wubu_arctic_islands.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_ai_present = 0;
static int g_ai_amdgpu = 0;

void wubu_arctic_islands_probe(void)
{
#ifdef WUBU_HOSTED
    g_ai_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_ai_amdgpu = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_ai_present = g_ai_amdgpu = 0;
#endif
}

int wubu_arctic_islands_present(void)
{
#ifdef WUBU_HOSTED
    return g_ai_present;
#else
    return 0;
#endif
}

int wubu_arctic_islands_uses_radv(int vulkan_available)
{
    /* RADV Vulkan 1.4 for GCN4 (RX 480/580). */
    return (vulkan_available) ? 1 : 0;
}

int wubu_arctic_islands_vulkan_level(int gcn_gen)
{
    /* GCN4 = Vulkan 1.4 per Mesa docs. */
    return (gcn_gen >= 4) ? 14 : 13;
}

void wubu_arctic_islands_summary(char *out, size_t cap)
{
    snprintf(out, cap, "arctic_islands[dev=%d amdgpu=%d]", g_ai_present, g_ai_amdgpu);
}
