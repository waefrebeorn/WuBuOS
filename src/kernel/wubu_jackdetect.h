/*
 * wubu_jackdetect.h -- kernel-owned audio jack detection routing.
 */
#ifndef WUBU_JACKDETECT_H
#define WUBU_JACKDETECT_H

#include <stddef.h>

void wubu_jackdetect_probe(void);
int  wubu_jackdetect_present(void);
int  wubu_jackdetect_headset(void);
int  wubu_jackdetect_mic(void);
int  wubu_jackdetect_omtp(void);
int  wubu_jackdetect_ctia(void);
const char *wubu_jackdetect_driver(void);
const char *wubu_jackdetect_pinout_for(const char *p);
const char *wubu_jackdetect_state_for(const char *s);
int wubu_jackdetect_summary(char *out, size_t cap);

#endif
