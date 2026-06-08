/*
 * styxfs.c — StyxFS: 9P2000/Styx Filesystem for .wubu Containers
 *
 * Implements a real filesystem namespace backed by .wubu containers.
 * Each .wubu container is exposed as a file in the Styx namespace.
 * Mount points allow composing multiple container repositories.
 */

#include "styxfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* ── Internal Helpers ────────────────────────────────────────────── */

static uint64_t next_qid_path(styxfs_server_t *srv) {
    return ++srv->next_qid_path;
}

static styxfs_file_t *file_alloc(styxfs_server_t *srv) {
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (!srv->open_files[i].in_use) {
            memset(&srv->open_files[i], 0, sizeof(styxfs_file_t));
            srv->open_files[i].in_use = 1;
            return &srv->open_files[i];
        }
    }
    return NULL;
}

static styxfs_file_t *file_lookup(styxfs_server_t *srv, uint64_t qid_path) {
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == qid_path)
            return &srv->open_files[i];
    }
    return NULL;
}

static void file_free(styxfs_server_t *srv, uint64_t qid_path) {
    styxfs_file_t *f = file_lookup(srv, qid_path);
    if (f) {
        if (f->container_payload) free(f->container_payload);
        if (f->write_buf) free(f->write_buf);
        f->in_use = 0;
    }
}

/* Path normalization: ensure leading slash, no trailing slash (except root) */
static void normalize_path(char *path) {
    if (!path || !*path) { strcpy(path, "/"); return; }
    if (path[0] != '/') {
        char tmp[STYXFS_MAX_PATH];
        snprintf(tmp, sizeof(tmp), "/%s", path);
        strcpy(path, tmp);
    }
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
        len--;
    }
}

/* Check if path is a directory mount point */
static int is_mount_point(styxfs_server_t *srv, const char *path) {
    for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
        if (strcmp(m->path, path) == 0) return 1;
    }
    return 0
}