/* Internal error-sink shared by the two package-signing leaves. */
#ifndef WUBU_PKG_SIGN_INTERNAL_H
#define WUBU_PKG_SIGN_INTERNAL_H
#include <stddef.h>
void wubu_pkg_sign_set_err(char *err, size_t errsz, const char *msg);
#ifdef WUBU_HAVE_OPENSSL_EDDSA
/* Count of successful native OpenSSL sign/verify operations in this process.
 * Opt-in builds only; the differential gate uses it to prove the native path
 * actually ran instead of silently falling back to the ladder. */
unsigned wubu_pkg_ed25519_native_ops(void);
#endif
#endif
