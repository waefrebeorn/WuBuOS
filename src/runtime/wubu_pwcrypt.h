/* wubu_pwcrypt.h -- SHA-512 crypt ($6$), the WuBuOS password-hash primitive.
 *
 * Implements Ulrich Drepper's 5000-round $6$ scheme so first-boot user accounts
 * are stored in the same form a real shadow file (or omarchy, which uses
 * `openssl passwd -6`) accepts. See wubu_pwcrypt.c for the algorithm walk and
 * the reference vector.
 *
 * C11, no heap, thread-unsafe object-free (pure function).
 */
#ifndef WUBU_PWCRYPT_H
#define WUBU_PWCRYPT_H

/* Capacity for "$6$" + salt (<=16) + "$" + 86 hash chars + NUL. */
#define WUBU_PWCRYPT_OUT 128

/* Hash `password` with salt `salt` (up to 16 chars, stops at `$`) into `out`
 * in $6$ form. Returns `out` on success, NULL on invalid input (no salt, or a
 * NULL pointer). `salt` may be passed bare ("wubuSALT") or already prefixed
 * ("$6$wubuSALT$"): the prefix is stripped. */
const char *wubu_pw_crypt_sha512(const char *password, const char *salt,
                                 char out[WUBU_PWCRYPT_OUT]);

#endif /* WUBU_PWCRYPT_H */