/*
 * wubu_trim.h -- kernel-owned storage TRIM + USB-C alt mode routing.
 */
#ifndef WUBU_TRIM_H
#define WUBU_TRIM_H

#include <stddef.h>

/* W1: probe the TRIM/alt-mode topology. */
void wubu_trim_probe(void);

/* W2: accessors */
int  wubu_trim_supported(void);
int  wubu_trim_fstrim(void);
int  wubu_trim_discard(void);
int  wubu_trim_altmode(void);
int  wubu_trim_thunderbolt(void);
const char *wubu_trim_driver(void);

/* W3: TRIM/alt-mode routing. */
const char *wubu_trim_mode_for(const char *fs);
const char *wubu_trim_altmode_for(const char *mode);

/* W4: summary fragment. */
int wubu_trim_summary(char *out, size_t cap);

#endif /* WUBU_TRIM_H */
