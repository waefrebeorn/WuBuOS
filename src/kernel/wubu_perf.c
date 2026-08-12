/*
 * wubu_perf.c -- kernel-owned GPU performance counter routing.
 *
 * GPU perf counters measure shader/active cycles, memory bandwidth,
 * and cache hits via DRM_IOC_PERF_* ioctls.
 *
 * Impl routing:
 *   - /sys/class/drm card device/gt_boost_freq_mhz: GPU freq
 *   - /sys/class/drm card device/engine_*: per-engine activity
 *   - /sys/class/drm card device/turbo_boost_enable: turbo flag
 *   - /sys/class/drm card device/mem_info_vram_mem_*: VRAM usage
 */
#include "wubu_perf.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_perf_freq_mhz = 0;
static int g_perf_turbo = 0;
static int g_perf_engine_count = 0;
static int g_perf_present = 0;

void wubu_perf_probe(void)
{
    /* Detect GPU perf via sysfs presence. */
#ifdef _GNU_SOURCE
    g_perf_present = (access("/sys/class/drm/card0/device/gt_boost_freq_mhz", R_OK) == 0);
#else
    g_perf_present = 0;
#endif
}

int wubu_perf_present(void)
{
#ifdef _GNU_SOURCE
    return g_perf_present;
#else
    return 0;
#endif
}

const char *wubu_perf_engine_for(const char *name)
{
    if (!name) return NULL;
    if (strstr(name, "render")) return "render";
    if (strstr(name, "blitter")) return "blitter";
    if (strstr(name, "video") && strstr(name, "decode")) return "decode";
    if (strstr(name, "video") && strstr(name, "encode")) return "encode";
    if (strstr(name, "copy")) return "copy";
    if (strstr(name, "video")) return "video";
    return "unknown";
}

const char *wubu_perf_freq_str(void)
{
    static char buf[32];
    snprintf(buf, sizeof(buf), "%s%dMHz%s",
             g_perf_freq_mhz > 0 ? "" : "none",
             g_perf_freq_mhz,
             g_perf_turbo ? "T" : "");
    return buf;
}

void wubu_perf_summary(char *out, size_t cap)
{
    snprintf(out, cap, "perf[freq=%s engines=%d]",
             wubu_perf_freq_str(), g_perf_engine_count);
}
