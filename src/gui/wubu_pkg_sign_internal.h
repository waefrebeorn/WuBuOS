/* Internal error-sink shared by the two package-signing leaves. */
#ifndef WUBU_PKG_SIGN_INTERNAL_H
#define WUBU_PKG_SIGN_INTERNAL_H
#include <stddef.h>
void wubu_pkg_sign_set_err(char *err, size_t errsz, const char *msg);
#endif
