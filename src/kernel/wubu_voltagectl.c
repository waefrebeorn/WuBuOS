/*
 * wubu_voltagectl.c -- kernel-owned GPU voltage control routing.
 *
 * Voltage control monitors GPU core/memory voltage rails via hwmon.
 * "Runs on everything" includes correct voltage trip routing on
 * every GPU generation.
 *
 * Impl routing:
 *   - /sys/class/hwmon hwmon in*_input: voltage rail
 */
#include "wubu_voltagectl.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_vddc_present = 0;
static int g_vddgfx_present = 0;

void wubu_voltagectl_probe(void)
{
    /* Detect GPU voltage via hwmon presence. */
#ifdef _GNU_SOURCE
    g_vddc_present = (access("/sys/class/hwmon/hwmon0/in0_input", R_OK) == 0) ? 1 : 0;
    g_vddgfx_present = (access("/sys/class/hwmon/hwmon0/in1_input", R_OK) == 0) ? 1 : 0;
#else
    g_vddc_present = g_vddgfx_present = 0;
#endif
}

int wubu_voltagectl_present(void)
{
#ifdef _GNU_SOURCE
    return g_vddc_present || g_vddgfx_present;
#else
    return 0;
#endif
}

const char *wubu_voltagectl_state_str(int mV)
{
    if (mV < 700) return "low";
    if (mV < 900) return "nominal";
    if (mV < 1100) return "high";
    return "critical";
}

int wubu_voltagectl_mv_to_uv(int mV)
{
    return mV * 1000;
}

void wubu_voltagectl_summary(char *out, size_t cap)
{
    snprintf(out, cap, "voltagectl[vddc=%d vddgfx=%d]",
             g_vddc_present, g_vddgfx_present);
}
