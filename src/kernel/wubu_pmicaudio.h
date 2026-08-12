/*
 * wubu_pmicaudio.h -- kernel-owned PMIC + audio amp/DAC driver routing.
 */
#ifndef WUBU_PMICAUDIO_H
#define WUBU_PMICAUDIO_H

#include <stddef.h>

/* W1: probe the PMIC/audio-analog topology. */
void wubu_pmicaudio_probe(void);

/* W2: accessors */
int  wubu_pmicaudio_pmic(void);
int  wubu_pmicaudio_dac(void);
int  wubu_pmicaudio_amp(void);
int  wubu_pmicaudio_regulator(void);
const char *wubu_pmicaudio_pmic_driver(void);
const char *wubu_pmicaudio_dac_driver(void);
const char *wubu_pmicaudio_amp_driver(void);

/* W3: driver routing. */
const char *wubu_pmicaudio_dac_route(const char *dac);
const char *wubu_pmicaudio_amp_route(const char *amp);

/* W4: summary fragment. */
int wubu_pmicaudio_summary(char *out, size_t cap);

#endif /* WUBU_PMICAUDIO_H */
