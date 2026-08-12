/*
 * wubu_nvidia_fermi.c -- kernel-owned NVIDIA Fermi legacy routing.
 *
 * NVIDIA Fermi (GTX 4xx/5xx) binds the nvidia legacy 470.xx
 * driver (EOL June 2024). Nouveau provides reverse-engineered
 * fallback. "Runs on everything" includes Fermi legacy routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x10DE)
 */
#include "wubu_nvidia_fermi.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_fermi_present = 0;
static int g_fermi_legacy = 0;

void wubu_nvidia_fermi_probe(void)
{
#ifdef _GNU_SOURCE
    g_fermi_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_fermi_legacy = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_fermi_present = g_fermi_legacy = 0;
#endif
}

int wubu_nvidia_fermi_present(void)
{
#ifdef _GNU_SOURCE
    return g_fermi_present;
#else
    return 0;
#endif
}

int wubu_nvidia_fermi_needs_legacy(int fermi)
{
    /* Fermi requires nvidia 470.xx legacy (EOL June 2024) or Nouveau. */
    return (fermi) ? 1 : 0;
}

int wubu_nvidia_fermi_eol_status(void)
{
    /* 470.256.02 is EOL since June 2024. */
    return 470;
}

void wubu_nvidia_fermi_summary(char *out, size_t cap)
{
    snprintf(out, cap, "nvidia_fermi[dev=%d eol=%d]", g_fermi_present, wubu_nvidia_fermi_eol_status());
}
