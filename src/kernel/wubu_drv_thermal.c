/*
 * wubu_drv_thermal.c -- the THERMAL driver (the Deck's cooling + every
 * laptop's cooling).
 *
 * The thermal zones (the CPU + the GPU + the skin) + the fan policy:
 * the same class contract as the EC fan (wubu_ec_control). This
 * driver adds the TEMPERATURE SENSORS + the policy:
 *
 *   - the zones (cpu / gpu / skin) with the current temp
 *   - the policy: the fan duty from the hottest zone (a simple
 *     hysteresis curve — the Deck's fan curve essence)
 *   - the throttle flag (the temp over the limit -> the gamemode
 *     should ease off)
 *
 * The tests inject the zone temps + assert the policy output.
 *
 * C11.
 */
#include "wubu_drv.h"
#include "wubu_drv_thermal.h"

#include <stdio.h>
#include <string.h>

#define WUBU_THZONES 3

typedef struct {
    int present;
    int temp[WUBU_THZONES];    /* celsius */
    int limit[WUBU_THZONES];   /* the throttle limit */
    int fan_duty;              /* the policy output 0-100 */
    int throttled;
} wubu_thermal_t;

static wubu_thermal_t g_th;

/* TH1: the driver probe. */
static int thermal_probe(wubu_drv_dev_t *dev)
{
    (void)dev;
    g_th.present = 1;
    for (int i = 0; i < WUBU_THZONES; i++) {
        if (g_th.limit[i] == 0) g_th.limit[i] = 90;
    }
    wubu_thermal_policy_update();
    return 0;
}

const wubu_drv_id_t wubu_thermal_ids[] = {
    { WUBU_DRV_ANY, WUBU_DRV_ANY, 0x11, 0x00 },  /* the signal proc/thermal */
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_thermal = {
    "thermal", wubu_thermal_ids, 1, thermal_probe,
};

/* the zone names */
const char *wubu_thermal_zone_name(int z)
{
    switch (z) {
    case WUBU_THERMAL_CPU:  return "cpu";
    case WUBU_THERMAL_GPU:  return "gpu";
    case WUBU_THERMAL_SKIN: return "skin";
    default:                return "?";
    }
}

/* TH3: the policy — the fan duty from the hottest zone.
 * The curve (the Deck's essence): below 50C = quiet (20%),
 * 50-70C = linear, over 70C = ramp to 100%, over the limit =
 * throttled + 100%. */
void wubu_thermal_policy_update(void)
{
    int hottest = 0;
    for (int i = 0; i < WUBU_THZONES; i++)
        if (g_th.temp[i] > hottest) hottest = g_th.temp[i];

    g_th.throttled = 0;
    for (int i = 0; i < WUBU_THZONES; i++)
        if (g_th.temp[i] >= g_th.limit[i]) g_th.throttled = 1;

    if (hottest < 50)      g_th.fan_duty = 20;
    else if (hottest < 70)  g_th.fan_duty = 20 + (hottest - 50) * 2;
    else if (hottest < 90)  g_th.fan_duty = 60 + (hottest - 70) * 2;
    else                    g_th.fan_duty = 100;
    if (g_th.fan_duty > 100) g_th.fan_duty = 100;
    if (g_th.throttled) g_th.fan_duty = 100;
}

/* the test hooks */
void wubu_thermal_set_present(int p) { g_th.present = p; }
void wubu_thermal_set_temp(int zone, int c) { g_th.temp[zone] = c; }
void wubu_thermal_set_limit(int zone, int c) { g_th.limit[zone] = c; }
int wubu_thermal_present(void) { return g_th.present; }
int wubu_thermal_temp(int zone) { return g_th.temp[zone]; }
int wubu_thermal_fan_duty(void) { return g_th.fan_duty; }
int wubu_thermal_throttled(void) { return g_th.throttled; }
