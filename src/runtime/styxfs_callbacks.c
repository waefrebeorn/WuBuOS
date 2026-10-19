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

/* d_type / DT_DIR come from dirent and are hidden by the build's
 * -D_POSIX_C_SOURCE=200809L (glibc gates them behind _DEFAULT_SOURCE) —
 * a legitimate feature-surface use, kept localized. */
#ifndef WUBU_HOSTED
#define WUBU_HOSTED  /* no-op when already set via -DWUBU_HOSTED */
#endif
#include "wubu_gnu_compat.h"
#include "styxfs.h"
#include "styxfs_internal.h"
#include "wubu_std.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include "wubu_ftw.h"
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
    styxfs_server_t *srv = styxfs_get_server(base);
    (void)srv;

    /* aname roots the session: attach "/n/kv" and every walk is relative to it. */
    const char *root = (aname && *aname) ? aname : "/";
    if (root[0] != '/') { errno = EINVAL; return -1; }

    styx_fid_t *f = styx_fid_alloc(base, fid);
    if (!f) return -1;

    size_t n = strlen(root);
    char *path = malloc(n + 1);
    if (!path) { styx_fid_free(base, fid); return -1; }
    memcpy(path, root, n + 1);
    while (n > 1 && path[n - 1] == '/') path[--n] = '\0';

    f->qid.type  = STX_QTDIR;
    f->qid.version = 1;
    f->qid.path = 1 + (uint64_t)(unsigned char)path[n ? n - 1 : 0];
    f->file_state = path;
    return 0;
}

int styxfs_lock_cb(styx_server_t *base, uint32_t fid, int32_t owner,
                   uint64_t start, uint64_t length, int type) {
    styxfs_server_t *srv = styxfs_get_server(base);
    styx_fid_t *f = styx_fid_lookup(base, fid);
    if (!f) return -1;
    uint64_t qid = (uint64_t)(uintptr_t)f->file_state ? f->qid.path : f->qid.path;
    if (type == STYX_LOCK_UNLCK)
        return styx_unlock(&srv->locks, qid, owner, start, length);
    return styx_lock(&srv->locks, qid, owner, start, length, type);
}

