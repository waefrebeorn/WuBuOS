/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "styxfs.h"
#include "styxfs_internal.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void build_path(char *out, size_t out_size, const char *base, const char *name) {
    if (strcmp(base, "/") == 0) {
        snprintf(out, out_size, "/%s", name);
    } else {
        snprintf(out, out_size, "%s/%s", base, name);
    }
}

/* Helper: check if a path is a mount point or under one */
int path_is_mounted(styxfs_server_t *srv, const char *path) {
    for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
        size_t mlen = strlen(m->path);
        if (strncmp(path, m->path, mlen) == 0 &&
            (path[mlen] == '/' || path[mlen] == '\0')) {
            return 1;
        }
    }
    return 0;
}

/* Helper: count immediate children under a path (mount points) */
int count_children(styxfs_server_t *srv, const char *path) {
    int count = 0;
    size_t plen = strlen(path);
    for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
        /* Check if m->path is a direct child of path */
        if (strncmp(m->path, path, plen) == 0 && m->path[plen] == '/') {
            const char *rest = m->path + plen + 1;
            if (strchr(rest, '/') == NULL) count++;
        }
    }
    return count;
}
