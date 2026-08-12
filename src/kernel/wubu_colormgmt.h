/*
 * wubu_colormgmt.h -- kernel-owned display color management routing.
 */
#ifndef WUBU_COLORMGMT_H
#define WUBU_COLORMGMT_H

#include <stddef.h>

/* W1: probe the color-mgmt topology. */
void wubu_colormgmt_probe(void);

/* W2: accessors */
int  wubu_colormgmt_ctm(void);
int  wubu_colormgmt_gamma(void);
int  wubu_colormgmt_degamma(void);
int  wubu_colormgmt_csc(void);
int  wubu_colormgmt_3dlut(void);
const char *wubu_colormgmt_driver(void);

/* W3: color-mgmt routing. */
const char *wubu_colormgmt_lut_for(const char *lut);
const char *wubu_colormgmt_csc_for(const char *cs);

/* W4: summary fragment. */
int wubu_colormgmt_summary(char *out, size_t cap);

#endif /* WUBU_COLORMGMT_H */
