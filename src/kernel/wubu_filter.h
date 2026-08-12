/*
 * wubu_filter.h -- kernel-owned audio DSP filter + EQ routing.
 */
#ifndef WUBU_FILTER_H
#define WUBU_FILTER_H

#include <stddef.h>

/* W1: probe the filter topology. */
void wubu_filter_probe(void);

/* W2: accessors */
int  wubu_filter_present(void);
int  wubu_filter_biquad_present(void);
int  wubu_filter_eq(void);
int  wubu_filter_pw(void);
int  wubu_filter_alsa(void);
const char *wubu_filter_driver(void);

/* W3: filter routing. */
const char *wubu_filter_type_for(const char *t);

/* W4: biquad coefficient computation (Audio EQ Cookbook). */
void wubu_filter_biquad(double f, double fs, double q, double db,
                        const char *type, double *b0, double *b1, double *b2,
                        double *a1, double *a2);

/* W4b: summary fragment. */
int wubu_filter_summary(char *out, size_t cap);

#endif /* WUBU_FILTER_H */
