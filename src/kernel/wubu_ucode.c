/*
 * wubu_ucode.c -- kernel-owned CPU microcode loading routing.
 *
 * Microcode is CPU firmware loaded at boot (early) or runtime to patch
 * silicon bugs + security. "Runs on everything" includes correct
 * microcode on every CPU.
 *
 * Microcode:
 *   - intel: intel-ucode, early load from initrd, /sys/devices/system/cpu
 *   - amd: amd-ucode, early load from initrd
 *   - /dev/cpu/microcode: runtime reload (late load)
 *   - /sys/devices/system/cpu/microcode: version + revision
 *
 * WuBuOS owns this: detect the CPU vendor + microcode path, route to the
 * right loader, and expose the microcode version/revision.
 *
 * Research (Kevin-Bacon 7-hop on the microcode frontier):
 *   - intel-ucode: early initrd + late /dev/cpu/microcode load
 *   - amd-ucode: early initrd + late load
 *   - x86 microcode: arch/x86/microcode.c
 */
#include "wubu_ucode.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_intel = 0;
static int  g_amd = 0;
static int  g_early = 0;
static int  g_late = 0;        /* /dev/cpu/microcode */
static int  g_loaded = 0;      /* microcode revision readable */
static char g_ucode_drv[24] = "";

/* ---- W1: probe the microcode topology ---- */
void wubu_ucode_probe(void)
{
    g_intel = 0; g_amd = 0; g_early = 0; g_late = 0; g_loaded = 0;
    g_ucode_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* CPU vendor? */
    if (access("/sys/devices/system/cpu/vulnerabilities", R_OK) == 0) {
        FILE *f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "GenuineIntel")) { g_intel = 1; break; }
                if (strstr(line, "AuthenticAMD")) { g_amd = 1; break; }
            }
            fclose(f);
        }
    }
    if (g_intel) strcpy(g_ucode_drv, "intel-ucode");
    else if (g_amd) strcpy(g_ucode_drv, "amd-ucode");

    /* microcode version readable (loaded)? */
    if (access("/sys/devices/system/cpu/microcode", R_OK) == 0) {
        g_loaded = 1;
    }
    /* late load path? */
    if (access("/dev/cpu/microcode", R_OK) == 0) {
        g_late = 1;
    }
    /* early load (initrd has microcode blob)? */
    if (access("/usr/lib/firmware/intel-ucode", R_OK) == 0 ||
        access("/usr/lib/firmware/amd-ucode", R_OK) == 0) {
        g_early = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_ucode_intel(void)  { return g_intel; }
int  wubu_ucode_amd(void)    { return g_amd; }
int  wubu_ucode_early(void)  { return g_early; }
int  wubu_ucode_late(void)   { return g_late; }
int  wubu_ucode_loaded(void) { return g_loaded; }
const char *wubu_ucode_driver(void){ return g_ucode_drv[0] ? g_ucode_drv : NULL; }

/* ---- W3: microcode loader routing ---- */
const char *wubu_ucode_loader_for(const char *cpu)
{
    if (!cpu) return NULL;
    if (strstr(cpu, "intel")) return "intel-ucode";
    if (strstr(cpu, "amd"))   return "amd-ucode";
    if (strstr(cpu, "hygon")) return "amd-ucode";
    return "ucode";
}

const char *wubu_ucode_loadpath_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "early")) return "initrd-early";
    if (strstr(mode, "late"))  return "dev-cpu-microcode";
    return "ucode";
}

/* ---- W4: summary ---- */
int wubu_ucode_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "ucode[intel=%d amd=%d early=%d late=%d loaded=%d drv=%s]",
        g_intel, g_amd, g_early, g_late, g_loaded,
        wubu_ucode_driver() ? wubu_ucode_driver() : "none");
}
