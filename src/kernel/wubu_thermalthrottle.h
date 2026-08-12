/*
 * wubu_thermalthrottle.h -- kernel-owned thermal throttling routing.
 */
#ifndef WUBU_THERMALTHROTTLE_H
#define WUBU_THERMALTHROTTLE_H

#include <stddef.h>

void wubu_thermalthrottle_probe(void);
int  wubu_thermalthrottle_present(void);
int  wubu_thermalthrottle_zone(void);
int  wubu_thermalthrottle_cooling(void);
int  wubu_thermalthrottle_trip(void);
int  wubu_thermalthrottle_governor(void);
const char *wubu_thermalthrottle_driver(void);
const char *wubu_thermalthrottle_gov_for(const char *g);
const char *wubu_thermalthrottle_trip_for(const char *t);
int wubu_thermalthrottle_summary(char *out, size_t cap);

#endif
