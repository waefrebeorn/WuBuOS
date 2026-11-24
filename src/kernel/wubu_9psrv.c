/*
 * wubu_9psrv.c  --  KERNEL-CLEAN Styx/9P2000 SERVER for /n/kv.
 *
 * Serves the LIVE KV tensor as a namespace, never a disk image:
 *
 *   /              dir   first segment of every live mount ("kv")
 *   /kv            dir   the mount table under it
 *   /kv/world      dir   a mount point
 *   /kv/world/tick_3     FILE: block_size floats at the mount+hash offset
 *
 * A tick file's bytes come from wubu_kvfs_route_read(), i.e. straight out of
 * g_wubu_kv_base -- the same ring-0 tensor the body writes each tick. That is
 * the AGI loop's read path: the Brain walks /n/kv over 9P, the Body's memory
 * answers.
 *
 * Freestanding: memcpy/memset only, no allocator, bounded pools, opaque
 * struct, one reply buffer supplied by the caller.
 */
#include "wubu_9psrv.h"
#include "wubu_9pwire.h"
#include "wubu_kvfs.h"

#define W9S_MAX_FIDS        32
#define W9S_MAX_PATH        256
#define W9S_DIR_MAX_CHILDREN 12
#define W9S_NAME_MAX        96
#define W9S_DIR_SCRATCH     1024
#define W9S_MAX_MOUNT_SCAN  64
#define W9S_QID_VERSION     1u

typedef struct {
    uint8_t  used;
    uint8_t  opened;
    uint32_t fid;
    char     path[W9S_MAX_PATH];
} w9s_fid_t;

/* one namespace per kernel, like g_wubu_kvfs */
static struct {
    w9s_fid_t fids[W9S_MAX_FIDS];
    uint32_t  msize;
} g_srv;

/* ---- small string helpers (no libc beyond memcpy/memset) ---- */

static size_t s_len(const char *s) { size_t n = 0; if (s) while (s[n]) n++; return n; }

