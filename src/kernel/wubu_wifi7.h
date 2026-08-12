/*
 * wubu_wifi7.h -- kernel-owned Wi-Fi 7 (802.11be) / 6GHz driver routing.
 */
#ifndef WUBU_WIFI7_H
#define WUBU_WIFI7_H

#include <stddef.h>

/* W1: probe the Wi-Fi 7 topology. */
void wubu_wifi7_probe(void);

/* W2: accessors */
int  wubu_wifi7_present(void);
int  wubu_wifi7_mlo(void);      /* MLO (multi-link operation) */
int  wubu_wifi7_6ghz(void);     /* 6GHz band */
int  wubu_wifi7_320mhz(void);   /* 320MHz channel */
int  wubu_wifi7_vendor(void);
const char *wubu_wifi7_driver(void);
const char *wubu_wifi7_name(void);

/* W3: Wi-Fi 7 driver routing. */
const char *wubu_wifi7_driver_for(int vendor, int device);

/* W4: summary fragment. */
int wubu_wifi7_summary(char *out, size_t cap);

#endif /* WUBU_WIFI7_H */
