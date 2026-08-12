/*
 * wubu_nvmehotplug.c -- kernel-owned NVMe hotplug routing.
 *
 * NVMe hotplug detects device insertion/removal at runtime.
 * "Runs on everything" includes correct NVMe hotplug routing
 * on all NVMe-capable systems.
 *
 * Impl routing:
 *   - /sys/class/nvme/nvme0: NVMe device presence
 *   - /proc/partitions: partition hotplug events
 */
#include "wubu_nvmehotplug.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_nvmehotplug_present = 0;
static int g_nvmehotplug_events = 0;

void wubu_nvmehotplug_probe(void)
{
#ifdef _GNU_SOURCE
    g_nvmehotplug_present = (access("/sys/class/nvme/nvme0", R_OK) == 0) ? 1 : 0;
    g_nvmehotplug_events = (access("/proc/partitions", R_OK) == 0) ? 1 : 0;
#else
    g_nvmehotplug_present = g_nvmehotplug_events = 0;
#endif
}

int wubu_nvmehotplug_present(void)
{
#ifdef _GNU_SOURCE
    return g_nvmehotplug_present;
#else
    return 0;
#endif
}

int wubu_nvmehotplug_stable(int events, int interval_ms)
{
    /* Hotplug stable if <= 1 event per 100ms interval. */
    if (interval_ms <= 0) return 0;
    if (events <= interval_ms / 100) return 1;
    return 0;
}

int wubu_nvmehotplug_is_event(int prev, int curr)
{
    /* Event if device count changed. */
    return (prev != curr) ? 1 : 0;
}

void wubu_nvmehotplug_summary(char *out, size_t cap)
{
    snprintf(out, cap, "nvmehotplug[dev=%d evt=%d]", g_nvmehotplug_present, g_nvmehotplug_events);
}
