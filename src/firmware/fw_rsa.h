/*
 * fw_rsa.h -- WuBuFW RSA-2048 signature verification (PKCS#1 v1.5).
 */
#ifndef WUBUFW_RSA_H
#define WUBUFW_RSA_H

#include <stdint.h>

/* Verify a 256-byte RSA-2048 PKCS#1 v1.5 signature over a SHA-256 hash.
 * n/e are 64 little-endian 32-bit limbs; sig is 256 big-endian bytes.
 * Returns 0 if the signature is valid, -1 otherwise. */
int fw_rsa2048_verify_pkcs1(const uint32_t n[64], const uint32_t e[64],
                            const uint8_t *hash32, const uint8_t sig[256]);

/* Boot-time self-test against a fixed openssl-generated vector. 0 = pass. */
int fw_rsa_selftest(void);
extern uint32_t g_fw_rsa_last_m[64];


#endif /* WUBUFW_RSA_H */
