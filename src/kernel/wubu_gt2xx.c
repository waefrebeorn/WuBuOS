/*
 * wubu_gt2xx.c -- kernel-owned NVIDIA GT2xx legacy routing.
 *
 * NVIDIA GT2xx (G8x/G9x) is legacy. Debian Wiki: 340.108 legacy
 * driver (EOL). Nouveau open-source driver is the modern
 * fallback. freedesktop: "Nouveau = accelerated open-source."
 * "Runs on everything" includes GT2xx fallback routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x10DE)
 */
#include "wubu_gt2xx.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_gt2xx_present = 0;
static int g_gt2xx_nouveau = 0;

void wubu_gt2xx_probe(void)
{
#ifdef _GNU_SOURCE
    g_gt2xx_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_gt2xx_nouveau = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_gt2xx_present = g_gt2xx_nouveau = 0;
#endif
}

int wubu_gt2xx_present(void)
{
#ifdef _GNU_SOURCE
    return g_gt2xx_present;
#else
    return 0;
#endif
}

int wubu_gt2xx_needs_nouveau(int legacy_eol)
{
    /* 340.108 legacy EOL; Nouveau is the open-source fallback. */
    return (legacy_eol) ? 1 : 0;
}

int wubu_gt2xx_nouveau_available(int nouveau_present)
{
    /* Nouveau kernel driver for GT2xx. */
    return (nouveau_present) ? 1 : 0;
}

void wubu_gt2xx_summary(char *out, size_t cap)
{
    snprintf(out, cap, "gt2xx[dev=%d nouveau=%d]", g_gt2xx_present, g_gt2xx_nouveau);
}
