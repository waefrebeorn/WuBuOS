/*
 * wubu_thermal.h -- kernel-owned fan/thermal control routing.
 */
#ifndef WUBU_THERMAL_H
#define WUBU_THERMAL_H

#include <stddef.h>

/* W1: probe the thermal topology. */
void wubu_thermal_probe(void);

/* W2: accessors */
int  wubu_thermal_hwmon(void);
int  wubu_thermal_fan(void);
int  wubu_thermal_zone(void);
int  wubu_thermal_trip(void);
int  wubu_thermal_fancontrol(void);
const char *wubu_thermal_driver(void);

/* W3: thermal routing. */
const char *wubu_thermal_mode_for(const char *mode);
const char *wubu_thermal_curve_for(const char *curve);

/* W4: summary fragment. */
int wubu_thermal_summary(char *out, size_t cap);

#endif /* WUBU_THERMAL_H */
