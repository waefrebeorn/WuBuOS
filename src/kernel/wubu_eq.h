/*
 * wubu_eq.h -- kernel-owned audio equalizer DSP coefficients routing.
 */
#ifndef WUBU_EQ_H
#define WUBU_EQ_H

#include <stddef.h>

/* W1: probe the EQ topology. */
void wubu_eq_probe(void);

/* W2: accessors */
int  wubu_eq_alsa(void);
int  wubu_eq_software(void);
int  wubu_eq_dsp(void);
int  wubu_eq_biquad(void);
int  wubu_eq_loudness(void);
const char *wubu_eq_driver(void);

/* W3: EQ driver routing. */
const char *wubu_eq_driver_for(const char *eq);

/* W4: summary fragment. */
int wubu_eq_summary(char *out, size_t cap);

#endif /* WUBU_EQ_H */
