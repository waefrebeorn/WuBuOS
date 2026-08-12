/*
 * wubu_dmcrypt.h -- kernel-owned storage dm-crypt/LUKS routing.
 */
#ifndef WUBU_DMCRYPT_H
#define WUBU_DMCRYPT_H

#include <stddef.h>

void wubu_dmcrypt_probe(void);
int  wubu_dmcrypt_present(void);
int  wubu_dmcrypt_luks(void);
int  wubu_dmcrypt_aes(void);
int  wubu_dmcrypt_xts(void);
int  wubu_dmcrypt_dm(void);
const char *wubu_dmcrypt_driver(void);
const char *wubu_dmcrypt_cipher_for(const char *c);
const char *wubu_dmcrypt_mode_for(const char *m);
int wubu_dmcrypt_summary(char *out, size_t cap);

#endif
