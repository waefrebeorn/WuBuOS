/*
 * styxfs_callbacks.c -- StyxFS 9P2000/Styx callback handlers.
 *
 * These are the filesystem callbacks installed by styxfs_init() into the
 * embedded styx_server_t. They implement a HOST-FILE-BACKED namespace: every
 * 9P path resolves (via the mount table) to a real file under a mounted
 * host directory, and reads/writes go straight to that file. This is exactly
 * what the /n control plane needs -- the namespace bridge writes real files
 * onto disk and a 9P client reads/writes the same bytes.
 *
 * CRITICAL: styx_serve() (styx.c) tracks fids in the EMBEDDED base.fids[]
 * table via styx_fid_alloc/styx_fid_lookup. These callbacks MUST use that
 * same table and stash the resolved path in fid->file_state. (An earlier
 * version populated a separate srv->open_files[] table, so styx_serve could
 * never find the fids -> every op returned "Bad fid". Fixed here.)
 *
 * C11, opaque-safe, minimal includes, no god headers.
 */

#define _GNU_SOURCE
#include "styxfs.h"
#include "styxfs_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* -- Helpers ------------------------------------------------ */

styxfs_server_t *styxfs_get_server(styx_server_t *base) {
    /* base is the first member of styxfs_server_t */
    return (styxfs_server_t *)base;
}

/* Resolve the absolute 9P path for a fid (file_state holds it; NULL = root). */
static const char *fid_path(styx_fid_t *f) {
    if (!f || !f->file_state) return "/";
    return (const char *)f->file_state;
}

/* Free a fid's stored path (call on clunk). */
static void fid_free_path(styx_fid_t *f) {
    if (f && f->file_state) { free(f->file_state); f->file_state = NULL; }
}

/* -- Attach ------------------------------------------------ */

int styxfs_attach_cb(styx_server_t *base, uint32_t fid, const char *aname) {
    (void)aname;
    styxfs_server_t *srv = styxfs_get_server(base);
    styx_fid_t *f = styx_fid_alloc(base, fid);
    if (!f) return -1;
    f->qid.type  = STX_QTDIR;
    f->qid.version = 1;
    f->qid.path = 1;            /* root */
    f->file_state = NULL;       /* root has no stored subpath */
    (void)srv;
    return 0;
}

/* -- Walk -------------------------------------------------- */

int styxfs_walk_cb(styx_server_t *base, uint32_t fid, uint32_t newfid,
                    const char **wname, int nwname,
                    styx_qid_t *qids, int *nwqid) {
    styxfs_server_t *srv = styxfs_get_server(base);
    *nwqid = 0;

    styx_fid_t *f = styx_fid_lookup(base, fid);
    if (!f) return -1;

    char cur[STYXFS_MAX_PATH];
    snprintf(cur, sizeof(cur), "%s", fid_path(f));

    int nqid = 0;
    for (int i = 0; i < nwname && i < 16; i++) {
        char next[STYXFS_MAX_PATH];
        build_path(next, sizeof(next), cur, wname[i]);

        /* A path is reachable if it is (a) the root, (b) a mount point,
         * (c) under a mount, or (d) an existing host file/dir under one. */
        int reachable = 0;
        if (strcmp(next, "/") == 0) reachable = 1;
        if (!reachable && path_is_mounted(srv, next)) reachable = 1;
        if (!reachable) {
            char host[STYXFS_MAX_PATH];
            styxfs_path_to_host(srv, next, host, sizeof(host));
            struct stat st;
            if (stat(host, &st) == 0) reachable = 1;
        }
        if (!reachable) break;  /* walk stops at the first missing component */

        /* qid type: a directory if the path is an exact mount point or a
         * real host directory; otherwise a plain file. NOTE: the root mount
         * "/" covers the whole namespace for reachability, but a subpath like
         * /snap/.../list is NOT itself a mount point, so it must be QTFILE. */
        int exact_mount = 0;
        for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
            if (strcmp(m->path, next) == 0) { exact_mount = 1; break; }
        }
        int host_is_dir = 0;
        if (!exact_mount) {
            char h2[STYXFS_MAX_PATH];
            styxfs_path_to_host(srv, next, h2, sizeof(h2));
            struct stat st2;
            if (stat(h2, &st2) == 0 && S_ISDIR(st2.st_mode)) host_is_dir = 1;
        }
        qids[nqid].type    = (exact_mount || host_is_dir) ? STX_QTDIR : STX_QTFILE;
        qids[nqid].version = 1;
        qids[nqid].path    = (uint64_t)(++srv->next_qid_path);
        nqid++;

        snprintf(cur, sizeof(cur), "%s", next);
    }

    if (nqid > 0) {
        styx_fid_t *nf = styx_fid_alloc(base, newfid);
        if (!nf) return -1;
        nf->qid      = qids[nqid - 1];
        nf->file_state = strdup(cur);
    }

    *nwqid = nqid;
    return 0;
}

