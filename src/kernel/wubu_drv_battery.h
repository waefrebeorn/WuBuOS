/*
 * wubu_drv_battery.h -- the ACPI battery driver.
 */
#ifndef WUBU_DRV_BATTERY_H
#define WUBU_DRV_BATTERY_H

/* the driver (registered by the registry) */
extern const struct wubu_drv wubu_drv_battery;

/* the test hooks + the ACPI bridge */
void wubu_battery_set_present(int present);
void wubu_battery_set_state(int charging, int capacity_mwh, int design_mwh,
                            int voltage_mv, const char *serial);

/* the state */
int wubu_battery_present(void);
int wubu_battery_charging(void);
int wubu_battery_capacity(void);
int wubu_battery_design(void);
int wubu_battery_voltage(void);
const char *wubu_battery_serial(void);

/* the percent (the ACPI _BST derivation) */
int wubu_battery_percent(void);

#endif
