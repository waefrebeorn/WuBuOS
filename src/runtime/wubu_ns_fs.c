/*
 * wubu_ns_fs.c -- WuBuOS Namespace Bridge: filesystem core.
 *
 * The shared /n tree primitives: the root handle (g_ns_root), the mkdir -p
 * and file-write helpers, and wubu_ns_bridge_create() that lays down /n/svc
 * and /n/bottles. Kept SEPARATE from wubu_ns_bridge.c (which holds the
 * svc/bottle dispatch and pulls in archd + bottles) so lightweight subtrees
 * like wubu_ns_snap.c can link only the FS core -- no archd/bottles needed.
 *
 * C11, minimal includes. Shared symbols declared in wubu_ns_bridge_internal.h.
 */

#include "wubu_ns_bridge_internal.h"
#include "wubu_archd.h"   /* WubuArchServiceState for wubu_ns_state_str */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/* The namespace root captured by wubu_ns_bridge_create(). Singleton per
 * process (one /n tree). Tests create a temp root. */
const char *g_ns_root = NULL;

/* Create a directory path under g_ns_root (mkdir -p, one component at a time). */
int ns_mkdir(const char *sub) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", g_ns_root, sub);
    char *p = path;
    if (p[0] == '/') p++;  /* skip leading slash if root is absolute */
    while (*p) {
        char *slash = strchr(p, '/');
        if (slash) *slash = '\0';
        if (mkdir(path, 0755) != 0 && errno != EEXIST) return -1;
        if (!slash) break;
        *slash = '/';
        p = slash + 1;
    }
    return 0;
}

/* Write a whole file under g_ns_root. */
int ns_write(const char *sub, const char *buf) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", g_ns_root, sub);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs(buf, f);
    fclose(f);
    return 0;
}

int wubu_ns_bridge_create(const char *ns_root) {
    if (!ns_root) return -1;
    struct stat st;
    if (stat(ns_root, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    g_ns_root = ns_root;
    if (ns_mkdir("svc")    != 0) return -1;
    if (ns_mkdir("bottles") != 0) return -1;
    return 0;
}

/* Service-state enum -> short string (active/inactive/...). Pure helper,
 * no deps, lives with the FS core so both bridge + snap can use it. */
const char *wubu_ns_state_str(WubuArchServiceState state) {
    switch (state) {
        case SERVICE_STATE_RUNNING:   return "active";
        case SERVICE_STATE_ENABLED:    return "enabled";
        case SERVICE_STATE_DISABLED:   return "inactive";
        case SERVICE_STATE_RESTARTING: return "restarting";
        case SERVICE_STATE_FAILED:     return "failed";
        default:                       return "unknown";
    }
}
