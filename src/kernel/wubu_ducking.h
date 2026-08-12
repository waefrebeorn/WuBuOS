/*
 * wubu_ducking.h -- kernel-owned audio ducking + compressor routing.
 */
#ifndef WUBU_DUCKING_H
#define WUBU_DUCKING_H

#include <stddef.h>

void wubu_ducking_probe(void);
int  wubu_ducking_present(void);
int  wubu_ducking_comp(void);
int  wubu_ducking_limiter(void);
int  wubu_ducking_gate(void);
int  wubu_ducking_sidechain(void);
const char *wubu_ducking_driver(void);
const char *wubu_ducking_type_for(const char *t);
const char *wubu_ducking_mode_for(const char *m);
int wubu_ducking_summary(char *out, size_t cap);

#endif
