/*
 * wubu_gpufwupd.c -- kernel-owned GPU firmware update routing.
 *
 * GPU firmware (GDDR VBIOS + microcode) must be version-checked
 * before driver bind to prevent ABI mismatch crashes.
 * "Runs on everything" includes firmware version routing on all GPUs.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/rom: VBIOS ROM file
 *   - /sys/class/drm/card0/device/uevent: driver version
 */
#include "wubu_gpufwupd.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_gpufwupd_present = 0;
static int g_gpufwupd_version = 0;

void wubu_gpufwupd_probe(void)
{
    /* Detect GPU firmware ROM presence. */
#ifdef _GNU_SOURCE
    g_gpufwupd_present = (access("/sys/class/drm/card0/device/rom", R_OK) == 0) ? 1 : 0;
    g_gpufwupd_version = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
#else
    g_gpufwupd_present = g_gpufwupd_version = 0;
#endif
}

int wubu_gpufwupd_present(void)
{
#ifdef _GNU_SOURCE
    return g_gpufwupd_present;
#else
    return 0;
#endif
}

int wubu_gpufwupd_match(int current, int expected)
{
    if (current < 0 || expected < 0) return 0;
    return (current == expected) ? 1 : 0;
}

const char *wubu_gpufwupd_status(int code)
{
    switch (code) {
        case 0: return "ok";
        case 1: return "stale";
        case 2: return "mismatch";
        case 3: return "flashing";
        default: return "unknown";
    }
}

void wubu_gpufwupd_summary(char *out, size_t cap)
{
    snprintf(out, cap, "gpufwupd[fw=%d ver=%d]",
             g_gpufwupd_present, g_gpufwupd_version);
}
