/*
 * wubu_ns_resolve.c -- resolve a /n/... path to its real location on disk.
 *
 * The /n namespace is a real directory tree under g_ns_root (ns_write does
 * fopen(g_ns_root/sub)). But a shell or tool handed "/n/bonzi/enabled"
 * treats it as a literal host path, which doesn't exist -- the tree lives
 * at g_ns_root. This module maps a /n/... argument to g_ns_root/... so any
 * /n-aware builtin (cat, ls, ...) reads the LIVE namespace tree, closing
 * the "ls /n shows the whole machine" loop from tested to usable.
 *
 * A path not under /n is returned unchanged (it's a real host path). If the
 * namespace root isn't set, /n paths resolve to NULL (the caller reports
 * "namespace not mounted"). Pure + testable.
 *
 * C11, minimal includes.
 */

#include "wubu_ns_resolve.h"
#include "wubu_ns_bridge_internal.h"   /* g_ns_root */
#include <string.h>
#include <stdio.h>

/* Resolve a /n/... path to its on-disk location under g_ns_root.
 * Returns 0 and fills out on success; returns -1 if the path isn't under /n
 * (out gets the path unchanged, so callers can still use it) or if /n is
 * given but the namespace root isn't mounted (out = ""). */
int wubu_ns_resolve(const char *path, char *out, size_t cap) {
    if (!path || !out || cap == 0) return -1;
    /* Not a /n path -> pass through unchanged (a real host path). */
    if (strncmp(path, "/n/", 3) != 0 && strcmp(path, "/n") != 0) {
        snprintf(out, cap, "%s", path);
        return 0;
    }
    /* A /n path but the namespace isn't mounted. */
    if (!g_ns_root) { out[0] = '\0'; return -1; }
    /* /n/bonzi/enabled -> <g_ns_root>/bonzi/enabled */
    const char *sub = path + 2;   /* skip "/n", keep the leading slash */
    if (*sub == '/') sub++;       /* -> "bonzi/enabled" */
    snprintf(out, cap, "%s/%s", g_ns_root, sub);
    return 0;
}

/* Whether a path is a /n namespace path. */
int wubu_ns_is_ns_path(const char *path) {
    return path && (strncmp(path, "/n/", 3) == 0 || strcmp(path, "/n") == 0);
}
