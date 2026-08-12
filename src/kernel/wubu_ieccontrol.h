/*
 * wubu_ieccontrol.h -- kernel-owned audio IEC control routing.
 */
#ifndef WUBU_IECCONTROL_H
#define WUBU_IECCONTROL_H

#include <stddef.h>

void wubu_ieccontrol_probe(void);
int  wubu_ieccontrol_present(void);
int  wubu_ieccontrol_aes(void);
int  wubu_ieccontrol_enc(void);
int  wubu_ieccontrol_clock(void);
int  wubu_ieccontrol_rate(void);
const char *wubu_ieccontrol_driver(void);
const char *wubu_ieccontrol_encoding_for(const char *e);
const char *wubu_ieccontrol_clock_for(const char *c);
int wubu_ieccontrol_summary(char *out, size_t cap);

#endif
