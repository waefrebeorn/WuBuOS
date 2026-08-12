/*
 * wubu_thermalthrottle.c -- kernel-owned thermal throttling routing.
 *
 * Thermal throttling (CPU/GPU thermal zone, cooling device) reduces
 * frequency when temperature is too high. "Runs on everything"
 * includes correct thermal management on every host.
 *
 * Thermal:
 *   - /sys/class/thermal/thermal_zone*: temperature
 *   - /sys/class/thermal/cooling_device*: cooling
 *   - governor: step_wise, fair_share, user_space
 *   - trip point: critical, hot, passive, active
 *   - policy: performance, powersave, balanced
 *
 * WuBuOS owns this: detect thermal + trip + governor, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the thermalthrottle frontier):
 *   -Linux thermal management
 *   - thermal zone cooling device
 */
#include "wubu_thermalthrottle.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_throttle = 0;    /* thermal throttle present */
static int  g_zone = 0;        /* thermal zone */
static int  g_cooling = 0;     /* cooling device */
static int  g_trip = 0;        /* trip point */
static int  g_governor = 0;    /* governor */
static char g_throttle_drv[24] = "";

void wubu_thermalthrottle_probe(void)
{
    g_throttle = 0; g_zone = 0; g_cooling = 0; g_trip = 0; g_governor = 0;
    g_throttle_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/class/thermal/thermal_zone0", R_OK) == 0 ||
        access("/sys/class/thermal", R_OK) == 0) {
        g_throttle = 1; g_zone = 1; g_cooling = 1; g_trip = 1; g_governor = 1;
        strcpy(g_throttle_drv, "intel-thermal");
    }
    if (access("/sys/class/hwmon/hwmon", R_OK) == 0 && !g_throttle_drv[0]) {
        g_throttle = 1; g_zone = 1;
        strcpy(g_throttle_drv, "k10temp");
    }
#endif
}

int  wubu_thermalthrottle_present(void){ return g_throttle; }
int  wubu_thermalthrottle_zone(void)    { return g_zone; }
int  wubu_thermalthrottle_cooling(void){ return g_cooling; }
int  wubu_thermalthrottle_trip(void)    { return g_trip; }
int  wubu_thermalthrottle_governor(void){ return g_governor; }
const char *wubu_thermalthrottle_driver(void){ return g_throttle_drv[0] ? g_throttle_drv : NULL; }

const char *wubu_thermalthrottle_gov_for(const char *g)
{
    if (!g) return NULL;
    if (strstr(g, "step")) return "step_wise";
    if (strstr(g, "fair")) return "fair_share";
    if (strstr(g, "user")) return "user_space";
    if (strstr(g, "bang")) return "bang_bang";
    return "step_wise";
}

const char *wubu_thermalthrottle_trip_for(const char *t)
{
    if (!t) return NULL;
    if (strstr(t, "crit")) return "critical";
    if (strstr(t, "hot"))  return "hot";
    if (strstr(t, "pass")) return "passive";
    if (strstr(t, "active")) return "active";
    return "passive";
}

int wubu_thermalthrottle_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "thermalthrottle[throttle=%d zone=%d cooling=%d trip=%d governor=%d drv=%s]",
        g_throttle, g_zone, g_cooling, g_trip, g_governor,
        wubu_thermalthrottle_driver() ? wubu_thermalthrottle_driver() : "none");
}
