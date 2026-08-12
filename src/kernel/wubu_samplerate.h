/*
 * wubu_samplerate.h -- kernel-owned audio sample rate/format routing.
 */
#ifndef WUBU_SAMPLERATE_H
#define WUBU_SAMPLERATE_H

#include <stddef.h>

void wubu_samplerate_probe(void);
int  wubu_samplerate_present(void);
int  wubu_samplerate_pcm(void);
int  wubu_samplerate_float(void);
int  wubu_samplerate_24bit(void);
int  wubu_samplerate_hi(void);
const char *wubu_samplerate_driver(void);
const char *wubu_samplerate_fmt_for(const char *f);
const char *wubu_samplerate_rate_for(const char *r);
int wubu_samplerate_summary(char *out, size_t cap);

#endif
