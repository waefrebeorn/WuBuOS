/*
 * wubu_gpusensor.h -- kernel-owned GPU sensor + fan curve routing.
 */
#ifndef WUBU_GPUSENSOR_H
#define WUBU_GPUSENSOR_H

#include <stddef.h>

/* W1: probe the GPU-sensor topology. */
void wubu_gpusensor_probe(void);

/* W2: accessors */
int  wubu_gpusensor_hwmon(void);
int  wubu_gpusensor_temp(void);
int  wubu_gpusensor_fan(void);
int  wubu_gpusensor_power(void);
int  wubu_gpusensor_curve(void);
const char *wubu_gpusensor_driver(void);

/* W3: GPU sensor routing. */
const char *wubu_gpusensor_curve_for(const char *curve);
const char *wubu_gpusensor_metric_for(const char *m);

/* W4: summary fragment. */
int wubu_gpusensor_summary(char *out, size_t cap);

#endif /* WUBU_GPUSENSOR_H */
