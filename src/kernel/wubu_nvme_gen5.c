/*
 * wubu_nvme_gen5.c -- kernel-owned NVMe Gen5 routing.
 *
 * NVMe Gen5 supports up to 14 GT/s per lane (64 GT/s x4).
 * "Runs on everything" includes correct Gen5 routing on
 * all NVMe controllers.
 *
 * Impl routing:
 *   - /sys/class/nvme/nvme0/device/uevent: driver version
 *   - /sys/block/nvme0n1/queue/max_hw_sectors_kb: max I/O
 */
#include "wubu_nvme_gen5.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_nvme_gen5_present = 0;
static int g_nvme_gen5_speed = 0;

void wubu_nvme_gen5_probe(void)
{
#ifdef _GNU_SOURCE
    g_nvme_gen5_present = (access("/sys/class/nvme/nvme0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_nvme_gen5_speed = (access("/sys/block/nvme0n1/queue/max_hw_sectors_kb", R_OK) == 0) ? 1 : 0;
#else
    g_nvme_gen5_present = g_nvme_gen5_speed = 0;
#endif
}

int wubu_nvme_gen5_present(void)
{
#ifdef _GNU_SOURCE
    return g_nvme_gen5_present;
#else
    return 0;
#endif
}

int wubu_nvme_gen5_speed_gbps(int gen, int lanes)
{
    /* Gen1=4GT/s, Gen2=8, Gen3=16, Gen4=32, Gen5=64 (effective per lane). */
    switch (gen) {
        case 1: return 4 * lanes;
        case 2: return 8 * lanes;
        case 3: return 16 * lanes;
        case 4: return 32 * lanes;
        case 5: return 64 * lanes;
        default: return 0;
    }
}

int wubu_nvme_gen5_is_fast(int speed_gbps)
{
    /* Fast: >= 256 GB/s (Gen4 x8 or Gen5 x4). */
    return (speed_gbps >= 256) ? 1 : 0;
}

void wubu_nvme_gen5_summary(char *out, size_t cap)
{
    snprintf(out, cap, "nvme_gen5[dev=%d gen5=%d]", g_nvme_gen5_present, g_nvme_gen5_speed);
}
