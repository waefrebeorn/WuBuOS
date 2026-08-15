/*
 * wubu_clock.c -- kernel-owned clock/thermal-topology driver routing.
 *
 * This module owns the *routing + topology* of clock (RTC chip) and
 * thermal management on the machine — which RTC driver and which thermal
 * driver the kernel binds. It complements wubu_rtc.c (the CMOS wall-clock
 * reader): that driver reads time, this one routes the RTC chip driver +
 * thermal zones.
 *
 * RTC chip drivers:
 *   - ds1307, ds3231 (Maxim), pcf8523/pcf2127 (NXP), m41t80 (ST),
 *     rx8025, rtc-cmos (PC battery RTC), rtc-efi
 *
 * Thermal drivers:
 *   - thermal zone core (thermal.ko)
 *   - x86: int340x (Intel), coretemp, acpitz
 *   - ARM: rockchip_thermal, exynos_tmu, imx_thermal
 *   - cooling devices: fan, cpufreq throttling
 *
 * WuBuOS owns this: detect the RTC chip + thermal zones, route to the
 * right driver, and expose the clock/thermal topology.
 *
 * Research (Kevin-Bacon 7-hop on the clock/thermal frontier):
 *   - rtc-core: /dev/rtc0, /sys/class/rtc
 *   - ds1307, pcf8523, rtc-cmos, ds3231 (precision RTC)
 *   - thermal: /sys/class/thermal, int340x, coretemp, rockchip_thermal
 */
#include "wubu_clock.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_rtc = 0;
static int  g_thermal = 0;
static int  g_thermal_zones = 0;
static int  g_cooling = 0;
static char g_rtc_drv[32] = "";
static char g_thermal_drv[32] = "";

/* ---- W1: probe the clock/thermal topology ---- */
void wubu_clock_probe(void)
{
    g_rtc = 0; g_thermal = 0; g_thermal_zones = 0; g_cooling = 0;
    g_rtc_drv[0] = '\0'; g_thermal_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* RTC present? */
    if (access("/dev/rtc0", R_OK) == 0 || access("/sys/class/rtc/rtc0", R_OK) == 0) {
        g_rtc = 1;
        if (access("/sys/bus/i2c/drivers/ds1307", R_OK) == 0)
            strcpy(g_rtc_drv, "ds1307");
        else if (access("/sys/bus/i2c/drivers/pcf8523", R_OK) == 0)
            strcpy(g_rtc_drv, "pcf8523");
        else if (access("/sys/bus/i2c/drivers/ds3231", R_OK) == 0)
            strcpy(g_rtc_drv, "ds3231");
        else if (access("/sys/bus/i2c/drivers/m41t80", R_OK) == 0)
            strcpy(g_rtc_drv, "m41t80");
        else if (access("/sys/bus/platform/drivers/rtc-cmos", R_OK) == 0)
            strcpy(g_rtc_drv, "rtc-cmos");
        else
            strcpy(g_rtc_drv, "rtc-core");
    }
    /* Thermal zones present? */
    if (access("/sys/class/thermal", R_OK) == 0) {
        g_thermal = 1;
        for (int i = 0; i < 32; i++) {
            char p[64];
            snprintf(p, sizeof(p), "/sys/class/thermal/thermal_zone%d", i);
            if (access(p, R_OK) == 0) g_thermal_zones++;
        }
        for (int i = 0; i < 32; i++) {
            char p[64];
            snprintf(p, sizeof(p), "/sys/class/thermal/cooling_device%d", i);
            if (access(p, R_OK) == 0) { g_cooling = 1; break; }
        }
        if (access("/sys/bus/platform/drivers/int3400_thermal", R_OK) == 0)
            strcpy(g_thermal_drv, "int340x");
        else if (access("/sys/bus/platform/drivers/coretemp", R_OK) == 0)
            strcpy(g_thermal_drv, "coretemp");
        else if (access("/sys/bus/platform/drivers/rockchip_thermal", R_OK) == 0)
            strcpy(g_thermal_drv, "rockchip_thermal");
        else
            strcpy(g_thermal_drv, "thermal-core");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_clock_has_rtc(void)     { return g_rtc; }
int  wubu_clock_has_thermal(void) { return g_thermal; }
int  wubu_clock_thermal_zones(void){ return g_thermal_zones; }
int  wubu_clock_has_cooling(void) { return g_cooling; }
const char *wubu_clock_rtc_driver(void){ return g_rtc_drv[0] ? g_rtc_drv : NULL; }
const char *wubu_clock_thermal_driver(void){ return g_thermal_drv[0] ? g_thermal_drv : NULL; }

/* ---- W3: driver routing ---- */
const char *wubu_clock_rtc_for(const char *rtc)
{
    if (!rtc) return NULL;
    if (strstr(rtc, "ds1307"))  return "ds1307";
    if (strstr(rtc, "ds3231"))  return "ds3231";
    if (strstr(rtc, "pcf8523")) return "pcf8523";
    if (strstr(rtc, "pcf2127")) return "pcf2127";
    if (strstr(rtc, "m41t80"))  return "m41t80";
    if (strstr(rtc, "rx8025"))  return "rx8025";
    if (strstr(rtc, "cmos"))    return "rtc-cmos";
    if (strstr(rtc, "efi"))     return "rtc-efi";
    return "rtc-core";
}

const char *wubu_clock_thermal_for(const char *tz)
{
    if (!tz) return NULL;
    if (strstr(tz, "int340") || strstr(tz, "intel")) return "int340x";
    if (strstr(tz, "coretemp") || strstr(tz, "x86_pkg")) return "coretemp";
    if (strstr(tz, "rockchip")) return "rockchip_thermal";
    if (strstr(tz, "exynos"))   return "exynos_tmu";
    if (strstr(tz, "acpitz") || strstr(tz, "acpi")) return "acpitz";
    return "thermal-core";
}

/* ---- W4: summary ---- */
int wubu_clock_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "clock[rtc=%d(%s) thermal=%d(%d zones/%s) cooling=%d]",
        g_rtc, wubu_clock_rtc_driver() ? wubu_clock_rtc_driver() : "none",
        g_thermal, g_thermal_zones,
        wubu_clock_thermal_driver() ? wubu_clock_thermal_driver() : "none",
        g_cooling);
}