/* -- Open -------------------------------------------------- */

int styxfs_open_cb(styx_server_t *base, uint32_t fid, int mode,
                    styx_qid_t *qid) {
    styxfs_server_t *srv = styxfs_get_server(base);
    (void)srv;
    styx_fid_t *f = styx_fid_lookup(base, fid);
    if (!f) return -1;
    f->open_mode = mode;
    f->offset    = 0;
    *qid = f->qid;
    return 0;
}

/* -- Read -------------------------------------------------- */

int styxfs_read_cb(styx_server_t *base, uint32_t fid,
                    uint64_t offset, uint32_t count,
                    uint8_t *data, uint32_t *nread) {
    styxfs_server_t *srv = styxfs_get_server(base);
    *nread = 0;
    styx_fid_t *f = styx_fid_lookup(base, fid);
    if (!f) return -1;

    const char *path = fid_path(f);

    /* Directory: list immediate children (mount points + host entries). */
    if (f->qid.type == STX_QTDIR) {
        uint32_t buf_off = 0;
        uint64_t dir_offset = offset;
        int entry_count = 0;

        /* Mount points that are direct children of `path`. */
        for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
            size_t plen = strlen(path);
            if (strncmp(m->path, path, plen) != 0) continue;
            if (path[0] == '/' && m->path[0] == '/' && plen == 1) {
                /* Root: every mount is a direct child. */
            } else if (m->path[plen] != '/') {
                continue;
            }
            const char *rest = m->path + plen + (plen == 1 ? 0 : 0);
            /* child name = first component after path */
            const char *name = m->path + (plen == 1 ? 1 : plen + 1);
            const char *slash = strchr(name, '/');
            size_t name_len = slash ? (size_t)(slash - name) : strlen(name);
            if (name_len == 0) continue;

            /* skip entries before offset (simple paging) */
            if ((uint64_t)entry_count < dir_offset) { entry_count++; continue; }

            uint32_t need = (uint32_t)(2 + 4 + 8 + 4 + 4 + 4 + 8 + 2 + name_len);
            if (buf_off + need > count) break;

            styx_dir_t dir;
            memset(&dir, 0, sizeof(dir));
            dir.type = 0; dir.dev = 0;
            dir.qid.type = STX_QTDIR; dir.qid.version = 1;
            dir.qid.path = (uint64_t)(entry_count + 100);
            dir.mode = 040755;
            dir.atime = (uint32_t)time(NULL);
            dir.mtime = dir.atime;
            dir.length = 0;
            memcpy(dir.name, name, name_len); dir.name[name_len] = '\0';
            strcpy(dir.uid, "wubu"); strcpy(dir.gid, "wubu"); strcpy(dir.muid, "wubu");

            uint32_t dir_size_pos = buf_off;
            buf_off += 2;
            uint32_t dir_start = buf_off;
            styx_put16(data + buf_off, dir.type); buf_off += 2;
            styx_put32(data + buf_off, dir.dev); buf_off += 4;
            data[buf_off++] = dir.qid.type;
            styx_put32(data + buf_off, dir.qid.version); buf_off += 4;
            styx_put64(data + buf_off, dir.qid.path); buf_off += 8;
            styx_put32(data + buf_off, dir.mode); buf_off += 4;
            styx_put32(data + buf_off, dir.atime); buf_off += 4;
            styx_put32(data + buf_off, dir.mtime); buf_off += 4;
            styx_put64(data + buf_off, dir.length); buf_off += 8;
            buf_off += styx_putstr(data + buf_off, dir.name);
            buf_off += styx_putstr(data + buf_off, dir.uid);
            buf_off += styx_putstr(data + buf_off, dir.gid);
            buf_off += styx_putstr(data + buf_off, dir.muid);
            uint32_t dir_total = buf_off - dir_start;
            styx_put16(data + dir_size_pos, (uint16_t)dir_total);
            entry_count++;
        }

        /* Host directory entries (if `path` maps to a real host dir). */
        char host[STYXFS_MAX_PATH];
        styxfs_path_to_host(srv, path, host, sizeof(host));
        DIR *d = opendir(host);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                    continue;
                if ((uint64_t)entry_count < dir_offset) { entry_count++; continue; }
                uint32_t need = (uint32_t)(2 + 4 + 8 + 4 + 4 + 4 + 8 + 2 + strlen(de->d_name));
                if (buf_off + need > count) break;
                styx_dir_t dir;
                memset(&dir, 0, sizeof(dir));
                dir.type = 0; dir.dev = 0;
                dir.qid.type = (de->d_type == DT_DIR) ? STX_QTDIR : STX_QTFILE;
                dir.qid.version = 1;
                dir.qid.path = (uint64_t)(entry_count + 100);
                dir.mode = (de->d_type == DT_DIR) ? 040755 : 0100644;
                dir.atime = (uint32_t)time(NULL);
                dir.mtime = dir.atime;
                dir.length = 0;
                strncpy(dir.name, de->d_name, sizeof(dir.name) - 1);
                strcpy(dir.uid, "wubu"); strcpy(dir.gid, "wubu"); strcpy(dir.muid, "wubu");
                uint32_t dir_size_pos = buf_off;
                buf_off += 2;
                uint32_t dir_start = buf_off;
                styx_put16(data + buf_off, dir.type); buf_off += 2;
                styx_put32(data + buf_off, dir.dev); buf_off += 4;
                data[buf_off++] = dir.qid.type;
                styx_put32(data + buf_off, dir.qid.version); buf_off += 4;
                styx_put64(data + buf_off, dir.qid.path); buf_off += 8;
                styx_put32(data + buf_off, dir.mode); buf_off += 4;
                styx_put32(data + buf_off, dir.atime); buf_off += 4;
                styx_put32(data + buf_off, dir.mtime); buf_off += 4;
                styx_put64(data + buf_off, dir.length); buf_off += 8;
                buf_off += styx_putstr(data + buf_off, dir.name);
                buf_off += styx_putstr(data + buf_off, dir.uid);
                buf_off += styx_putstr(data + buf_off, dir.gid);
                buf_off += styx_putstr(data + buf_off, dir.muid);
                uint32_t dir_total = buf_off - dir_start;
                styx_put16(data + dir_size_pos, (uint16_t)dir_total);
                entry_count++;
            }
            closedir(d);
        }

        *nread = buf_off;
        return 0;
    }

    /* Regular file: read the backing host file so /n is genuinely readable. */
    char host[STYXFS_MAX_PATH];
    styxfs_path_to_host(srv, path, host, sizeof(host));
    int fd = open(host, O_RDONLY);
    if (fd < 0) return 0;   /* absent file reads as empty, not error */
    ssize_t got = pread(fd, data, count, (off_t)offset);
    close(fd);
    *nread = (got < 0) ? 0 : (uint32_t)got;
    return 0;
}