static int s_eq(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* does `path` start with `pfx`? (byte prefix, no boundary rules) */
static int s_starts(const char *path, const char *pfx)
{
    if (!path || !pfx) return 0;
    while (*pfx) { if (*path != *pfx) return 0; path++; pfx++; }
    return 1;
}

static void s_join(char *dst, size_t cap, const char *dir, const char *name)
{
    size_t d = s_len(dir), n = s_len(name), at = 0;
    if (d >= cap - 2) d = cap - 2;
    memcpy(dst, dir, d);
    at = d;
    if (at == 0 || dir[at - 1] != '/') { if (at + 1 < cap) dst[at++] = '/'; }
    for (size_t i = 0; i < n && at + 1 < cap; i++) dst[at++] = name[i];
    dst[at] = '\0';
}

/* ---- namespace model (the mount table IS the tree) ---- */

#define NS_NONE 0
#define NS_DIR  1
#define NS_FILE 2

/* classify a path: dir, file (size = one block of floats), or nothing */
static int ns_classify(const char *path, uint32_t *size)
{
    if (!g_wubu_kvfs || !path || !path[0]) return NS_NONE;
    if (s_eq(path, "/")) return NS_DIR;

    int ancestor_of_mount = 0;
    for (int i = 0; i < W9S_MAX_MOUNT_SCAN; i++) {
        wubu_kvfs_mount_t m;
        if (wubu_kvfs_mount_at(g_wubu_kvfs, i, &m) != 0) continue;
        if (s_eq(m.path, "/")) continue;              /* root is implicit */
        if (s_eq(path, m.path)) return NS_DIR;        /* a mount point is a dir */
        if (s_starts(path, m.path) && path[s_len(m.path)] == '/') {
            /* under the mount: one file per name, one block of floats */
            if (size) *size = m.block_size * (uint32_t)sizeof(float);
            return NS_FILE;
        }
        if (s_starts(m.path, path) && m.path[s_len(path)] == '/')
            ancestor_of_mount = 1;
    }
    return ancestor_of_mount ? NS_DIR : NS_NONE;
}

/* collect the unique child names of a directory (mount-table order) */
static int ns_children(const char *dir, char names[][W9S_NAME_MAX], int max)
{
    int n = 0;
    if (!g_wubu_kvfs) return 0;
    size_t dlen = s_len(dir);
    for (int i = 0; i < W9S_MAX_MOUNT_SCAN && n < max; i++) {
        wubu_kvfs_mount_t m;
        if (wubu_kvfs_mount_at(g_wubu_kvfs, i, &m) != 0) continue;
        if (s_eq(m.path, "/")) continue;
        const char *rest = NULL;
        if (s_eq(dir, "/")) {
            rest = m.path + 1;                     /* skip the leading '/' */
        } else if (s_starts(m.path, dir) && m.path[dlen] == '/') {
            rest = m.path + dlen + 1;
        } else {
            continue;
        }
        size_t nl = 0;
        while (rest[nl] && rest[nl] != '/') nl++;
        if (nl == 0 || nl >= W9S_NAME_MAX) continue;
        int dup = 0;
        for (int k = 0; k < n; k++)
            if (s_len(names[k]) == nl && memcmp(names[k], rest, nl) == 0) dup = 1;
        if (dup) continue;
        memcpy(names[n], rest, nl);
        names[n][nl] = '\0';
        n++;
    }
    return n;
}

/* ---- reply encoding (9P2000 stat layout, byte-identical to styxfs) ---- */

static void put_qid(uint8_t *p, uint8_t type, uint64_t path)
{
    p[0] = type;
    w9w_put32(p + 1, W9S_QID_VERSION);
    w9w_put64(p + 5, path);
}

static uint64_t qid_path_of(const char *path)
{
    uint64_t h = 0xcbf29ce484222325ull;              /* FNV-1a: stable qids */
    while (path && *path) { h ^= (uint8_t)*path++; h *= 0x100000001b3ull; }
    return h;
}

/* stat entry: size[2] type[2] dev[4] qid[13] mode[4] atime[4] mtime[4]
 * length[8] name[s] uid[s] gid[s] muid[s]; returns bytes written */
static uint32_t put_stat(uint8_t *p, const char *name, const char *full,
                         int isdir, uint32_t length)
{
    /* body = everything after size[2]: type[2] dev[4] qid[13] mode[4]
     * atime[4] mtime[4] length[8] + four strings (2+len each) */
    uint32_t body = 2 + 4 + 13 + 4 + 4 + 4 + 8 +
                    (2 + (uint32_t)s_len(name)) + 6 + 6 + 6;
    w9w_put16(p, (uint16_t)body);
    w9w_put16(p + 2, 0);                             /* type: regular/anything */
    w9w_put32(p + 4, 0);                             /* dev */
    put_qid(p + 8, isdir ? W9_QTDIR : W9_QTFILE, qid_path_of(full));
    w9w_put32(p + 21, isdir ? 040755u : 0100644u);
    w9w_put32(p + 25, 0);                            /* atime */
    w9w_put32(p + 29, 0);                            /* mtime */
    w9w_put64(p + 33, length);
    uint32_t at = 41;
    at += w9w_putstr(p + at, name);
    at += w9w_putstr(p + at, "wubu");
    at += w9w_putstr(p + at, "wubu");
    at += w9w_putstr(p + at, "wubu");
    return at;
}

static int r_error(uint8_t *rsp, uint32_t cap, uint16_t tag,
                   const char *msg, uint32_t *olen)
{
    uint32_t at = W9_HDR;
    at += w9w_putstr(rsp + at, msg);
    if (at > cap) return -1;
    w9w_header(rsp, at, W9_RERROR, tag);
    *olen = at;
    return 0;
}

/* ---- fid table ---- */

static w9s_fid_t *fid_find(uint32_t fid)
{
    for (int i = 0; i < W9S_MAX_FIDS; i++)
        if (g_srv.fids[i].used && g_srv.fids[i].fid == fid) return &g_srv.fids[i];
    return NULL;
}

static w9s_fid_t *fid_alloc(uint32_t fid, const char *path)
{
    for (int i = 0; i < W9S_MAX_FIDS; i++) {
        w9s_fid_t *f = &g_srv.fids[i];
        if (f->used) continue;
        f->used = 1;
        f->opened = 0;
        f->fid = fid;
        size_t l = s_len(path);
        if (l >= W9S_MAX_PATH) l = W9S_MAX_PATH - 1;
        memcpy(f->path, path, l);
        f->path[l] = '\0';
        return f;
    }
    return NULL;
}

void wubu_9psrv_reset(void)
{
    for (int i = 0; i < W9S_MAX_FIDS; i++) {
        g_srv.fids[i].used = 0;
        g_srv.fids[i].opened = 0;
        g_srv.fids[i].fid = 0;
        g_srv.fids[i].path[0] = '\0';
    }
    g_srv.msize = WUBU_9PSRV_MSIZE;
}

/* ---- handlers ---- */

static int h_version(const uint8_t *req, uint32_t reqlen, uint8_t *rsp,
                     uint32_t cap, uint16_t tag, uint32_t *olen)
{
    if (reqlen < W9_HDR + 4) return -1;
    uint32_t msize = w9w_get32(req + W9_HDR);
    if (msize > WUBU_9PSRV_MSIZE) msize = WUBU_9PSRV_MSIZE;
    wubu_9psrv_reset();                     /* a version = a new session */
    g_srv.msize = msize;
    uint32_t at = W9_HDR;
    w9w_put32(rsp + at, msize); at += 4;
    at += w9w_putstr(rsp + at, "9P2000");   /* always plain 2000 */
    if (at > cap) return -1;
    w9w_header(rsp, at, W9_RVERSION, tag);
    *olen = at;
    return 0;
}

static int h_attach(const uint8_t *req, uint32_t reqlen, uint8_t *rsp,
                    uint32_t cap, uint16_t tag, uint32_t *olen)
{
    if (reqlen < W9_HDR + 8) return -1;
    uint32_t fid = w9w_get32(req + W9_HDR);
    w9s_fid_t *f = fid_alloc(fid, "/");
    if (!f) return r_error(rsp, cap, tag, "no free fid", olen);
    uint32_t at = W9_HDR;
    put_qid(rsp + at, W9_QTDIR, qid_path_of("/")); at += 13;
    w9w_put32(rsp + at, 0); at += 4;                 /* iounit */
    if (at > cap) return -1;
    w9w_header(rsp, at, W9_RATTACH, tag);
    *olen = at;
    return 0;
}

static int h_walk(const uint8_t *req, uint32_t reqlen, uint8_t *rsp,
                  uint32_t cap, uint16_t tag, uint32_t *olen)
{
    if (reqlen < W9_HDR + 10) return -1;
    uint32_t fid = w9w_get32(req + W9_HDR);
    uint32_t newfid = w9w_get32(req + W9_HDR + 4);
    uint16_t nwname = w9w_get16(req + W9_HDR + 8);
    w9s_fid_t *f = fid_find(fid);
    if (!f) return r_error(rsp, cap, tag, "unknown fid", olen);
    const uint8_t *p = req + W9_HDR + 10;
    const uint8_t *end = req + reqlen;

    char path[W9S_MAX_PATH];
    size_t pl = s_len(f->path);
    if (pl >= W9S_MAX_PATH) pl = W9S_MAX_PATH - 1;
    memcpy(path, f->path, pl); path[pl] = '\0';

    uint8_t qids[16][13];
    uint16_t nq = 0;
    for (uint16_t i = 0; i < nwname && i < 16; i++) {
        char name[W9S_NAME_MAX];
        uint32_t fld = w9w_getstr(p, end, name, sizeof name);
        if (!fld) break;
        p += fld;
        char next[W9S_MAX_PATH];
        s_join(next, sizeof next, path, name);
        uint32_t size = 0;
        int kind = ns_classify(next, &size);
        if (kind == NS_NONE) break;                  /* partial walk ends here */
        put_qid(qids[nq], kind == NS_DIR ? W9_QTDIR : W9_QTFILE, qid_path_of(next));
        nq++;
        size_t nl = s_len(next);
        if (nl >= W9S_MAX_PATH) nl = W9S_MAX_PATH - 1;
        memcpy(path, next, nl); path[nl] = '\0';
    }
    if (nwname > 0 && nq == 0)
        return r_error(rsp, cap, tag, "not found", olen);

    if (newfid == fid) {
        size_t l = s_len(path);
        memcpy(f->path, path, l + 1);
    } else {
        w9s_fid_t *nf = fid_alloc(newfid, path);
        if (!nf) return r_error(rsp, cap, tag, "no free fid", olen);
    }
    uint32_t at = W9_HDR;
    w9w_put16(rsp + at, nq); at += 2;
    for (uint16_t i = 0; i < nq; i++) { memcpy(rsp + at, qids[i], 13); at += 13; }
    if (at > cap) return -1;
    w9w_header(rsp, at, W9_RWALK, tag);
    *olen = at;
    return 0;
}

static int h_open(const uint8_t *req, uint32_t reqlen, uint8_t *rsp,
                  uint32_t cap, uint16_t tag, uint32_t *olen)
{
    if (reqlen < W9_HDR + 5) return -1;
    uint32_t fid = w9w_get32(req + W9_HDR);
    uint8_t mode = req[W9_HDR + 4];
    w9s_fid_t *f = fid_find(fid);
    if (!f) return r_error(rsp, cap, tag, "unknown fid", olen);
    if (mode & 3)                                  /* write/rdwr: body-owned */
        return r_error(rsp, cap, tag, "permission denied: /n/kv is body-owned", olen);
    uint32_t size = 0;
    int kind = ns_classify(f->path, &size);
    if (kind == NS_NONE) return r_error(rsp, cap, tag, "not found", olen);
    f->opened = 1;
    uint32_t at = W9_HDR;
    put_qid(rsp + at, kind == NS_DIR ? W9_QTDIR : W9_QTFILE, qid_path_of(f->path));
    at += 13;
    w9w_put32(rsp + at, 0); at += 4;                /* iounit */
    if (at > cap) return -1;
    w9w_header(rsp, at, W9_ROPEN, tag);
    *olen = at;
    return 0;
}

static int h_read(const uint8_t *req, uint32_t reqlen, uint8_t *rsp,
                  uint32_t cap, uint16_t tag, uint32_t *olen)
{
    if (reqlen < W9_HDR + 16) return -1;
    uint32_t fid = w9w_get32(req + W9_HDR);
    uint64_t off = w9w_get64(req + W9_HDR + 4);
    uint32_t count = w9w_get32(req + W9_HDR + 12);
    w9s_fid_t *f = fid_find(fid);
    if (!f) return r_error(rsp, cap, tag, "unknown fid", olen);
    if (!f->opened) return r_error(rsp, cap, tag, "fid not open", olen);

    uint32_t size = 0;
    int kind = ns_classify(f->path, &size);
    if (kind == NS_NONE) return r_error(rsp, cap, tag, "not found", olen);

    uint32_t at = W9_HDR + 4;                       /* leave room for count[4] */
    uint32_t n = 0;

    if (kind == NS_FILE) {
        if (off >= size) { n = 0; }
        else {
            uint32_t want = count;
            if (want > size - (uint32_t)off) want = size - (uint32_t)off;
            if (at + want > cap) want = cap > at ? cap - at : 0;
            uint32_t got = 0;
            if (want && wubu_kvfs_route_read(f->path, off, want,
                                             rsp + at, &got) == 0)
                n = got;
        }
    } else {
        /* directory: build the stat stream, then page it by BYTE offset */
        uint8_t scratch[W9S_DIR_SCRATCH];
        char names[W9S_DIR_MAX_CHILDREN][W9S_NAME_MAX];
        int nc = ns_children(f->path, names, W9S_DIR_MAX_CHILDREN);
        uint32_t sl = 0;
        for (int i = 0; i < nc; i++) {
            char child[W9S_MAX_PATH];
            s_join(child, sizeof child, f->path, names[i]);
            uint32_t csize = 0;
            int ck = ns_classify(child, &csize);
            if (ck == NS_NONE) continue;
            if (sl + 41 + 2 * s_len(names[i]) + 24 > sizeof scratch) break;
            sl += put_stat(scratch + sl, names[i], child, ck == NS_DIR, csize);
        }
        if (off < sl) {
            uint32_t want = sl - (uint32_t)off;
            if (want > count) want = count;
            if (at + want > cap) want = cap > at ? cap - at : 0;
            memcpy(rsp + at, scratch + off, want);
            n = want;
        }
    }

    if (n > count) n = count;
    w9w_put32(rsp + W9_HDR, n);
    at += n;
    w9w_header(rsp, at, W9_RREAD, tag);
    *olen = at;
    return 0;
}

static int h_clunk(const uint8_t *req, uint32_t reqlen, uint8_t *rsp,
                   uint32_t cap, uint16_t tag, uint32_t *olen)
{
    if (reqlen < W9_HDR + 4) return -1;
    uint32_t fid = w9w_get32(req + W9_HDR);
    w9s_fid_t *f = fid_find(fid);
    if (f) { f->used = 0; f->opened = 0; }
    uint32_t at = W9_HDR;
    if (at > cap) return -1;
    w9w_header(rsp, at, W9_RCLUNK, tag);
    *olen = at;
    return 0;
}

static int h_stat(const uint8_t *req, uint32_t reqlen, uint8_t *rsp,
                  uint32_t cap, uint16_t tag, uint32_t *olen)
{
    if (reqlen < W9_HDR + 4) return -1;
    uint32_t fid = w9w_get32(req + W9_HDR);
    w9s_fid_t *f = fid_find(fid);
    if (!f) return r_error(rsp, cap, tag, "unknown fid", olen);
    uint32_t size = 0;
    int kind = ns_classify(f->path, &size);
    if (kind == NS_NONE) return r_error(rsp, cap, tag, "not found", olen);
    const char *base = f->path;
    for (const char *q = f->path; *q; q++) if (*q == '/') base = q + 1;
    if (!*base) base = "/";
    uint32_t at = W9_HDR;
    at += put_stat(rsp + at, base, f->path, kind == NS_DIR, size);
    if (at > cap) return -1;
    w9w_header(rsp, at, W9_RSTAT, tag);
    *olen = at;
    return 0;
}

/* ---- dispatcher ---- */

int wubu_9psrv_serve(const uint8_t *req, uint32_t reqlen,
                     uint8_t *rsp, uint32_t rsp_cap, uint32_t *rsplen)
{
    if (!req || !rsp || !rsplen || reqlen < W9_HDR + 1 || rsp_cap < W9_HDR)
        return -1;
    uint8_t type = req[4];
    uint16_t tag = w9w_get16(req + 5);

    switch (type) {
    case W9_TVERSION: return h_version(req, reqlen, rsp, rsp_cap, tag, rsplen);
    case W9_TATTACH:  return h_attach(req, reqlen, rsp, rsp_cap, tag, rsplen);
    case W9_TWALK:    return h_walk(req, reqlen, rsp, rsp_cap, tag, rsplen);
    case W9_TOPEN:    return h_open(req, reqlen, rsp, rsp_cap, tag, rsplen);
    case W9_TREAD:    return h_read(req, reqlen, rsp, rsp_cap, tag, rsplen);
    case W9_TCLUNK:   return h_clunk(req, reqlen, rsp, rsp_cap, tag, rsplen);
    case W9_TSTAT:    return h_stat(req, reqlen, rsp, rsp_cap, tag, rsplen);
    default:
        return r_error(rsp, rsp_cap, tag, "unsupported message", rsplen);
    }
}
