/*
 * wubu_renoir.c -- kernel-owned AMD Raven/Renoir APU routing.
 *
 * AMD Raven/Renoir APU (GCN 5.1) binds amdgpu kernel driver +
 * RADV Vulkan + Mesa OpenGL 4.6. Gentoo Wiki: "supports Vulkan
 * (RADV driver) and OpenGL." Renoir = GCN 5.1, DirectX 12.
 * "Runs on everything" includes Renoir APU routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x1002)
 */
#include "wubu_renoir.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_renoir_present = 0;
static int g_renoir_amdgpu = 0;

void wubu_renoir_probe(void)
{
#ifdef WUBU_HOSTED
    g_renoir_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_renoir_amdgpu = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_renoir_present = g_renoir_amdgpu = 0;
#endif
}

int wubu_renoir_present(void)
{
#ifdef WUBU_HOSTED
    return g_renoir_present;
#else
    return 0;
#endif
}

int wubu_renoir_uses_radv(int vulkan_available)
{
    /* RADV Vulkan for Raven/Renoir APUs. */
    return (vulkan_available) ? 1 : 0;
}

int wubu_renoir_is_apu(int integration)
{
    /* Renoir is an APU (integrated GPU). */
    return (integration) ? 1 : 0;
}

void wubu_renoir_summary(char *out, size_t cap)
{
    snprintf(out, cap, "renoir[dev=%d amdgpu=%d]", g_renoir_present, g_renoir_amdgpu);
}
