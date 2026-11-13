/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "hosted.h"
#include "hosted_internal.h"
#include "../kernel/vbe.h"
#include "../kernel/memory.h"
#include "../kernel/input.h"
#include "../gui/wm.h"
#include "../gui/wm.h"
#include "../runtime/styx.h"
#include "hosted_fs_persist.h"
#include "../apps/guest_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Shared filesystem state (declared extern in hosted_internal.h). */
styxfs_file_t g_fs[STYXFS_MAX_FILES];
int           g_nfiles = 0;
uint64_t      g_next_path = 1;

/* Set by any mutating callback.  Saving is deliberately NOT done inline: the
 * Styx callbacks run inside message dispatch, and doing file I/O there would
 * make every protocol read depend on disk speed.  The host flushes on a clean
 * exit instead, and tests can force one. */
static int g_fs_dirty = 0;

/* Find an entry by exact name.  Built-in registration uses this so a persisted
 * copy of the same name is never overwritten by a freshly rebuilt default --
 * otherwise a user edit to a stock file would vanish on every restart. */
/* Mark the namespace as having unsaved changes.  Tests and future mutation
 * paths use this instead of touching the static flag directly. */
void hosted_fs_mark_dirty(void) { g_fs_dirty = 1; }

int fs_find(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < g_nfiles; i++)
        if (strcmp(g_fs[i].name, name) == 0) return i;
    return -1;
}

int fs_add_dir(const char *name) {
    if (g_nfiles >= STYXFS_MAX_FILES) return -1;
    styxfs_file_t *f = &g_fs[g_nfiles++];
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->qtype = STX_QTDIR;
    f->path = g_next_path++;
    f->length = 0;
    return 0;
}

int fs_add_file(const char *name, const uint8_t *data, uint32_t len) {
    if (g_nfiles >= STYXFS_MAX_FILES) return -1;
    styxfs_file_t *f = &g_fs[g_nfiles++];
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->qtype = STX_QTFILE;
    f->path = g_next_path++;
    f->length = len;
    if (data && len > 0) {
        uint32_t clen = len < sizeof(f->data) ? len : sizeof(f->data);
        memcpy(f->data, data, clen);
        f->data_len = clen;
    }
    return 0;
}

void fs_reset(void) { g_nfiles = 0; g_next_path = 1; g_fs_dirty = 0; }

/* Persist pending changes if there are any.  A failed save is reported loudly:
 * silently continuing would tell the user their work is safe when it is not.
 * Returns 0 when the namespace is safely on disk, -1 otherwise. */
