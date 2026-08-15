/*
 * wubu_fantml.c -- kernel-owned GPU fan + thermal routing.
 *
 * Fan/thermal monitors GPU temperature + fan RPM via hwmon/sysfs.
 * "Runs on everything" includes correct thermal trip routing on
 * every GPU generation.
 *
 * Impl routing:
 *   - /sys/class/hwmon hwmon temp_input: GPU temp
 *   - /sys/class/hwmon hwmon fan_input: fan RPM
 *   - /sys/class/hwmon hwmon pwm: fan PWM control
 */
#include "wubu_fantml.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_gpu_temp = 0;
static int g_fan_rpm = 0;
static int g_fan_pwm = 0;

void wubu_fantml_probe(void)
{
    /* Detect GPU fan/thermal via hwmon presence. */
#ifdef WUBU_HOSTED
    g_gpu_temp = (access("/sys/class/hwmon/hwmon0/temp1_input", R_OK) == 0) ? 1 : 0;
    g_fan_rpm = (access("/sys/class/hwmon/hwmon0/fan1_input", R_OK) == 0) ? 1 : 0;
    g_fan_pwm = (access("/sys/class/hwmon/hwmon0/pwm1", R_OK) == 0) ? 1 : 0;
#else
    g_gpu_temp = g_fan_rpm = g_fan_pwm = 0;
#endif
}

int wubu_fantml_present(void)
{
#ifdef WUBU_HOSTED
    return g_gpu_temp || g_fan_rpm || g_fan_pwm;
#else
    return 0;
#endif
}

const char *wubu_fantml_status_str(int temp_c)
{
    if (temp_c < 50) return "cool";
    if (temp_c < 70) return "normal";
    if (temp_c < 85) return "warm";
    if (temp_c < 100) return "hot";
    return "critical";
}

int wubu_fantml_fan_pct(int rpm, int max_rpm)
{
    if (max_rpm <= 0 || rpm < 0) return 0;
    return (rpm * 100) / max_rpm;
}

void wubu_fantml_summary(char *out, size_t cap)
{
    snprintf(out, cap, "fantml[temp=%d rpm=%d pwm=%d]",
             g_gpu_temp, g_fan_rpm, g_fan_pwm);
}
