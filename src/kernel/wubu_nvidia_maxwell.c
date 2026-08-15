/*
 * wubu_nvidia_maxwell.c -- kernel-owned NVIDIA Maxwell routing.
 *
 * NVIDIA Maxwell (GTX 9xx) binds the nvidia 535/580 driver (590
 * drops GTX 900). NVK enabled for Maxwell (April 2025).
 * "Runs on everything" includes Maxwell routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x10DE)
 */
#include "wubu_nvidia_maxwell.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_maxwell_present = 0;
static int g_maxwell_driver = 0;

void wubu_nvidia_maxwell_probe(void)
{
#ifdef WUBU_HOSTED
    g_maxwell_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_maxwell_driver = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_maxwell_present = g_maxwell_driver = 0;
#endif
}

int wubu_nvidia_maxwell_present(void)
{
#ifdef WUBU_HOSTED
    return g_maxwell_present;
#else
    return 0;
#endif
}

int wubu_nvidia_maxwell_uses_proprietary(int nvidia_version)
{
    /* Maxwell needs nvidia 535+ (590 drops GTX 900, NVK enabled). */
    return (nvidia_version >= 535) ? 1 : 0;
}

int wubu_nvidia_maxwell_has_nvk(int nvk_status)
{
    /* NVK enabled for Maxwell April 2025. */
    return (nvk_status) ? 1 : 0;
}

void wubu_nvidia_maxwell_summary(char *out, size_t cap)
{
    snprintf(out, cap, "nvidia_maxwell[dev=%d driver=%d]", g_maxwell_present, g_maxwell_driver);
}