int hosted_fs_flush(void) {
    if (!g_fs_dirty) return 0;
    if (fs_persist_save() != 0) {
        fprintf(stderr, "WuBuOS: ERROR: could not save the filesystem to %s -- "
                        "changes are only in memory\n",
                fs_persist_path() ? fs_persist_path() : "(nowhere)");
        return -1;
    }
    g_fs_dirty = 0;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════
 * Styx Server Callbacks
 * ══════════════════════════════════════════════════════════════════ */

styx_fid_t *find_fid(styx_server_t *srv, uint32_t fid) {
    for (int i = 0; i < STYX_MAX_FIDS; i++)
        if (srv->fids[i].in_use && srv->fids[i].fid == fid)
            return &srv->fids[i];
    return NULL;
}

int styx_attach_cb(styx_server_t *srv, uint32_t fid, const char *aname) {
    /* Allow attach to root regardless of aname */
    styx_fid_t *f = NULL;
    for (int i = 0; i < STYX_MAX_FIDS; i++) {
        if (!srv->fids[i].in_use) { f = &srv->fids[i]; break; }
    }
    if (!f) return -1;
    memset(f, 0, sizeof(*f));
    f->in_use = 1;
    f->fid = fid;
    f->qid.type = STX_QTDIR;
    f->qid.path = 0;
    f->qid.version = 1;
    return 0;
}

int styx_walk_cb(styx_server_t *srv, uint32_t fid, uint32_t newfid,
                         const char **wname, int nwname,
                         styx_qid_t *qids, int *nwqid) {
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    if (nwname == 0) {
        styx_fid_t *nf = NULL;
        for (int i = 0; i < STYX_MAX_FIDS; i++)
            if (!srv->fids[i].in_use) { nf = &srv->fids[i]; break; }
        if (!nf) return -1;
        *nf = *f; nf->fid = newfid;
        qids[0] = f->qid; *nwqid = 1;
        return 0;
    }
    styx_fid_t *nf = NULL;
    int walked = 0;
    for (int i = 0; i < nwname; i++) {
        styxfs_file_t *file = NULL;
        for (int j = 0; j < g_nfiles; j++)
            if (strcmp(g_fs[j].name, wname[i]) == 0) { file = &g_fs[j]; break; }
        if (!file) { *nwqid = walked; return walked > 0 ? 0 : -1; }
        qids[walked].type = file->qtype;
        qids[walked].path = file->path;
        qids[walked].version = 1;
        walked++;
        if (!nf) {
            for (int j = 0; j < STYX_MAX_FIDS; j++)
                if (!srv->fids[j].in_use) { nf = &srv->fids[j]; break; }
            if (!nf) return -1;
            *nf = *f; nf->fid = newfid;
        }
        nf->qid.type = file->qtype;
        nf->qid.path = file->path;
    }
    *nwqid = walked;
    return 0;
}

int styx_open_cb(styx_server_t *srv, uint32_t fid, int mode, styx_qid_t *qid) {
    /* mode is meaningful for host-side validation but we trust client */
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    *qid = f->qid;
    return 0;
}

int styx_read_cb(styx_server_t *srv, uint32_t fid, uint64_t offset,
                         uint32_t count, uint8_t *data, uint32_t *nread) {
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    for (int i = 0; i < g_nfiles; i++) {
        if (g_fs[i].path == f->qid.path) {
            if (g_fs[i].qtype & STX_QTDIR) { *nread = 0; return 0; }
            if (offset >= g_fs[i].data_len) { *nread = 0; return 0; }
            uint32_t avail = g_fs[i].data_len - (uint32_t)offset;
            *nread = (count < avail) ? count : avail;
            if (*nread > 0) memcpy(data, g_fs[i].data + offset, *nread);
            return 0;
        }
    }
    return -1;
}

int styx_stat_cb(styx_server_t *srv, uint32_t fid, styx_dir_t *dir) {
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    memset(dir, 0, sizeof(*dir));
    dir->qid = f->qid;
    dir->mode = (f->qid.type & STX_QTDIR) ? STX_DMDIR : 0;
    dir->mode |= 0555;
    dir->length = 0;
    if (f->qid.path == 0) {
        strcpy(dir->name, "/");
    } else {
        for (int i = 0; i < g_nfiles; i++)
            if (g_fs[i].path == f->qid.path) {
                strncpy(dir->name, g_fs[i].name, STYX_MAX_FNAME - 1);
                dir->length = g_fs[i].length;
                break;
            }
    }
    strcpy(dir->uid, "wubu");
    strcpy(dir->gid, "wubu");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════
 * Mutations -- write, clunk, remove
 *
 * These were absent, which meant the guest filesystem was strictly
 * read-only no matter what the Explorer offered to the user.  All three
 * fail closed: a write that cannot be stored exactly, a remove of a
 * directory, or any operation on an unknown fid returns -1 and leaves
 * the namespace unchanged.
 * ══════════════════════════════════════════════════════════════════ */

int styx_write_cb(styx_server_t *srv, uint32_t fid, uint64_t offset,
                  uint32_t count, const uint8_t *data, uint32_t *nwritten) {
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;

    for (int i = 0; i < g_nfiles; i++) {
        if (g_fs[i].path != f->qid.path) continue;
        if (g_fs[i].qtype & STX_QTDIR) return -1;

        /* Refuse rather than truncate: a partial write would leave the client
         * believing the file holds bytes that are not there. */
        if (offset > sizeof(g_fs[i].data)) return -1;
        uint32_t room = (uint32_t)(sizeof(g_fs[i].data) - offset);
        if (count > room) return -1;

        memcpy(g_fs[i].data + offset, data, count);
        uint32_t end = (uint32_t)(offset + count);
        if (end > g_fs[i].data_len) g_fs[i].data_len = end;
        g_fs[i].length = g_fs[i].data_len;
        *nwritten = count;
        g_fs_dirty = 1;
        return 0;
    }
    return -1;
}

int styx_clunk_cb(styx_server_t *srv, uint32_t fid) {
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    memset(f, 0, sizeof(*f));
    return 0;
}

int styx_remove_cb(styx_server_t *srv, uint32_t fid) {
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;

    int idx = -1;
    for (int i = 0; i < g_nfiles; i++)
        if (g_fs[i].path == f->qid.path) { idx = i; break; }
    if (idx < 0) return -1;
    if (g_fs[idx].qtype & STX_QTDIR) return -1;   /* no recursive delete */

    for (int i = idx; i < g_nfiles - 1; i++) g_fs[i] = g_fs[i + 1];
    g_nfiles--;
    memset(f, 0, sizeof(*f));
    g_fs_dirty = 1;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════
 * Application-facing file API
 *
 * Implemented here, next to `g_fs`, so there is exactly one owner of the
 * namespace.  Applications get guest_file.h and nothing else.
 * ══════════════════════════════════════════════════════════════════ */

int guest_file_read(const char *name, uint8_t *buf, uint32_t cap, uint32_t *len) {
    if (!name || !*name || !buf || !len) return -1;
    *len = 0;

    int idx = fs_find(name);
    if (idx < 0) return -1;
    if (g_fs[idx].qtype & STX_QTDIR) return -1;

    uint32_t avail = g_fs[idx].data_len;
    if (avail > cap) return -1;                 /* refuse rather than truncate */

    if (avail > 0) memcpy(buf, g_fs[idx].data, avail);
    *len = avail;
    return 0;
}

int guest_file_write(const char *name, const uint8_t *data, uint32_t len) {
    if (!name || !*name || (!data && len > 0)) return -1;
    if (len > GUEST_FILE_MAX) return -1;
    if (strlen(name) >= sizeof(g_fs[0].name)) return -1;

    int idx = fs_find(name);

    /* Replace in place when we can, so the qid/path stays stable and anything
     * holding a handle to the old file does not suddenly point elsewhere. */
    if (idx >= 0) {
        if (g_fs[idx].qtype & STX_QTDIR) return -1;   /* never clobber a dir */
        if (len > sizeof(g_fs[idx].data)) return -1;
        memset(g_fs[idx].data, 0, sizeof(g_fs[idx].data));
        if (len > 0) memcpy(g_fs[idx].data, data, len);
        g_fs[idx].data_len = len;
        g_fs[idx].length = len;
        g_fs_dirty = 1;
        return 0;
    }

    if (g_nfiles >= STYXFS_MAX_FILES) return -1;
    return fs_add_file(name, data, len) == 0 ? (g_fs_dirty = 1, 0) : -1;
}

int guest_file_flush(void) { return hosted_fs_flush(); }

int guest_file_delete(const char *name) {
    if (!name || !*name) return -1;
    int idx = fs_find(name);
    if (idx < 0) return -1;
    if (g_fs[idx].qtype & STX_QTDIR) return -1;   /* never recurse */

    for (int i = idx; i < g_nfiles - 1; i++) g_fs[i] = g_fs[i + 1];
    g_nfiles--;
    g_fs_dirty = 1;
    return 0;
}

int guest_file_list(char out[][64], int max_len, int cap) {
    if (!out || max_len <= 0 || cap <= 0) return -1;
    int n = 0;
    for (int i = 0; i < g_nfiles && n < cap; i++) {
        if (g_fs[i].qtype & STX_QTDIR) continue;
        snprintf(out[n], (size_t)max_len, "%s", g_fs[i].name);
        n++;
    }
    return n;
}

bool guest_file_exists(const char *name) {
    if (!name || !*name) return false;
    int idx = fs_find(name);
    return idx >= 0 && !(g_fs[idx].qtype & STX_QTDIR);
}
