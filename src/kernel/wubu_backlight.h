/*
 * wubu_backlight.h -- kernel-owned display backlight + NIC WoL routing.
 */
#ifndef WUBU_BACKLIGHT_H
#define WUBU_BACKLIGHT_H

#include <stddef.h>

/* W1: probe the backlight/WoL topology. */
void wubu_backlight_probe(void);

/* W2: accessors */
int  wubu_backlight_present(void);
int  wubu_backlight_acpi(void);
int  wubu_backlight_native(void);
int  wubu_backlight_wol(void);
int  wubu_backlight_wol_magic(void);
const char *wubu_backlight_driver(void);

/* W3: routing. */
const char *wubu_backlight_driver_for(const char *dev);
const char *wubu_backlight_wol_for(const char *wol);

/* W4: summary fragment. */
int wubu_backlight_summary(char *out, size_t cap);

#endif /* WUBU_BACKLIGHT_H */
