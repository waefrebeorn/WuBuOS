/*
 * wubu_abi_path.h -- foreign-ABI path translation (Track D1).
 *
 * Maps a foreign app's filesystem expectations (Windows C:\, Unix /usr,
 * DOS drive letters) onto the WuBu 9P namespace so a shim can serve any
 * foreign OS app from /n/foreign/<abi>/... . Pure + dependency-light.
 */
#ifndef WUBU_ABI_PATH_H
#define WUBU_ABI_PATH_H

#include <stddef.h>

typedef enum {
    WUBU_ABI_WIN = 0,   /* Windows PE (C:\... -> /n/foreign/win/c/...) */
    WUBU_ABI_UNIX,      /* Unix (/usr/... -> /n/foreign/unix/usr/...) */
    WUBU_ABI_DOS        /* DOS 16-bit (C:\... -> /n/foreign/dos/c/...) */
} wubu_abi_t;

/* Map a foreign path onto the WuBu namespace. Returns 0 on success (out
 * filled), -1 on bad args or overflow. Backslashes become slashes; the
 * drive letter / leading root is preserved as the first namespace
 * component so two drives never collide. */
int wubu_abi_path_translate(wubu_abi_t abi, const char *foreign,
                            char *out, size_t cap);

/* "win" / "unix" / "dos" / "unknown" */
const char *wubu_abi_name(wubu_abi_t abi);

#endif /* WUBU_ABI_PATH_H */
