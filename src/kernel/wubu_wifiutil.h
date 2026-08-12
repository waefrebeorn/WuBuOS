/*
 * wubu_wifiutil.h -- kernel-owned WiFi channel utilization routing.
 */
#ifndef WUBU_WIFIUTIL_H
#define WUBU_WIFIUTIL_H

#include <stddef.h>

void wubu_wifiutil_probe(void);
int  wubu_wifiutil_present(void);
int  wubu_wifiutil_cca(void);
int  wubu_wifiutil_airtime(void);
int  wubu_wifiutil_survey(void);
int  wubu_wifiutil_chan(void);
const char *wubu_wifiutil_driver(void);
const char *wubu_wifiutil_band_for(const char *b);
const char *wubu_wifiutil_state_for(const char *s);
int wubu_wifiutil_summary(char *out, size_t cap);

#endif
