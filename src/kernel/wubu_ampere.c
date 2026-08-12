/*
 * wubu_ampere.c -- kernel-owned NVIDIA Ampere routing.
 *
 * NVIDIA Ampere (RTX 30xx) binds nvidia 535/550/590 driver.
 * Vulkan 1.4 (NVIDIA Developer beta). Ray tracing via Vulkan
 * RT extensions. NVIDIA Forums: "Ampere and Ada same experience."
 * "Runs on everything" includes Ampere routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x10DE)
 */
#include "wubu_ampere.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_ampere_present = 0;
static int g_ampere_driver = 0;

void wubu_ampere_probe(void)
{
#ifdef _GNU_SOURCE
    g_ampere_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_ampere_driver = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_ampere_present = g_ampere_driver = 0;
#endif
}

int wubu_ampere_present(void)
{
#ifdef _GNU_SOURCE
    return g_ampere_present;
#else
    return 0;
#endif
}

int wubu_ampere_has_raytracing(int rt_cores)
{
    /* Ampere has RT cores (2nd gen). */
    return (rt_cores) ? 1 : 0;
}

int wubu_ampere_vulkan(int vulkan_level)
{
    /* Ampere supports Vulkan 1.4 (NVIDIA Dev beta). */
    return (vulkan_level >= 14) ? 1 : 0;
}

void wubu_ampere_summary(char *out, size_t cap)
{
    snprintf(out, cap, "ampere[dev=%d driver=%d]", g_ampere_present, g_ampere_driver);
}
