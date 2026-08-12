/*
 * wubu_thermal.c -- kernel-owned fan/thermal control routing.
 *
 * Thermal management controls CPU/GPU fans via hwmon PWM to keep temps
 * safe. "Runs on everything" includes correct cooling on every board.
 *
 * Thermal:
 *   - hwmon: /sys/class/hwmon, pwm1 (fan duty), temp1_input
 *   - fancontrol: fan speed curves (config: fancontrol in /etc)
 *   - thermal zone: /sys/class/thermal type + mode
 *   - pwm: fan PWM controller (pwm-fan, hwmon-pwm)
 *   - trip points: thermal_zone trip_point temp
 *
 * WuBuOS owns this: detect the hwmon fan + thermal zone, route to the
 * right driver, and expose the thermal topology.
 *
 * Research (Kevin-Bacon 7-hop on the thermal frontier):
 *   - hwmon: fan pwm1 + temp inputs
 *   - fancontrol: fan curves
 *   - thermal zone: trip points + mode (cooling)
 *   - pwm-fan driver
 */
#include "wubu_thermal.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_hwmon = 0;       /* hwmon present */
static int  g_fan = 0;         /* fan PWM */
static int  g_zone = 0;        /* thermal zone */
static int  g_trip = 0;        /* trip point */
static int  g_fancontrol = 0;  /* fancontrol */
static char g_thermal_drv[24] = "";

/* ---- W1: probe the thermal topology ---- */
void wubu_thermal_probe(void)
{
    g_hwmon = 0; g_fan = 0; g_zone = 0; g_trip = 0; g_fancontrol = 0;
    g_thermal_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* hwmon present? */
    if (access("/sys/class/hwmon", R_OK) == 0) {
        g_hwmon = 1;
        strcpy(g_thermal_drv, "hwmon");
    }
    /* fan PWM (pwm1)? */
    if (access("/sys/class/hwmon", R_OK) == 0) {
        DIR *d = opendir("/sys/class/hwmon");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_name[0] == 'h') {
                    char p[96];
                    snprintf(p, sizeof(p), "/sys/class/hwmon/%s/pwm1", e->d_name);
                    if (access(p, R_OK) == 0) { g_fan = 1; break; }
                }
            }
            closedir(d);
        }
    }
    /* thermal zone? */
    if (access("/sys/class/thermal/thermal_zone0", R_OK) == 0) {
        g_zone = 1;
        if (!g_thermal_drv[0]) strcpy(g_thermal_drv, "thermal-zone");
        /* trip points? */
        if (access("/sys/class/thermal/thermal_zone0/trip_point_0_temp", R_OK) == 0) {
            g_trip = 1;
        }
    }
    /* fancontrol present? */
    if (access("/usr/sbin/fancontrol", R_OK) == 0 ||
        access("/usr/bin/fancontrol", R_OK) == 0) {
        g_fancontrol = 1;
        if (!g_thermal_drv[0]) strcpy(g_thermal_drv, "fancontrol");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_thermal_hwmon(void) { return g_hwmon; }
int  wubu_thermal_fan(void)   { return g_fan; }
int  wubu_thermal_zone(void)  { return g_zone; }
int  wubu_thermal_trip(void)  { return g_trip; }
int  wubu_thermal_fancontrol(void){ return g_fancontrol; }
const char *wubu_thermal_driver(void){ return g_thermal_drv[0] ? g_thermal_drv : NULL; }

/* ---- W3: thermal routing ---- */
const char *wubu_thermal_mode_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "auto"))  return "auto";
    if (strstr(mode, "manual"))return "manual";
    if (strstr(mode, "disabled")) return "disabled";
    return "auto";
}

const char *wubu_thermal_curve_for(const char *curve)
{
    if (!curve) return NULL;
    if (strstr(curve, "aggressive")) return "aggressive";
    if (strstr(curve, "quiet"))    return "quiet";
    if (strstr(curve, "balanced")) return "balanced";
    return "balanced";
}

/* ---- W4: summary ---- */
int wubu_thermal_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "thermal[hwmon=%d fan=%d zone=%d trip=%d fanctl=%d drv=%s]",
        g_hwmon, g_fan, g_zone, g_trip, g_fancontrol,
        wubu_thermal_driver() ? wubu_thermal_driver() : "none");
}
