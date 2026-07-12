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

void styxfs_path_to_host(styxfs_server_t *srv, const char *path,
                                 char *out, size_t outsz) {
    if (!path) { out[0] = '\0'; return; }
    if (!srv) { snprintf(out, outsz, "%s", path); return; }

    styxfs_mount_t *best = NULL;
    size_t best_len = 0;
    for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
        size_t mlen = strlen(m->path);
        if (strncmp(path, m->path, mlen) == 0 &&
            (path[mlen] == '/' || path[mlen] == '\0')) {
            if (mlen > best_len) { best = m; best_len = mlen; }
        }
    }
    if (best) {
        const char *rest = path + best_len;
        if (*rest == '/') rest++;
        if (*rest == '\0') snprintf(out, outsz, "%s", best->source);
        else              snprintf(out, outsz, "%s/%s", best->source, rest);
    } else {
        snprintf(out, outsz, "%s", path);
    }
}

/* readdir - read directory entries (POSIX-style heap array of copies)
 * Returns the count of entries (>=0) or -1 on error. The caller owns both the
 * array and each struct dirent (free entries[i], then the array). */
int styxfs_readdir(const char *path, struct dirent ***entries) {
    if (!path || !entries) return -1;
    *entries = NULL;

    char host[STYXFS_MAX_PATH];
    styxfs_path_to_host(g_styxfs_server, path, host, sizeof(host));
    DIR *d = opendir(host);
    if (!d) return -1;

    struct dirent **arr = NULL;
    int cap = 0, count = 0, rc = -1;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (count >= cap) {
            int ncap = cap ? cap * 2 : 16;
            struct dirent **na = (struct dirent **)realloc(arr, (size_t)ncap * sizeof(*na));
            if (!na) goto cleanup;
            arr = na; cap = ncap;
        }
        size_t sz = sizeof(struct dirent) + strlen(de->d_name) + 1;
        struct dirent *copy = (struct dirent *)malloc(sz);
        if (!copy) goto cleanup;
        memcpy(copy, de, sizeof(struct dirent));
        strcpy(copy->d_name, de->d_name);
        arr[count++] = copy;
    }
    rc = count;

cleanup:
    closedir(d);
    if (rc < 0) {
        for (int i = 0; i < count; i++) free(arr[i]);
        free(arr);
        *entries = NULL;
        return -1;
    }
    *entries = arr;
    return count;
}

/* opendir - open a directory, returning a real host DIR* via mount mapping.
 * Works even without an active server (path is treated as a direct host path). */
DIR *styxfs_opendir(const char *path) {
    if (!path) return NULL;
    char host[STYXFS_MAX_PATH];
    styxfs_path_to_host(g_styxfs_server, path, host, sizeof(host));
    return opendir(host);
}

/* closedir - close a directory opened by styxfs_opendir */
int styxfs_closedir(DIR *dirp) {
    if (!dirp) return -1;
    return closedir(dirp);
}

/* readdir_r - reentrant readdir into a caller-provided buffer.
 * The buffer must be large enough to hold a dirent + d_name (NAME_MAX+1).
 * Returns 0 and sets *result to entry (or NULL at end-of-dir), -1 on error. */
int styxfs_readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result) {
    if (!dirp || !entry || !result) return -1;
    struct dirent *de = readdir(dirp);
    if (!de) { *result = NULL; return 0; }
    entry->d_ino    = de->d_ino;
    entry->d_off    = de->d_off;
    entry->d_reclen = de->d_reclen;
    entry->d_type   = de->d_type;
    memcpy(entry->d_name, de->d_name, strlen(de->d_name) + 1);
    *result = entry;
    return 0;
}