/* -- Write ------------------------------------------------- */

int styxfs_write_cb(styx_server_t *base, uint32_t fid,
                     uint64_t offset, uint32_t count,
                     const uint8_t *data, uint32_t *nwritten) {
    styxfs_server_t *srv = styxfs_get_server(base);
    *nwritten = 0;
    if (srv->readonly) return -1;

    styx_fid_t *f = styx_fid_lookup(base, fid);
    if (!f) return -1;

    char host[STYXFS_MAX_PATH];
    styxfs_path_to_host(srv, fid_path(f), host, sizeof(host));
    /* A control-plane write must replace the file's content (echo > file),
     * not append -- O_TRUNC. */
    int fd = open(host, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    ssize_t done = (count > 0) ? pwrite(fd, data, count, (off_t)offset) : 0;
    close(fd);
    if (done < 0) return -1;
    *nwritten = (uint32_t)done;
    return 0;
}

/* -- Clunk / Remove ---------------------------------------- */

int styxfs_clunk_cb(styx_server_t *base, uint32_t fid) {
    styx_fid_t *f = styx_fid_lookup(base, fid);
    fid_free_path(f);
    return 0;   /* base styx_serve frees the fid slot itself */
}

int styxfs_remove_cb(styx_server_t *base, uint32_t fid) {
    styxfs_server_t *srv = styxfs_get_server(base);
    styx_fid_t *f = styx_fid_lookup(base, fid);
    if (!f) return -1;
    if (srv->readonly) return -1;
    char host[STYXFS_MAX_PATH];
    styxfs_path_to_host(srv, fid_path(f), host, sizeof(host));
    int rc = unlink(host);
    if (rc != 0 && errno == EISDIR) rc = rmdir(host);
    fid_free_path(f);
    return rc == 0 ? 0 : -1;
}

/* -- Stat -------------------------------------------------- */

int styxfs_stat_cb(styx_server_t *base, uint32_t fid, styx_dir_t *dir) {
    styxfs_server_t *srv = styxfs_get_server(base);
    memset(dir, 0, sizeof(*dir));
    styx_fid_t *f = styx_fid_lookup(base, fid);
    if (!f) return -1;

    dir->type = 0; dir->dev = 0;
    dir->qid = f->qid;

    if (f->qid.type == STX_QTDIR) {
        dir->mode = 040755; dir->length = 0;
    } else {
        dir->mode = 0100644;
        char host[STYXFS_MAX_PATH];
        styxfs_path_to_host(srv, fid_path(f), host, sizeof(host));
        struct stat st;
        if (stat(host, &st) == 0) dir->length = (uint64_t)st.st_size;
    }
    dir->atime = (uint32_t)time(NULL);
    dir->mtime = dir->atime;
    const char *path = fid_path(f);
    const char *last = strrchr(path, '/');
    strncpy(dir->name, last ? last + 1 : path, sizeof(dir->name) - 1);
    strcpy(dir->uid, "wubu"); strcpy(dir->gid, "wubu"); strcpy(dir->muid, "wubu");
    return 0;
}

/* -- Wstat ------------------------------------------------- */

int styxfs_wstat_cb(styx_server_t *base, uint32_t fid, const styx_dir_t *dir) {
    styxfs_server_t *srv = styxfs_get_server(base);
    if (srv->readonly) return -1;
    styx_fid_t *f = styx_fid_lookup(base, fid);
    if (!f) return -1;

    if (dir->mode != 0xFFFFFFFF) {
        char host[STYXFS_MAX_PATH];
        styxfs_path_to_host(srv, fid_path(f), host, sizeof(host));
        int mode = dir->mode & 0777;
        if (chmod(host, mode) != 0) return -1;
    }
    if (dir->mtime != 0xFFFFFFFF || dir->atime != 0xFFFFFFFF) {
        char host[STYXFS_MAX_PATH];
        styxfs_path_to_host(srv, fid_path(f), host, sizeof(host));
        struct timespec ts[2];
        ts[0].tv_sec = (dir->atime != 0xFFFFFFFF) ? dir->atime : 0;
        ts[0].tv_nsec = 0;
        ts[1].tv_sec = (dir->mtime != 0xFFFFFFFF) ? dir->mtime : 0;
        ts[1].tv_nsec = 0;
        if (utimensat(AT_FDCWD, host, ts, 0) != 0) return -1;
    }
    return 0;
}
