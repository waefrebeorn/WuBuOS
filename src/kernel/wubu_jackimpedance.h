/*
 * wubu_jackimpedance.h -- kernel-owned audio jack impedance routing.
 */
#ifndef WUBU_JACKIMPEDANCE_H
#define WUBU_JACKIMPEDANCE_H

#include <stddef.h>

void wubu_jackimpedance_probe(void);
int  wubu_jackimpedance_present(void);
int  wubu_jackimpedance_headphone(void);
int  wubu_jackimpedance_mic(void);
int  wubu_jackimpedance_line(void);
int  wubu_jackimpedance_threshold(void);
const char *wubu_jackimpedance_driver(void);
const char *wubu_jackimpedance_type_for(const char *t);
const char *wubu_jackimpedance_device_for(const char *d);
int wubu_jackimpedance_summary(char *out, size_t cap);

#endif
