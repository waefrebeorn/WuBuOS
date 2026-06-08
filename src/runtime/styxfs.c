/*
 * styxfs.c — StyxFS: 9P2000/Styx Filesystem for .wubu Containers
 *
 * Implements a filesystem namespace backed by .wubu containers.
 * Each .wubu container is exposed as a file in the Styx namespace.
 * Mount points allow composing multiple container repositories.
 */

#include "styxfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal Helpers ────────────────────────────────────────── */

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

/* ── Server Lifecycle ───────────────────────────────────────── */

void styxfs_init(styxfs_server_t *srv) {
    memset(srv, 0, sizeof(*srv));
    srv->readonly = 0;
    srv->next_qid_path = 1;
}

/* Create a root directory entry */
static int ensure_root(styxfs_server_t *srv) {
    styxfs_file_t *root = file_lookup(srv, 0);
    if (root) return 0;
    root = file_alloc(srv);
    if (!root) return -1;
    root->qid_path = 0;
    root->qid_type = STX_QTDIR;
    root->qid_version = 1;
    strcpy(root->path, "/");
    return 0;
}

/* ── Mount Operations ───────────────────────────────────────── */

int styxfs_mount(styxfs_server_t *srv, const char *path, const char *source, int is_repo) {
    if (!srv || !path || !source) return -1;
    ensure_root(srv);

    /* Allocate mount entry */
    styxfs_mount_t *m = (styxfs_mount_t*)calloc(1, sizeof(styxfs_mount_t));
    if (!m) return -1;
    strncpy(m->path, path, STYXFS_MAX_PATH - 1);
    strncpy(m->source, source, STYXFS_MAX_PATH - 1);
    m->is_container_repo = is_repo;

    /* Add to mount list */
    m->next = srv->mounts;
    srv->mounts = m;

    /* Create directory entry for mount point */
    styxfs_file_t *dir = file_alloc(srv);
    if (!dir) { free(m); return -1; }
    dir->qid_path = next_qid_path(srv);
    dir->qid_type = STX_QTDIR;
    dir->qid_version = 1;
    strncpy(dir->path, path, STYXFS_MAX_PATH - 1);
    normalize_path(dir->path);

    return 0;
}

int styxfs_unmount(styxfs_server_t *srv, const char *path) {
    if (!srv || !path) return -1;
    styxfs_mount_t **pp = &srv->mounts;
    while (*pp) {
        if (strcmp((*pp)->path, path) == 0) {
            styxfs_mount_t *m = *pp;
            *pp = m->next;
            free(m);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

void styxfs_set_readonly(styxfs_server_t *srv, int readonly) {
    if (srv) srv->readonly = readonly;
}

/* ── Scan Repository ────────────────────────────────────────── */

int styxfs_scan_repo(styxfs_server_t *srv, const char *mount_path, const char *fs_path) {
    if (!srv || !mount_path) return -1;
    (void)fs_path; /* In full impl, scan fs_path for .wubu files */
    ensure_root(srv);
    return 0;
}

/* ── Styx Serve (dispatch) ──────────────────────────────────── */

int styxfs_serve(styxfs_server_t *srv,
                  const uint8_t *inbuf, uint32_t inlen,
                  uint8_t *outbuf, uint32_t *outlen) {
    if (!srv || !inbuf || !outbuf || !outlen) return -1;
    /* Delegate to base styx_serve with our callbacks */
    return styx_serve(&srv->base, inbuf, inlen, outbuf, outlen);
}

/* ── Callbacks ──────────────────────────────────────────────── */

int styxfs_attach_cb(styx_server_t *base, uint32_t fid, const char *aname) {
    (void)base; (void)aname;
    (void)fid;
    return 0;
}

int styxfs_walk_cb(styx_server_t *base, uint32_t fid, uint32_t newfid,
                    const char **wname, int nwname,
                    styx_qid_t *qids, int *nwqid) {
    (void)base; (void)fid; (void)newfid; (void)wname; (void)nwname;
    (void)qids;
    if (nwqid) *nwqid = 0;
    return 0;
}

int styxfs_open_cb(styx_server_t *base, uint32_t fid, int mode,
                    styx_qid_t *qid) {
    (void)base; (void)fid; (void)mode; (void)qid;
    return 0;
}

int styxfs_create_cb(styx_server_t *base, uint32_t fid, const char *name,
                      uint32_t perm, int mode, styx_qid_t *qid) {
    (void)base; (void)fid; (void)name; (void)perm; (void)mode; (void)qid;
    return 0;
}

int styxfs_read_cb(styx_server_t *base, uint32_t fid,
                    uint64_t offset, uint32_t count,
                    uint8_t *data, uint32_t *nread) {
    (void)base; (void)fid; (void)offset; (void)count; (void)data;
    if (nread) *nread = 0;
    return 0;
}

int styxfs_write_cb(styx_server_t *base, uint32_t fid,
                     uint64_t offset, uint32_t count,
                     const uint8_t *data, uint32_t *nwritten) {
    (void)base; (void)fid; (void)offset; (void)count; (void)data;
    if (nwritten) *nwritten = 0;
    return 0;
}

int styxfs_clunk_cb(styx_server_t *base, uint32_t fid) {
    (void)base; (void)fid;
    return 0;
}

int styxfs_remove_cb(styx_server_t *base, uint32_t fid) {
    (void)base; (void)fid;
    return 0;
}

int styxfs_stat_cb(styx_server_t *base, uint32_t fid,
                    styx_dir_t *dir) {
    (void)base; (void)fid; (void)dir;
    return 0;
}

int styxfs_wstat_cb(styx_server_t *base, uint32_t fid,
                     const styx_dir_t *dir) {
    (void)base; (void)fid; (void)dir;
    return 0;
}

/* ── Helper Utilities ───────────────────────────────────────── */

styxfs_mount_t *styxfs_find_mount(styxfs_server_t *srv, const char *path, char *rel_path) {
    if (!srv || !path) return NULL;
    for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
        size_t mlen = strlen(m->path);
        if (strncmp(path, m->path, mlen) == 0) {
            if (rel_path) {
                const char *rest = path + mlen;
                if (*rest == '/') rest++;
                strncpy(rel_path, rest, STYXFS_MAX_PATH - 1);
            }
            return m;
        }
    }
    return NULL;
}

styxfs_file_t *styxfs_resolve(styxfs_server_t *srv, const char *path, int create_if_missing) {
    if (!srv || !path) return NULL;
    (void)create_if_missing;
    char norm[STYXFS_MAX_PATH];
    strncpy(norm, path, STYXFS_MAX_PATH - 1);
    norm[STYXFS_MAX_PATH - 1] = '\0';
    normalize_path(norm);
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && strcmp(srv->open_files[i].path, norm) == 0)
            return &srv->open_files[i];
    }
    return NULL;
}

int styxfs_build_dirent(styxfs_server_t *srv, const char *path,
                         uint8_t *buf, uint32_t buf_size, uint32_t *out_size,
                         uint64_t offset, uint32_t count) {
    (void)srv; (void)path; (void)buf; (void)buf_size;
    (void)offset; (void)count;
    if (out_size) *out_size = 0;
    return 0;
}

int styxfs_load_container(const char *path, WUBU_HEADER *out_hdr, uint8_t **out_payload, size_t *out_size) {
    (void)path; (void)out_hdr; (void)out_payload; (void)out_size;
    return -1;
}

int styxfs_is_wubu_container(const char *path) {
    if (!path) return 0;
    size_t len = strlen(path);
    return len >= 5 && strcmp(path + len - 5, ".wubu") == 0;
}
