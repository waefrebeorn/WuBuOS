/*
 * wubu_guest_root.c -- see wubu_guest_root.h.
 *
 * Default is ~/.local/share/wubu/guest: inside the user's own state directory but
 * NOT their home directory, so the desktop and File Manager never display host
 * files.  $HOME itself is only used to build the default; it is not a source of
 * truth about where guest data lives.
 */

#include "wubu_guest_root.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void wubu_guest_root(char *out, size_t outsz)
{
    static char root[1024];
    static int ready = 0;

    if (!ready) {
        const char *env = getenv("WUBU_GUEST_ROOT");
        if (env && *env) {
            snprintf(root, sizeof(root), "%s", env);
        } else {
            const char *home = getenv("HOME");
            snprintf(root, sizeof(root), "%s/.local/share/wubu/guest",
                     home ? home : ".");
        }
        /* Store without a trailing slash so callers can append "/x" directly;
         * the root itself keeps its single leading slash. */
        size_t l = strlen(root);
        while (l > 1 && root[l - 1] == '/') root[--l] = '\0';
        ready = 1;
    }

    if (out && outsz) snprintf(out, outsz, "%s", root);
}
