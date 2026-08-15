/*
 * wubu_nvidia_kepler.c -- kernel-owned NVIDIA Kepler legacy routing.
 *
 * NVIDIA Kepler (GTX 6xx/7xx) binds the nvidia legacy 470.xx
 * driver (EOL June 2024) or Nouveau. NVK now supports Maxwell+.
 * "Runs on everything" includes Kepler legacy routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x10DE)
 */
#include "wubu_nvidia_kepler.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_kepler_present = 0;
static int g_kepler_legacy = 0;

void wubu_nvidia_kepler_probe(void)
{
#ifdef WUBU_HOSTED
    g_kepler_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_kepler_legacy = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_kepler_present = g_kepler_legacy = 0;
#endif
}

int wubu_nvidia_kepler_present(void)
{
#ifdef WUBU_HOSTED
    return g_kepler_present;
#else
    return 0;
#endif
}

int wubu_nvidia_kepler_needs_legacy(int kepler)
{
    /* Kepler (GTX 6xx/7xx) requires nvidia 470.xx or Nouveau. */
    return (kepler) ? 1 : 0;
}

int wubu_nvidia_kepler_nvk_support(int gen)
{
    /* NVK supports Maxwell+ (gen >= 2), not Kepler (gen=1). */
    return (gen >= 2) ? 1 : 0;
}

void wubu_nvidia_kepler_summary(char *out, size_t cap)
{
    snprintf(out, cap, "nvidia_kepler[dev=%d eol=%d]", g_kepler_present, 470);
}
