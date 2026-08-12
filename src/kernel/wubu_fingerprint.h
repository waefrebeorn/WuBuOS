/*
 * wubu_fingerprint.h -- kernel-owned fingerprint/biometric routing.
 */
#ifndef WUBU_FINGERPRINT_H
#define WUBU_FINGERPRINT_H

#include <stddef.h>

/* W1: probe the fingerprint topology. */
void wubu_fingerprint_probe(void);

/* W2: accessors */
int  wubu_fingerprint_present(void);
int  wubu_fingerprint_goodix(void);
int  wubu_fingerprint_vfs(void);
int  wubu_fingerprint_egis(void);
int  wubu_fingerprint_authenc(void);
int  wubu_fingerprint_fpc(void);
const char *wubu_fingerprint_driver(void);
const char *wubu_fingerprint_vendor(void);

/* W3: fingerprint vendor driver routing. */
const char *wubu_fingerprint_vendor_driver(const char *vendor);

/* W4: summary fragment. */
int wubu_fingerprint_summary(char *out, size_t cap);

#endif /* WUBU_FINGERPRINT_H */
