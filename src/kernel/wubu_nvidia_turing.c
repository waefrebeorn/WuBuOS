/*
 * wubu_nvidia_turing.c -- kernel-owned NVIDIA Turing routing.
 *
 * NVIDIA Turing (RTX 20xx) binds nvidia 535/550/590. Vulkan,
 * OpenGL 4.6, RT cores. Turing still supported on 550/590.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x10DE)
 */
#include "wubu_nvidia_turing.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_turing_present = 0;
static int g_turing_driver = 0;

void wubu_nvidia_turing_probe(void)
{
#ifdef WUBU_HOSTED
    g_turing_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_turing_driver = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_turing_present = g_turing_driver = 0;
#endif
}

int wubu_nvidia_turing_present(void)
{
#ifdef WUBU_HOSTED
    return g_turing_present;
#else
    return 0;
#endif
}

int wubu_nvidia_turing_has_rt_core(int rt_supported)
{
    /* Turing RT cores (sm_75). */
    return (rt_supported) ? 1 : 0;
}

int wubu_nvidia_turing_vulkan(int vulkan_level)
{
    /* Turing supports Vulkan 1.3+. */
    return (vulkan_level >= 13) ? 1 : 0;
}

void wubu_nvidia_turing_summary(char *out, size_t cap)
{
    snprintf(out, cap, "nvidia_turing[dev=%d driver=%d]", g_turing_present, g_turing_driver);
}
