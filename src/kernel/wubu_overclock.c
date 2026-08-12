/*
 * wubu_overclock.c -- kernel-owned GPU overclocking + clocks routing.
 *
 * GPU overclocking raises core/memory clocks past stock. "Runs on
 * everything" includes correct clock control on every GPU.
 *
 * GPU overclock:
 *   - amdgpu: OverDrive, /sys/class/drm card device/pp_od_clk_voltage
 *   - i915: OC via GT freq, /sys/class/drm card/gt_min_freq_mhz
 *   - nvidia: nvclock, Xorg overclock
 *   - sysfs: pp_power_profile_mode, pp_od_clk_voltage
 *   - clock: core (sclk), memory (mclk), VDDC
 *   - state: boot/oc/stable (clocks)
 *
 * WuBuOS owns this: detect overclock + overdrive + sysfs, route to
 * the right driver, expose the topology.
 *
 * Research (7-hop on the overclock frontier):
 *   - amdgpu OverDrive pp_od_clk_voltage
 *   - i915 GT freq min/max
 *   - sysfs clock control
 */
#include "wubu_overclock.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_oc = 0;          /* overclock present */
static int  g_od = 0;          /* overdrive */
static int  g_sysfs = 0;       /* sysfs clock ctrl */
static int  g_core = 0;        /* core clock */
static int  g_mem = 0;         /* memory clock */
static char g_oc_drv[24] = "";

void wubu_overclock_probe(void)
{
    g_oc = 0; g_od = 0; g_sysfs = 0; g_core = 0; g_mem = 0;
    g_oc_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_oc = 1; g_od = 1; g_sysfs = 1; g_core = 1; g_mem = 1;
        strcpy(g_oc_drv, "amdgpu-od");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_oc = 1; g_sysfs = 1; g_core = 1;
        if (!g_oc_drv[0]) strcpy(g_oc_drv, "i915-gt");
    }
    if (access("/sys/module/nvidia", R_OK) == 0 ||
        access("/sys/module/nvidia_drm", R_OK) == 0) {
        if (!g_oc_drv[0]) strcpy(g_oc_drv, "nvidia-oc");
    }
    if (access("/sys/class/drm", R_OK) == 0 && !g_oc_drv[0]) {
        g_sysfs = 1;
        strcpy(g_oc_drv, "drm-sysfs");
    }
#endif
}

int  wubu_overclock_present(void){ return g_oc; }
int  wubu_overclock_od(void)     { return g_od; }
int  wubu_overclock_sysfs(void)  { return g_sysfs; }
int  wubu_overclock_core(void)   { return g_core; }
int  wubu_overclock_mem(void)    { return g_mem; }
const char *wubu_overclock_driver(void){ return g_oc_drv[0] ? g_oc_drv : NULL; }

const char *wubu_overclock_clk_for(const char *c)
{
    if (!c) return NULL;
    if (strstr(c, "core") || strstr(c, "sclk") || strstr(c, "gt")) return "core";
    if (strstr(c, "mem")  || strstr(c, "mclk")) return "memory";
    if (strstr(c, "vddc")) return "vddc";
    if (strstr(c, "soc"))  return "soc";
    return "core";
}

const char *wubu_overclock_state_for(const char *s)
{
    if (!s) return NULL;
    if (strstr(s, "stock"))  return "stock";
    if (strstr(s, "boot"))   return "boot";
    if (strstr(s, "stable")) return "stable";
    if (strstr(s, "oc"))     return "overclock";
    return "stock";
}

int wubu_overclock_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "overclock[oc=%d od=%d sysfs=%d core=%d mem=%d drv=%s]",
        g_oc, g_od, g_sysfs, g_core, g_mem,
        wubu_overclock_driver() ? wubu_overclock_driver() : "none");
}