/* A server with no auth phase must reject Tauth instead of handing out a fid. */
int styxfs_auth_cb(styx_server_t *base, uint32_t fid, const char *aname,
                   const char *uname, styx_qid_t *qid) {
    (void)base; (void)fid; (void)aname; (void)uname; (void)qid;
    errno = ENOTSUP;
    return -1;
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
        nf->file_state = wubu_strdup(cur);
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

/* -- Dir record encoding ----------------------------------- */

uint32_t styxfs_dir_encode(uint8_t *out, size_t cap, const char *name,
                           int is_dir, uint64_t qidpath) {
    if (!out || !name) return 0;
    size_t nl = strlen(name);
    if (nl > 255) return 0;

    uint32_t need = (uint32_t)(41 + 2 + nl + 3 * (2 + 4));
    if (need > cap) return 0;

    uint32_t now = (uint32_t)time(NULL);
    styx_put16(out + 0, (uint16_t)(need - 2));   /* size, excluding itself */
    styx_put16(out + 2, 0);                      /* type */
    styx_put32(out + 4, 0);                      /* dev  */
    out[8] = (uint8_t)(is_dir ? STX_QTDIR : STX_QTFILE);
    styx_put32(out + 9, 1);                      /* qid.version */
    styx_put64(out + 13, qidpath);               /* qid.path    */
    styx_put32(out + 21, (uint32_t)(is_dir ? 040755 : 0100644));
    styx_put32(out + 25, now);                   /* atime */
    styx_put32(out + 29, now);                   /* mtime */
    styx_put64(out + 33, 0);                     /* length */
    uint32_t o = 41;
    o += styx_putstr(out + o, name);
    o += styx_putstr(out + o, "wubu");
    o += styx_putstr(out + o, "wubu");
    o += styx_putstr(out + o, "wubu");
    return o;
}

/* -- Create ------------------------------------------------ */

int styxfs_create_cb(styx_server_t *base, uint32_t fid, const char *name,
                     uint32_t perm, int mode, styx_qid_t *qid) {
    styxfs_server_t *srv = styxfs_get_server(base);
    if (srv->readonly) { errno = EACCES; return -1; }
    if (!name || !*name) { errno = EINVAL; return -1; }

    styx_fid_t *f = styx_fid_lookup(base, fid);
    if (!f) return -1;

    const char *parent = fid_path(f);
    char host_parent[STYXFS_MAX_PATH];
    char host[STYXFS_MAX_PATH];
    char nspath[STYXFS_MAX_PATH];
    styxfs_path_to_host(srv, parent, host_parent, sizeof(host_parent));
    snprintf(host, sizeof(host), "%s/%s", host_parent, name);
    snprintf(nspath, sizeof(nspath), "%s%s%s", parent,
             (parent[0] && parent[strlen(parent) - 1] == '/') ? "" : "/", name);

    struct stat st;
    int exists = (stat(host, &st) == 0);
    int want_dir = (perm & STX_DMDIR) != 0;

    if (exists) {
        if ((perm & STX_DMEXCL) || (mode & STX_OEXCL)) { errno = EEXIST; return -1; }
        if (want_dir && !S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }
        if (!want_dir && S_ISDIR(st.st_mode)) { errno = EISDIR; return -1; }
    } else if (want_dir) {
        if (mkdir(host, 0755) != 0) return -1;
    } else {
        unsigned pmode = perm & 0777u;
        int fd = open(host, O_CREAT | O_WRONLY, pmode ? pmode : 0644u);
        if (fd < 0) return -1;
        close(fd);
    }

    /* The fid now names the created node. */
    free(f->file_state);
    f->file_state = strdup(nspath);
    f->qid.type = want_dir ? STX_QTDIR : 0;
    f->qid.path = (uint64_t)(unsigned char)nspath[strlen(nspath) - 1] + 1000;
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

        /* One ordered entry list: mount children, then host entries, deduped. */
        char names[256][64];
        int  isdir[256];
        int  n = 0;

        size_t plen = strlen(path);
        for (styxfs_mount_t *m = srv->mounts; m && n < 256; m = m->next) {
            if (strncmp(m->path, path, plen) != 0) continue;
            if (!(plen == 1 && path[0] == '/') && m->path[plen] != '/') continue;
            const char *name = m->path + (plen == 1 ? 1 : plen + 1);
            const char *slash = strchr(name, '/');
            size_t nl = slash ? (size_t)(slash - name) : strlen(name);
            if (!nl || nl >= 64) continue;
            memcpy(names[n], name, nl);
            names[n][nl] = '\0';
            isdir[n] = 1;
            n++;
        }

        char host[STYXFS_MAX_PATH];
        styxfs_path_to_host(srv, path, host, sizeof(host));
        DIR *d = opendir(host);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL && n < 256) {
                if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                    continue;
                int dup = 0;
                for (int i = 0; i < n; i++)
                    if (strcmp(names[i], de->d_name) == 0) { dup = 1; break; }
                if (dup) continue;
                snprintf(names[n], sizeof names[n], "%s", de->d_name);
                isdir[n] = (de->d_type == DT_DIR);
                n++;
            }
            closedir(d);
        }

        /* Page by BYTE offset, as 9P defines it. The encoder returns each
         * record's exact length, so nread always lands on a record boundary. */
        uint64_t rec_start = 0;
        for (int i = 0; i < n; i++) {
            uint8_t rec[512];
            uint32_t rl = styxfs_dir_encode(rec, sizeof rec, names[i], isdir[i],
                                            (uint64_t)(i + 100));
            if (!rl) break;
            if (rec_start + rl <= offset) { rec_start += rl; continue; }
            if (buf_off + rl > count) break;
            memcpy(data + buf_off, rec, rl);
            buf_off += rl;
            rec_start += rl;
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
    /* One Rwrite can acknowledge at most STYX_MAX_DATA bytes; the client must
     * send the rest. A QTAPPEND node ignores the client offset entirely. */
    uint32_t n = count > STYX_MAX_DATA ? STYX_MAX_DATA : count;
    int append = (f->qid.type & STX_QTAPPEND) != 0;

    /* A control-plane write replaces the file's content (echo > file), except
     * on an append-only node where replacing would defeat the point. */
    int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
    int fd = open(host, flags, 0644);
    if (fd < 0) return -1;
    ssize_t done = (n > 0) ? pwrite(fd, data, n, append ? 0 : (off_t)offset) : 0;
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

int styxfs_rename_cb(styx_server_t *base, uint32_t oldfid, const char *oldname,
                     uint32_t newfid, const char *newname) {
    styxfs_server_t *srv = styxfs_get_server(base);
    if (srv->readonly) { errno = EACCES; return -1; }
    if (!oldname || !newname) { errno = EINVAL; return -1; }

    styx_fid_t *of = styx_fid_lookup(base, oldfid);
    styx_fid_t *nf = styx_fid_lookup(base, newfid);
    if (!of || !nf) return -1;

    char oh[STYXFS_MAX_PATH], nh[STYXFS_MAX_PATH], op[STYXFS_MAX_PATH], np[STYXFS_MAX_PATH];
    styxfs_path_to_host(srv, fid_path(of), oh, sizeof(oh));
    styxfs_path_to_host(srv, fid_path(nf), nh, sizeof(nh));
    snprintf(op, sizeof(op), "%s/%s", oh, oldname);
    snprintf(np, sizeof(np), "%s/%s", nh, newname);

    if (rename(op, np) != 0) return -1;
    return 0;
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

    /* A mount point is a boundary: advertise QTMOUNT with a qid that differs
     * from the underlying node, so a client can detect it without walking. */
    for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
        if (strcmp(m->path, fid_path(f)) == 0) {
            dir->qid.type |= STX_QTMOUNT;
            dir->qid.path  = 0x8000000000000000ull | (uint64_t)(unsigned char)m->path[1];
            break;
        }
    }

    {
        char host[STYXFS_MAX_PATH];
        styxfs_path_to_host(srv, fid_path(f), host, sizeof(host));
        struct stat st;
        if (stat(host, &st) == 0) {
            dir->mode = (uint32_t)((S_ISDIR(st.st_mode) ? 0040000 : 0100000) |
                                   (st.st_mode & 07777));
            dir->length = (uint64_t)st.st_size;
        } else {
            dir->mode = (f->qid.type == STX_QTDIR) ? 040755 : 0100644;
            dir->length = 0;
        }
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
