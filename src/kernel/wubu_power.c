/*
 * wubu_power.c -- kernel-owned CPU/power/thermal driver routing + tuning.
 *
 * Power is the "runs on everything" efficiency spine. The kernel must
 * select the right cpufreq driver + governor per CPU vendor, tune C-states
 * for latency, route the battery via power_supply, and set thermal policy.
 * The most common headaches:
 *   - intel_pstate vs acpi-cpufreq: Intel uses intel_pstate (active),
 *     AMD uses amd_pstate (Epp/guided), older need acpi-cpufreq
 *   - governor choice: schedutil (default, dynamic) vs performance vs
 *     powersave (latency-sensitive apps need performance on some cores)
 *   - C-states: deep C-states add wake latency; latency-critical kernels
 *     want intel_idle.max_cstate to cap depth
 *   - battery charge thresholds (Framework-style sysfs) extend lifespan
 *   - thermal: the thermal zone + cooling device (fan/PWM) policy
 *
 * WuBuOS owns this: detect CPU vendor + governor, emit the right module
 * params and sysfs policy, expose power_supply + thermal topology.
 *
 * Research (Kevin-Bacon 7-hop on the power frontier):
 *   - cpufreq: intel_pstate (active, HWP), amd_pstate (Epp/guided),
 *     acpi-cpufreq (legacy), schedutil governor (default on modern)
 *   - C-states: intel_idle (Intel), acpi_idle (generic); deep C6/C7/C10
 *     add wake latency, cap via intel_idle.max_cstate
 *   - battery: power_supply class, /sys/class/power_supply/BAT0
 *   - thermal: thermal_zoneN + cooling_deviceN (fan/PWM/processor)
 */
#include "wubu_power.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

/* ---- CPU vendor IDs (CPUID vendor strings) ---- */
#define CPU_VENDOR_INTEL  1
#define CPU_VENDOR_AMD    2
#define CPU_VENDOR_ARM    3
#define CPU_VENDOR_OTHER  0

/* ---- cpufreq driver + governor per vendor ---- */
static int  g_cpu_vendor = CPU_VENDOR_OTHER;
static int  g_battery = 0;
static int  g_thermal = 0;
static int  g_fan = 0;
static int  g_ncores = 0;
static char g_cpufreq_drv[32] = "";
static char g_governor[24] = "";
static char g_cstate[48] = "";

/* ---- W1: probe the power/CPU topology ---- */
void wubu_power_probe(void)
{
    g_cpu_vendor = CPU_VENDOR_OTHER;
    g_battery = 0; g_thermal = 0; g_fan = 0; g_ncores = 0;
    g_cpufreq_drv[0] = '\0'; g_governor[0] = '\0'; g_cstate[0] = '\0';

#ifdef WUBU_HOSTED
    /* Detect CPU vendor via /proc/cpuinfo. */
    FILE *f = fopen("/proc/cpuinfo", "r");
    char line[256];
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "vendor_id") && strstr(line, "GenuineIntel")) {
                g_cpu_vendor = CPU_VENDOR_INTEL;
                break;
            }
            if (strstr(line, "vendor_id") && strstr(line, "AuthenticAMD")) {
                g_cpu_vendor = CPU_VENDOR_AMD;
                break;
            }
        }
        fclose(f);
    }
    if (g_cpu_vendor == CPU_VENDOR_OTHER) {
        /* ARM (aarch64) — no vendor_id; check /proc/cpuinfo "Processor". */
        f = fopen("/proc/cpuinfo", "r");
        if (f) {
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "Processor") || strstr(line, "CPU implementer")) {
                    g_cpu_vendor = CPU_VENDOR_ARM;
                    break;
                }
            }
            fclose(f);
        }
    }

    /* Count cores (logical processors). */
    f = fopen("/proc/cpuinfo", "r");
    if (f) {
        while (fgets(line, sizeof(line), f))
            if (strstr(line, "processor"))
                g_ncores++;
        fclose(f);
    }
    if (g_ncores == 0) g_ncores = 1;

    /* Battery present? */
    g_battery = (access("/sys/class/power_supply/BAT0", R_OK) == 0) ||
                (access("/sys/class/power_supply/BAT1", R_OK) == 0);

    /* Thermal zone present? */
    g_thermal = (access("/sys/class/thermal/thermal_zone0", R_OK) == 0);
    g_fan     = (access("/sys/class/thermal/cooling_device0", R_OK) == 0);
#endif
}

/* ---- W2: cpufreq driver + governor selection ---- */
const char *wubu_power_cpufreq_driver(void)
{
    if (g_cpufreq_drv[0]) return g_cpufreq_drv;
    switch (g_cpu_vendor) {
    case CPU_VENDOR_INTEL: strcpy(g_cpufreq_drv, "intel_pstate"); break;
    case CPU_VENDOR_AMD:   strcpy(g_cpufreq_drv, "amd_pstate"); break;
    case CPU_VENDOR_ARM:   strcpy(g_cpufreq_drv, "cpufreq-dt"); break;
    default:               strcpy(g_cpufreq_drv, "acpi-cpufreq"); break;
    }
    return g_cpufreq_drv;
}

const char *wubu_power_governor(void)
{
    if (g_governor[0]) return g_governor;
    /* schedutil is the default on modern; a latency kernel may prefer
     * performance. We route to schedutil unless told otherwise. */
    strcpy(g_governor, "schedutil");
    return g_governor;
}

/* C-state cap for latency-sensitive kernels (gaming). Deep C6/C7/C10 add
 * wake latency; capping at C1 keeps the first IRQ wake fast. */
const char *wubu_power_cstate_cap(void)
{
    if (g_cstate[0]) return g_cstate;
    if (g_cpu_vendor == CPU_VENDOR_INTEL)
        strcpy(g_cstate, "intel_idle.max_cstate=4");
    else if (g_cpu_vendor == CPU_VENDOR_AMD)
        strcpy(g_cstate, "processor.max_cstate=4");
    else
        strcpy(g_cstate, "");
    return g_cstate[0] ? g_cstate : NULL;
}

/* ---- W3: accessors ---- */
int  wubu_power_cpu_vendor(void)   { return g_cpu_vendor; }
int  wubu_power_has_battery(void)  { return g_battery; }
int  wubu_power_has_thermal(void)  { return g_thermal; }
int  wubu_power_has_fan(void)      { return g_fan; }
int  wubu_power_ncores(void)       { return g_ncores; }

/* ---- W4: summary ---- */
int wubu_power_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "power[cpu=%d cores=%d cpufreq=%s gov=%s bat=%d therm=%d fan=%d cstate=%s]",
        g_cpu_vendor, g_ncores,
        wubu_power_cpufreq_driver() ? wubu_power_cpufreq_driver() : "none",
        wubu_power_governor() ? wubu_power_governor() : "none",
        g_battery, g_thermal, g_fan,
        wubu_power_cstate_cap() ? wubu_power_cstate_cap() : "-");
}
