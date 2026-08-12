/*
 * wubu_usb4.h -- kernel-owned USB4/Thunderbolt driver routing.
 */
#ifndef WUBU_USB4_H
#define WUBU_USB4_H

#include <stddef.h>

/* W1: probe the USB4/Thunderbolt topology. */
void wubu_usb4_probe(void);

/* W2: accessors */
int  wubu_usb4_tb(void);
int  wubu_usb4_usb4(void);
int  wubu_usb4_bolt(void);
int  wubu_usb4_secure(void);
int  wubu_usb4_domains(void);
const char *wubu_usb4_driver(void);

/* W3: TB/USB4 driver routing. */
const char *wubu_usb4_driver_for(const char *host);

/* W4: summary fragment. */
int wubu_usb4_summary(char *out, size_t cap);

#endif /* WUBU_USB4_H */
