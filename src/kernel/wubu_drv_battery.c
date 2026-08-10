/*
 * wubu_drv_battery.c -- the ACPI BATTERY driver (the Deck's battery +
 * every laptop's battery).
 *
 * The ACPI battery contract (_BIF = battery info, _BST = battery
 * status):
 *   _BIF -> the design capacity, the voltage, the serial
 *   _BST -> the state (charging/discharging), the present capacity
 *
 * This driver models that contract with a state table the ACPI layer
 * fills. The tests inject the battery state directly.
 *
 * C11.
 */
#include "wubu_drv.h"
#include "wubu_drv_battery.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int    present;
    int    charging;        /* 1 = charging, 0 = discharging */
    int    capacity_mwh;    /* the present capacity */
    int    design_mwh;      /* the design capacity */
    int    voltage_mv;
    char   serial[16];
} wubu_battery_t;

static wubu_battery_t g_bat;

/* B1: the driver probe — the battery is present iff the ACPI _BIF
 * exposed a design capacity. */
static int battery_probe(wubu_drv_dev_t *dev)
{
    (void)dev;
    if (!g_bat.present) return -1;
    return 0;
}

const wubu_drv_id_t wubu_battery_ids[] = {
    { WUBU_DRV_ANY, WUBU_DRV_ANY, 0x01, 0x0C },  /* the ACPI battery class */
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_battery = {
    "battery", wubu_battery_ids, 1, battery_probe,
};

/* the test hooks + the ACPI bridge */
void wubu_battery_set_present(int present) { g_bat.present = present; }
void wubu_battery_set_state(int charging, int capacity_mwh, int design_mwh,
                            int voltage_mv, const char *serial)
{
    g_bat.charging = charging;
    g_bat.capacity_mwh = capacity_mwh;
    g_bat.design_mwh = design_mwh;
    g_bat.voltage_mv = voltage_mv;
    if (serial) snprintf(g_bat.serial, sizeof(g_bat.serial), "%s", serial);
}

int wubu_battery_present(void) { return g_bat.present; }
int wubu_battery_charging(void) { return g_bat.charging; }
int wubu_battery_capacity(void) { return g_bat.capacity_mwh; }
int wubu_battery_design(void) { return g_bat.design_mwh; }
int wubu_battery_voltage(void) { return g_bat.voltage_mv; }
const char *wubu_battery_serial(void) { return g_bat.serial; }

/* the percent (the standard ACPI _BST derivation) */
int wubu_battery_percent(void)
{
    if (!g_bat.present || g_bat.design_mwh <= 0) return 0;
    int pct = (g_bat.capacity_mwh * 100) / g_bat.design_mwh;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}
