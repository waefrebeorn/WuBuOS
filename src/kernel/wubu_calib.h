/*
 * wubu_calib.h -- kernel-owned display brightness/gamma calibration routing.
 */
#ifndef WUBU_CALIB_H
#define WUBU_CALIB_H

#include <stddef.h>

/* W1: probe the calibration topology. */
void wubu_calib_probe(void);

/* W2: accessors */
int  wubu_calib_drm_color(void);
int  wubu_calib_ddc(void);
int  wubu_calib_gamma(void);
int  wubu_calib_colord(void);
int  wubu_calib_icc(void);
const char *wubu_calib_driver(void);

/* W3: calibration driver routing. */
const char *wubu_calib_driver_for(const char *calib);

/* W4: summary fragment. */
int wubu_calib_summary(char *out, size_t cap);

#endif /* WUBU_CALIB_H */
