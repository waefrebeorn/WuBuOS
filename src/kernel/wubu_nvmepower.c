/*
 * wubu_nvmepower.c -- kernel-owned NVMe power state routing.
 *
 * NVMe drives expose power states (PS0-PS4) and autonomous power state
 * transition (APST) for power-vs-latency tuning. "Runs on everything"
 * includes correct NVMe power handling on every generation.
 *
 * Impl routing:
 *   - /sys/class/nvme nvme0/power (power state)
 *   - /sys/class/nvme nvme0/apst: APST enable
 */
#include "wubu_nvmepower.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_nvmepower_present = 0;
static int g_nvmepower_apst = 0;

void wubu_nvmepower_probe(void)
{
    /* Detect NVMe power + APST availability. */
#ifdef WUBU_HOSTED
    g_nvmepower_present = (access("/sys/class/nvme/nvme0/power", R_OK) == 0) ? 1 : 0;
    g_nvmepower_apst = (access("/sys/class/nvme/nvme0/apst", W_OK) == 0) ? 1 : 0;
#else
    g_nvmepower_present = g_nvmepower_apst = 0;
#endif
}

int wubu_nvmepower_present(void)
{
#ifdef WUBU_HOSTED
    return g_nvmepower_present;
#else
    return 0;
#endif
}

int wubu_nvmepower_state(int state)
{
    if (state < 0) return 0;
    if (state > 4) return 4;
    return state;
}

int wubu_nvmepower_apst_latency(int us)
{
    if (us < 0) return 0;
    return us;
}

void wubu_nvmepower_summary(char *out, size_t cap)
{
    snprintf(out, cap, "nvmepower[dev=%d apst=%d]",
             g_nvmepower_present, g_nvmepower_apst);
}
