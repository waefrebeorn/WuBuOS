/*
 * wubu_hidadv.h -- kernel-owned USB HID advanced driver routing.
 */
#ifndef WUBU_HIDADV_H
#define WUBU_HIDADV_H

#include <stddef.h>

/* W1: probe the HID topology. */
void wubu_hidadv_probe(void);

/* W2: accessors */
int  wubu_hidadv_present(void);
int  wubu_hidadv_generic(void);
int  wubu_hidadv_multitouch(void);
int  wubu_hidadv_ff(void);
int  wubu_hidadv_vendor(void);
const char *wubu_hidadv_driver(void);

/* W3: HID driver routing. */
const char *wubu_hidadv_driver_for(const char *vendor);

/* W4: summary fragment. */
int wubu_hidadv_summary(char *out, size_t cap);

#endif /* WUBU_HIDADV_H */
