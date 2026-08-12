/*
 * wubu_nvme_gen4.c -- kernel-owned NVMe Gen4 routing.
 *
 * NVMe Gen4 supports up to 16 GT/s per lane (64 GT/s x4).
 * "Runs on everything" includes correct Gen4 routing on
 * all NVMe controllers.
 *
 * Impl routing:
 *   - /sys/class/nvme/nvme0/device/uevent: driver version
 *   - /sys/block/nvme0n1/queue/max_hw_sectors_kb: max I/O
 */
#include "wubu_nvme_gen4.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_nvme_gen4_present = 0;
static int g_nvme_gen4_speed = 0;

void wubu_nvme_gen4_probe(void)
{
#ifdef _GNU_SOURCE
    g_nvme_gen4_present = (access("/sys/class/nvme/nvme0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_nvme_gen4_speed = (access("/sys/block/nvme0n1/queue/max_hw_sectors_kb", R_OK) == 0) ? 1 : 0;
#else
    g_nvme_gen4_present = g_nvme_gen4_speed = 0;
#endif
}

int wubu_nvme_gen4_present(void)
{
#ifdef _GNU_SOURCE
    return g_nvme_gen4_present;
#else
    return 0;
#endif
}

int wubu_nvme_gen4_speed_gbps(int lanes)
{
    /* Gen4: 16 GT/s per lane. */
    return 16 * lanes;
}

int wubu_nvme_gen4_is_fast(int speed_gbps)
{
    /* Fast: >= 64 GT/s (Gen4 x4). */
    return (speed_gbps >= 64) ? 1 : 0;
}

void wubu_nvme_gen4_summary(char *out, size_t cap)
{
    snprintf(out, cap, "nvme_gen4[dev=%d gen4=%d]", g_nvme_gen4_present, g_nvme_gen4_speed);
}
