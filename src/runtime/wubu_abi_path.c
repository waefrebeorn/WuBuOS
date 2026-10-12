/*
 * wubu_abi_path.c -- foreign-ABI path translation (Track D1).
 *
 * The Styx translator model: a foreign app expects its OWN filesystem
 * layout -- Windows paths (C:\Users\...), Unix paths (/usr/bin/...), DOS
 * drive letters (C:\PROG\...). To run that app in the WuBu stack, a shim
 * must map those expectations onto the WuBu 9P namespace. This module is
 * that mapping: given a foreign path + its ABI, produce the namespace path
 * a 9P server would serve it from.
 *
 * Policy (the WuBu namespace convention):
 *   - a foreign app's root is mounted under /n/foreign/<abi>/<drive-or-root>
 *   - Windows 'C:\a\b'  -> /n/foreign/win/c/a/b   (backslashes -> slashes)
 *   - Unix    '/usr/bin' -> /n/foreign/unix/usr/bin
 *   - DOS     'C:\PROG'  -> /n/foreign/dos/c/PROG
 * The drive letter / leading slash is preserved as the first namespace
 * component so two drives never collide. Pure + dependency-light.
 *
 * C11, minimal includes.
 */

#include "wubu_abi_path.h"
#include <string.h>
#include <stdio.h>

/* Map a foreign path onto the WuBu namespace. Returns 0 on success (out
 * filled), -1 on bad args or overflow. abi selects the mount prefix. */
int wubu_abi_path_translate(wubu_abi_t abi, const char *foreign,
                            char *out, size_t cap) {
    if (!foreign || !out || cap == 0) return -1;
    const char *prefix;
    switch (abi) {
    case WUBU_ABI_WIN: prefix = "/n/foreign/win"; break;
    case WUBU_ABI_UNIX: prefix = "/n/foreign/unix"; break;
    case WUBU_ABI_DOS: prefix = "/n/foreign/dos"; break;
    default: return -1;
    }
    /* Skip a leading slash on Unix paths (the prefix supplies the root). */
    const char *p = foreign;
    if (abi == WUBU_ABI_UNIX && p[0] == '/') p++;
    /* Windows/DOS drive letter: C:\... -> drop the ':' and lowercase the
     * drive so it becomes a clean namespace component (c/...). */
    static char drive_stripped[1024];
    if ((abi == WUBU_ABI_WIN || abi == WUBU_ABI_DOS) &&
        ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) &&
        p[1] == ':') {
        char d = p[0];
        if (d >= 'A' && d <= 'Z') d = (char)(d - 'A' + 'a');  /* lowercase */
        drive_stripped[0] = d;
        snprintf(drive_stripped + 1, sizeof(drive_stripped) - 1, "%s", p + 2);
        p = drive_stripped;
    }

    int n = snprintf(out, cap, "%s/", prefix);
    if (n < 0 || (size_t)n >= cap) return -1;
    size_t off = (size_t)n;

    /* Copy the rest, translating backslashes to slashes and collapsing
     * repeats, so C:\a\b -> c/a/b and /usr//bin -> usr/bin. */
    int last_slash = 1;  /* suppress a leading duplicate after the prefix */
    for (const char *s = p; *s && off + 1 < cap; s++) {
        char c = *s;
        if (c == '\\') c = '/';
        if (c == '/') {
            if (last_slash) continue;   /* collapse // */
            last_slash = 1;
        } else {
            last_slash = 0;
        }
        out[off++] = c;
    }
    /* strip a trailing slash (unless it's the bare prefix) */
    while (off > 1 && out[off-1] == '/') off--;
    out[off] = '\0';
    return 0;
}

const char *wubu_abi_name(wubu_abi_t abi) {
    switch (abi) {
    case WUBU_ABI_WIN:  return "win";
    case WUBU_ABI_UNIX: return "unix";
    case WUBU_ABI_DOS:  return "dos";
    default: return "unknown";
    }
}
