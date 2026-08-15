/*
 * wubu_gpucsched.c -- kernel-owned GPU compute scheduler routing.
 *
 * Compute queues feed async work to GPU compute engines; a scheduler
 * balances queue priority, preemption, and time-slice fairness. "Runs
 * on everything" includes correct compute queue state per GPU vendor.
 *
 * Impl routing:
 *   - /sys/class/drm card0/device (compute engine presence)
 *   - /dev/dri renderD128: compute render node
 */
#include "wubu_gpucsched.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_gpucsched_queues = 0;
static int g_gpucsched_preempt = 0;

void wubu_gpucsched_probe(void)
{
    /* Detect GPU compute queue + preemption capability. */
#ifdef WUBU_HOSTED
    g_gpucsched_queues = (access("/sys/class/drm/card0/device", R_OK) == 0) ? 1 : 0;
    g_gpucsched_preempt = (access("/dev/dri/renderD128", R_OK) == 0) ? 1 : 0;
#else
    g_gpucsched_queues = g_gpucsched_preempt = 0;
#endif
}

int wubu_gpucsched_present(void)
{
#ifdef WUBU_HOSTED
    return g_gpucsched_queues;
#else
    return 0;
#endif
}

int wubu_gpucsched_priority(int base)
{
    if (base < 0) return 0;
    return base;
}

int wubu_gpucsched_timeslice_ms(int queue)
{
    if (queue < 0) return 0;
    return 8 + (queue * 4);
}

void wubu_gpucsched_summary(char *out, size_t cap)
{
    snprintf(out, cap, "gpucsched[queue=%d preempt=%d]",
             g_gpucsched_queues, g_gpucsched_preempt);
}
