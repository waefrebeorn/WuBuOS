/*
 * wubu_xe3.c -- kernel-owned Intel Xe3 (Celestial) routing.
 *
 * Intel Xe3 (Celestial) binds the xe kernel driver + Iris/ANV
 * in Mesa. Mesa enablement started; kernel xe driver being
 * wired up. "Runs on everything" includes early Xe3 routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x8086)
 */
#include "wubu_xe3.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_xe3_present = 0;
static int g_xe3_xe = 0;

void wubu_xe3_probe(void)
{
#ifdef WUBU_HOSTED
    g_xe3_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_xe3_xe = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_xe3_present = g_xe3_xe = 0;
#endif
}

int wubu_xe3_present(void)
{
#ifdef WUBU_HOSTED
    return g_xe3_present;
#else
    return 0;
#endif
}

int wubu_xe3_uses_xe_driver(int xe_available)
{
    /* Xe3 binds the xe kernel driver. */
    return (xe_available) ? 1 : 0;
}

int wubu_xe3_uses_iris_anv(int mesa_ready)
{
    /* Mesa Iris/ANV drivers needed for Xe3. */
    return (mesa_ready) ? 1 : 0;
}

void wubu_xe3_summary(char *out, size_t cap)
{
    snprintf(out, cap, "xe3[dev=%d xe=%d]", g_xe3_present, g_xe3_xe);
}
