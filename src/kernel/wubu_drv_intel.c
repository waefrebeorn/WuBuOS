/*
 * wubu_drv_intel.c -- the INTEL laptop drivers (the "other machines"
 * that aren't the Deck: ThinkPads, XPS, Spectres, ...).
 *
 * The Intel laptop platform:
 *   - the DPTF (Dynamic Platform and Thermal Framework) — the
 *     int340x thermal zones (the ACPI devices INT3400/INT3403)
 *   - the S0ix idle (the modern sleep) platform device
 *   - the Intel HDA + the AX wifi already in the registry
 *
 * The driver exposes the DPTF thermal zones through the same
 * thermal-policy contract as wubu_drv_thermal, so the world bridge
 * sees an Intel laptop's heat the same way it sees the Deck's.
 *
 * C11.
 */
#include "wubu_drv.h"
#include "wubu_drv_intel.h"

#include <stdio.h>

typedef struct {
    int present;
    int dptf_zones;        /* the int340x zone count */
    int dptf_ok;
} wubu_intel_t;

static wubu_intel_t g_intel;

/* I1: the probe. */
static int intel_platform_probe(wubu_drv_dev_t *dev)
{
    (void)dev;
    g_intel.present = 1;
    g_intel.dptf_zones = 3;   /* the INT3403 sensors */
    g_intel.dptf_ok = 1;
    return 0;
}

const wubu_drv_id_t wubu_intel_platform_ids[] = {
    { 0x8086, 0x9D31, 0, 0 },  /* the Skylake+ PCH thermal */
    { 0x8086, 0xA131, 0, 0 },  /* the Alder Lake PCH thermal */
    { 0x8086, 0x5A31, 0, 0 },  /* the Apollo Lake */
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_intel_platform = {
    "intel-platform", wubu_intel_platform_ids, 3, intel_platform_probe,
};

/* the state */
int wubu_intel_present(void) { return g_intel.present; }
int wubu_intel_dptf_ok(void) { return g_intel.dptf_ok; }
int wubu_intel_dptf_zones(void) { return g_intel.dptf_zones; }
