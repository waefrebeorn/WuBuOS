/*
 * wubu_mem.c -- kernel-owned memory/ECC driver routing.
 *
 * Memory health is critical for "runs on everything" reliability. The
 * kernel must detect ECC support, route to the EDAC subsystem, and read
 * DIMM SPD (serial presence detect) for capacity/timing/rank info. On a
 * workstation/server ECC memory errors silently corrupt data if not
 * watched — WuBuOS owns the EDAC routing + error telemetry.
 *
 * Research (Kevin-Bacon 7-hop on the memory frontier):
 *   - EDAC core: drivers/edac/edac_mc.ko; drivers per memory controller
 *   - Intel: i7core_edac (Nehalem/Westmere), sb_edac (Sandy-Bridge+),
 *     skx_edac (Skylake-X), i10nm_edac, e7xxx_edac (legacy)
 *   - AMD: amd64_edac, pnd2_edac, al_mc_edac
 *   - DDR4/DDR5: "ddr5" UMC, sm_edac (CXL?), storm
 *   - SPD: eeprom via SMBus (ee1004/at24), /sys/bus/i2c or dmidecode
 *   - ECC status: /sys/devices/system/edac/mc/mc0/ce_count, ue_count
 */
#include "wubu_mem.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_edac = 0;
static int  g_ecc = 0;
static int  g_spd = 0;
static long g_ce_count = 0;   /* corrected errors */
static long g_ue_count = 0;   /* uncorrected errors */
static char g_edac_drv[32] = "";

/* ---- W1: probe memory/ECC topology ---- */
void wubu_mem_probe(void)
{
    g_edac = 0; g_ecc = 0; g_spd = 0; g_ce_count = 0; g_ue_count = 0;
    g_edac_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* EDAC present? */
    if (access("/sys/devices/system/edac", R_OK) == 0) {
        g_edac = 1;
        /* ECC = a memory controller with ce_count/ue_count. */
        if (access("/sys/devices/system/edac/mc/mc0/ce_count", R_OK) == 0) {
            g_ecc = 1;
            FILE *f = fopen("/sys/devices/system/edac/mc/mc0/ce_count", "r");
            if (f) { if (fscanf(f, "%ld", &g_ce_count) != 1) g_ce_count = 0; fclose(f); }
            f = fopen("/sys/devices/system/edac/mc/mc0/ue_count", "r");
            if (f) { if (fscanf(f, "%ld", &g_ue_count) != 1) g_ue_count = 0; fclose(f); }
        }
        strcpy(g_edac_drv, "edac_mc");
    }

    /* SPD present via SMBus eeprom (ee1004 = DDR4 SPD, at24 = generic). */
    if (access("/sys/bus/i2c/devices", R_OK) == 0) {
        g_spd = (access("/sys/bus/i2c/drivers/ee1004", R_OK) == 0) ||
                (access("/sys/bus/i2c/drivers/at24", R_OK) == 0);
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_mem_has_edac(void)  { return g_edac; }
int  wubu_mem_has_ecc(void)   { return g_ecc; }
int  wubu_mem_has_spd(void)   { return g_spd; }
long wubu_mem_ce_count(void)  { return g_ce_count; }
long wubu_mem_ue_count(void)  { return g_ue_count; }
const char *wubu_mem_edac_driver(void) { return g_edac_drv[0] ? g_edac_drv : NULL; }

/* ---- W3: EDAC driver routing per memory controller ---- */
const char *wubu_mem_edac_route(const char *controller)
{
    if (!controller) return NULL;
    if (strstr(controller, "i7") || strstr(controller, "westmere"))
        return "i7core_edac";
    if (strstr(controller, "sb_edac") || strstr(controller, "sandy"))
        return "sb_edac";
    if (strstr(controller, "skylake") || strstr(controller, "skx"))
        return "skx_edac";
    if (strstr(controller, "i10nm")) return "i10nm_edac";
    if (strstr(controller, "amd64") || strstr(controller, "zen"))
        return "amd64_edac";
    if (strstr(controller, "al_mc")) return "al_mc_edac";
    return "edac_mc";
}

/* ---- W4: summary ---- */
int wubu_mem_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "mem[edac=%d ecc=%d spd=%d ce=%ld ue=%ld drv=%s]",
        g_edac, g_ecc, g_spd, g_ce_count, g_ue_count,
        wubu_mem_edac_driver() ? wubu_mem_edac_driver() : "none");
}
