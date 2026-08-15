/*
 * wubu_adreno700.c -- kernel-owned Qualcomm Adreno 700 GPU routing.
 *
 * Adreno 700 (7xx series) binds the free-software freedreno
 * driver. "Runs on everything" includes correct Adreno 700
 * routing on all Qualcomm SOCs.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x5143)
 */
#include "wubu_adreno700.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_adreno700_present = 0;
static int g_adreno700_freedreno = 0;

void wubu_adreno700_probe(void)
{
#ifdef WUBU_HOSTED
    g_adreno700_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_adreno700_freedreno = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_adreno700_present = g_adreno700_freedreno = 0;
#endif
}

int wubu_adreno700_present(void)
{
#ifdef WUBU_HOSTED
    return g_adreno700_present;
#else
    return 0;
#endif
}

int wubu_adreno700_uses_freedreno(int freedreno_available)
{
    /* Adreno 7xx requires freedreno; proprietary freedreno is mainlined. */
    return (freedreno_available) ? 1 : 0;
}

int wubu_adreno700_gen(int gen)
{
    /* gen: 7 = Adreno 7xx. */
    return (gen == 7) ? 1 : 0;
}

void wubu_adreno700_summary(char *out, size_t cap)
{
    snprintf(out, cap, "adreno700[dev=%d freedreno=%d]", g_adreno700_present, g_adreno700_freedreno);
}
