/*
 * wubu_securekey.h -- kernel-owned security key / TOTP / TPM routing.
 */
#ifndef WUBU_SECUREKEY_H
#define WUBU_SECUREKEY_H

#include <stddef.h>

/* W1: probe the security topology. */
void wubu_securekey_probe(void);

/* W2: accessors */
int  wubu_securekey_fido(void);
int  wubu_securekey_ccid(void);
int  wubu_securekey_tpm(void);
int  wubu_securekey_present(void);
const char *wubu_securekey_driver(void);
const char *wubu_securekey_type(void);

/* W3: driver routing. */
const char *wubu_securekey_driver_for(const char *dev);

/* W4: summary fragment. */
int wubu_securekey_summary(char *out, size_t cap);

#endif /* WUBU_SECUREKEY_H */
