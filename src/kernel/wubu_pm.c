/*
 * wubu_pm.c -- kernel-owned power mode (S0ix/deep sleep/runtime PM) routing.
 *
 * Power modes are how the machine sleeps and wakes. "Runs on everything"
 * includes correct idle/suspend/hibernate and battery life. WuBuOS owns
 * the power-mode routing + sleep-state exposure.
 *
 * Power states:
 *   - S0ix / s2idle: modern x86 low-power idle (Intel/AMD)
 *   - S3 (suspend to RAM), S4 (hibernate), S5 (shutdown)
 *   - Runtime PM: per-device autosuspend (dpm, sysfs power control)
 *   - cpuidle: CPU idle states (intel_idle, acpi_idle)
 *   - hibernate: swap-based /dev/resume + hibernation_platform
 *
 * WuBuOS owns this: detect the supported sleep states (ACPI/sysfs),
 * the runtime PM framework, and the CPU idle driver.
 *
 * Research (Kevin-Bacon 7-hop on the power-mode frontier):
 *   - S0ix (s2idle): modern low-power idle (intel_idle, amd-pstate)
 *   - S3/S4/S5: ACPI sleep states (/sys/power/state)
 *   - runtime PM: sysfs device power/control (auto/on), autosuspend
 *   - cpuidle: intel_idle, acpi_idle (C-states)
 */
#include "wubu_pm.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_s0ix = 0;      /* s2idle/S0ix */
static int  g_s3 = 0;        /* suspend to RAM */
static int  g_s4 = 0;        /* hibernate */
static int  g_runtime_pm = 0;
static int  g_cpuidle = 0;
static char g_pm_drv[32] = "";
static char g_idle_drv[32] = "";

/* ---- W1: probe the power-mode topology ---- */
void wubu_pm_probe(void)
{
    g_s0ix = 0; g_s3 = 0; g_s4 = 0; g_runtime_pm = 0; g_cpuidle = 0;
    g_pm_drv[0] = '\0'; g_idle_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* /sys/power/state lists supported sleep states (s2idle/s3/s4). */
    FILE *f = fopen("/sys/power/state", "r");
    if (f) {
        char st[64] = "";
        if (fgets(st, sizeof(st), f)) {
            if (strstr(st, "s2idle")) g_s0ix = 1;
            if (strstr(st, "s3") || strstr(st, "mem")) g_s3 = 1;
            if (strstr(st, "s4") || strstr(st, "disk")) g_s4 = 1;
        }
        fclose(f);
    }
    /* Runtime PM framework present? */
    if (access("/sys/power/pm_async", R_OK) == 0 ||
        access("/sys/power/pm_print_times", R_OK) == 0) {
        g_runtime_pm = 1;
        strcpy(g_pm_drv, "runtime-pm");
    }
    /* CPU idle driver present? */
    if (access("/sys/bus/platform/drivers/intel_idle", R_OK) == 0) {
        g_cpuidle = 1;
        strcpy(g_idle_drv, "intel_idle");
    } else if (access("/sys/bus/platform/drivers/acpi_idle", R_OK) == 0) {
        g_cpuidle = 1;
        strcpy(g_idle_drv, "acpi_idle");
    } else if (access("/sys/devices/system/cpu/cpuidle", R_OK) == 0) {
        g_cpuidle = 1;
        if (!g_idle_drv[0]) strcpy(g_idle_drv, "cpuidle");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_pm_s0ix(void)       { return g_s0ix; }
int  wubu_pm_s3(void)         { return g_s3; }
int  wubu_pm_s4(void)         { return g_s4; }
int  wubu_pm_runtime(void)    { return g_runtime_pm; }
int  wubu_pm_cpuidle(void)    { return g_cpuidle; }
const char *wubu_pm_driver(void){ return g_pm_drv[0] ? g_pm_drv : NULL; }
const char *wubu_pm_idle_driver(void){ return g_idle_drv[0] ? g_idle_drv : NULL; }

/* ---- W3: sleep-state routing ---- */
const char *wubu_pm_sleep_state(int mask)
{
    if (mask & (1<<0)) return "s2idle";   /* S0ix */
    if (mask & (1<<1)) return "s3";       /* suspend to RAM */
    if (mask & (1<<2)) return "s4";       /* hibernate */
    return "none";
}

const char *wubu_pm_idle_for(const char *cpu)
{
    if (!cpu) return NULL;
    if (strstr(cpu, "intel")) return "intel_idle";
    if (strstr(cpu, "amd"))   return "acpi_idle";
    if (strstr(cpu, "arm"))   return "cpuidle-arm";
    return "cpuidle";
}

/* ---- W4: summary ---- */
int wubu_pm_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "pm[s0ix=%d s3=%d s4=%d runtime=%d cpuidle=%d(%s) drv=%s]",
        g_s0ix, g_s3, g_s4, g_runtime_pm, g_cpuidle,
        wubu_pm_idle_driver() ? wubu_pm_idle_driver() : "none",
        wubu_pm_driver() ? wubu_pm_driver() : "none");
}
