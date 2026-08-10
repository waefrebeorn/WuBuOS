/*
 * wubu_drv_thermal.h -- the thermal driver.
 */
#ifndef WUBU_DRV_THERMAL_H
#define WUBU_DRV_THERMAL_H

/* the zones */
enum {
    WUBU_THERMAL_CPU = 0,
    WUBU_THERMAL_GPU,
    WUBU_THERMAL_SKIN,
};

/* the driver (registered by the registry) */
extern const struct wubu_drv wubu_drv_thermal;

/* TH3: the policy — the fan duty from the hottest zone. */
void wubu_thermal_policy_update(void);

/* the test hooks */
void wubu_thermal_set_present(int p);
void wubu_thermal_set_temp(int zone, int c);
void wubu_thermal_set_limit(int zone, int c);

/* the state */
int wubu_thermal_present(void);
int wubu_thermal_temp(int zone);
int wubu_thermal_fan_duty(void);
int wubu_thermal_throttled(void);
const char *wubu_thermal_zone_name(int z);

#endif
