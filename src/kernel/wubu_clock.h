/*
 * wubu_clock.h -- kernel-owned clock (RTC) + thermal driver routing.
 */
#ifndef WUBU_CLOCK_H
#define WUBU_CLOCK_H

#include <stddef.h>

/* W1: probe the clock/thermal topology. */
void wubu_clock_probe(void);

/* W2: accessors */
int  wubu_clock_has_rtc(void);
int  wubu_clock_has_thermal(void);
int  wubu_clock_thermal_zones(void);
int  wubu_clock_has_cooling(void);
const char *wubu_clock_rtc_driver(void);
const char *wubu_clock_thermal_driver(void);

/* W3: driver routing. */
const char *wubu_clock_rtc_for(const char *rtc);
const char *wubu_clock_thermal_for(const char *tz);

/* W4: summary fragment. */
int wubu_clock_summary(char *out, size_t cap);

#endif /* WUBU_CLOCK_H */
