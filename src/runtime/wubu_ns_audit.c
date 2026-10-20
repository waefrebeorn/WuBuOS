/*
 * wubu_ns_audit.c -- namespace audit log (see wubu_ns_audit.h).
 *
 * NAMESPACE #581 (every 9P transaction recorded), #585 (denial trace at
 * /n/audit/denied), #594 (rotation with deterministic replay).
 *
 * Design: two record streams (transactions + denials), each a chain of
 * FNV-1a-64 hashes that crosses segment boundaries, so rotation never
 * breaks replay. The chain value of record i is
 *     chain_i = fnv64(chain_{i-1} || seq_i || payload_i)
 * and chain_{-1} is the stream seed. Replay digest folds every record in
 * sequence order -- the same log always yields the same digest.
 */
#include "wubu_ns_audit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TX_STREAM_SEED    0x545853545245414dULL  /* "TXSTREAM" */
#define DENY_STREAM_SEED  0x44454e5944535452ULL  /* "DENYSTRM" */

struct tx_rec {
    uint64_t seq;
    char     tx[WUBU_NS_AUDIT_TX_LEN];
    uint64_t fid;
    char     path[WUBU_NS_AUDIT_PATH_LEN];
    int      status;
    uint64_t chain;
};

struct deny_rec {
    uint64_t seq;
    char     path[WUBU_NS_AUDIT_PATH_LEN];
    uint64_t missing_rights;
    uint64_t chain;
};

struct wubu_ns_audit {
    struct tx_rec   tx[WUBU_NS_AUDIT_SEGS][WUBU_NS_AUDIT_CAP];
    struct deny_rec deny[WUBU_NS_AUDIT_SEGS][WUBU_NS_AUDIT_CAP];
    int active_seg;
    int tx_len[WUBU_NS_AUDIT_SEGS];      /* records per segment */
    int deny_len[WUBU_NS_AUDIT_SEGS];
    int sealed[WUBU_NS_AUDIT_SEGS];
    uint64_t tx_seq, deny_seq;
    uint64_t tx_chain, deny_chain;       /* the running chain heads */
    int sealed_count;
};

/* ---- FNV-1a-64 (ours, C11, no deps) -------------------------------- */

static uint64_t fnv_byte(uint64_t h, uint8_t b)
{
    h ^= b;
    h *= 0x100000001b3ULL;
    return h;
}

static uint64_t fnv_bytes(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    for (size_t i = 0; i < n; i++) h = fnv_byte(h, b[i]);
    return h;
}

/* ---- helpers -------------------------------------------------------- */

wubu_ns_audit_t *wubu_ns_audit_new(void)
{
    wubu_ns_audit_t *a = malloc(sizeof(*a));
    if (!a) return NULL;
    memset(a, 0, sizeof(*a));
    a->active_seg = 0;
    a->tx_chain   = TX_STREAM_SEED;
    a->deny_chain = DENY_STREAM_SEED;
    return a;
}

void wubu_ns_audit_free(wubu_ns_audit_t *a) { free(a); }

size_t wubu_ns_audit_tx_count(const wubu_ns_audit_t *a)
{
    if (!a) return 0;
    size_t n = 0;
    for (int s = 0; s <= a->active_seg; s++)
        n += (size_t)a->tx_len[s];
    return n;
}

size_t wubu_ns_audit_denial_count(const wubu_ns_audit_t *a)
{
    if (!a) return 0;
    size_t n = 0;
    for (int s = 0; s <= a->active_seg; s++)
        n += (size_t)a->deny_len[s];
    return n;
}

int wubu_ns_audit_sealed_count(const wubu_ns_audit_t *a)
{
    return a ? a->sealed_count : -1;
}

int wubu_ns_audit_rotate(wubu_ns_audit_t *a)
{
    if (!a) return -1;
    if (a->active_seg + 1 >= WUBU_NS_AUDIT_SEGS) return -2;
    a->sealed[a->active_seg] = 1;
    a->sealed_count++;
    a->active_seg++;
    return 0;
}

int wubu_ns_audit_tx(wubu_ns_audit_t *a, const char *tx, uint64_t fid,
                     const char *path, int status)
{
    if (!a || !tx || !path || !tx[0]) return -1;
    int s = a->active_seg;
    if (a->tx_len[s] >= WUBU_NS_AUDIT_CAP) return -2;  /* rotate first */
    struct tx_rec *r = &a->tx[s][a->tx_len[s]];
    memset(r, 0, sizeof(*r));
    r->seq = ++a->tx_seq;
    for (size_t i = 0; i + 1 < sizeof(r->tx) && tx[i]; i++)
        r->tx[i] = tx[i];
    for (size_t i = 0; i + 1 < sizeof(r->path) && path[i]; i++)
        r->path[i] = path[i];
    r->fid    = fid;
    r->status = status;
    /* chain_i = fnv(chain_prev || seq || tx || fid || path || status) */
    uint64_t h = a->tx_chain;
    h = fnv_bytes(h, &r->seq,    sizeof(r->seq));
    h = fnv_bytes(h, r->tx,      sizeof(r->tx));
    h = fnv_bytes(h, &r->fid,    sizeof(r->fid));
    h = fnv_bytes(h, r->path,    sizeof(r->path));
    h = fnv_bytes(h, &r->status, sizeof(r->status));
    r->chain    = h;
    a->tx_chain = h;
    a->tx_len[s]++;
    return 0;
}

int wubu_ns_audit_denial(wubu_ns_audit_t *a, const char *path,
                         uint64_t missing_rights)
{
    if (!a || !path || !path[0]) return -1;
    int s = a->active_seg;
    if (a->deny_len[s] >= WUBU_NS_AUDIT_CAP) return -2;
    struct deny_rec *r = &a->deny[s][a->deny_len[s]];
    memset(r, 0, sizeof(*r));
    r->seq = ++a->deny_seq;
    for (size_t i = 0; i + 1 < sizeof(r->path) && path[i]; i++)
        r->path[i] = path[i];
    r->missing_rights = missing_rights;
    uint64_t h = a->deny_chain;
    h = fnv_bytes(h, &r->seq,           sizeof(r->seq));
    h = fnv_bytes(h, r->path,            sizeof(r->path));
    h = fnv_bytes(h, &r->missing_rights, sizeof(r->missing_rights));
    r->chain    = h;
    a->deny_chain = h;
    a->deny_len[s]++;
    return 0;
}

/* ---- formatted read-back (the /n/audit/log + denied files) ---------- */

/* Iterate records in sequence order across segments; produce each line
 * into line_buf; return 1 while a record remains. `idx` is a running
 * record index. */
static int tx_line(const wubu_ns_audit_t *a, size_t idx, char *line,
                   size_t cap)
{
    size_t seen = 0;
    for (int s = 0; s <= a->active_seg; s++) {
        if ((size_t)a->tx_len[s] > idx - seen) {
            const struct tx_rec *r = &a->tx[s][idx - seen];
            snprintf(line, cap, "%llu %s fid=%llu %s status=%d\n",
                     (unsigned long long)r->seq, r->tx,
                     (unsigned long long)r->fid, r->path, r->status);
            return 1;
        }
        seen += (size_t)a->tx_len[s];
    }
    return 0;
}

static int deny_line(const wubu_ns_audit_t *a, size_t idx, char *line,
                     size_t cap)
{
    size_t seen = 0;
    for (int s = 0; s <= a->active_seg; s++) {
        if ((size_t)a->deny_len[s] > idx - seen) {
            const struct deny_rec *r = &a->deny[s][idx - seen];
            snprintf(line, cap, "%llu %s missing=%llx\n",
                     (unsigned long long)r->seq, r->path,
                     (unsigned long long)r->missing_rights);
            return 1;
        }
        seen += (size_t)a->deny_len[s];
    }
    return 0;
}

/* Generic offset read: walk the formatted lines, skip `off` bytes, copy
 * the rest up to n. `line_fn` yields record idx's line. */
static size_t read_stream(const wubu_ns_audit_t *a, size_t off,
                          char *buf, size_t n,
                          int (*line_fn)(const wubu_ns_audit_t *,
                                         size_t, char *, size_t))
{
    if (!a || !buf || n == 0) return 0;
    size_t done = 0, skipped = 0;
    char line[512];
    for (size_t idx = 0; line_fn(a, idx, line, sizeof(line)); idx++) {
        size_t ll = strlen(line);
        if (skipped + ll <= off) { skipped += ll; continue; }
        size_t from = off - skipped;
        size_t copy = ll - from;
        if (copy > n - done) copy = n - done;
        memcpy(buf + done, line + from, copy);
        done += copy;
        if (done == n) break;
    }
    return done;
}

size_t wubu_ns_audit_log_read(const wubu_ns_audit_t *a, size_t off,
                              char *buf, size_t n)
{
    return read_stream(a, off, buf, n, tx_line);
}

size_t wubu_ns_audit_denied_read(const wubu_ns_audit_t *a, size_t off,
                                 char *buf, size_t n)
{
    return read_stream(a, off, buf, n, deny_line);
}

/* ---- replay + verify ------------------------------------------------- */

uint64_t wubu_ns_audit_replay_digest(const wubu_ns_audit_t *a)
{
    if (!a) return 0;
    uint64_t h = 0x9e3779b97f4a7c15ULL;     /* a fixed fold seed */
    size_t seen = 0;
    for (int s = 0; s <= a->active_seg; s++) {
        for (int i = 0; i < a->tx_len[s]; i++) {
            const struct tx_rec *r = &a->tx[s][i];
            h = fnv_bytes(h, &r->chain, sizeof(r->chain));
            seen++;
        }
    }
    (void)seen;
    for (int s = 0; s <= a->active_seg; s++) {
        for (int i = 0; i < a->deny_len[s]; i++) {
            const struct deny_rec *r = &a->deny[s][i];
            h = fnv_bytes(h, &r->chain, sizeof(r->chain));
        }
    }
    return h;
}

int wubu_ns_audit_verify(const wubu_ns_audit_t *a)
{
    if (!a) return -1;
    uint64_t expect = TX_STREAM_SEED;
    for (int s = 0; s <= a->active_seg; s++) {
        for (int i = 0; i < a->tx_len[s]; i++) {
            const struct tx_rec *r = &a->tx[s][i];
            uint64_t h = expect;
            h = fnv_bytes(h, &r->seq,    sizeof(r->seq));
            h = fnv_bytes(h, r->tx,      sizeof(r->tx));
            h = fnv_bytes(h, &r->fid,    sizeof(r->fid));
            h = fnv_bytes(h, r->path,    sizeof(r->path));
            h = fnv_bytes(h, &r->status, sizeof(r->status));
            if (h != r->chain) return -1;   /* altered record */
            expect = h;
        }
    }
    expect = DENY_STREAM_SEED;
    for (int s = 0; s <= a->active_seg; s++) {
        for (int i = 0; i < a->deny_len[s]; i++) {
            const struct deny_rec *r = &a->deny[s][i];
            uint64_t h = expect;
            h = fnv_bytes(h, &r->seq,           sizeof(r->seq));
            h = fnv_bytes(h, r->path,            sizeof(r->path));
            h = fnv_bytes(h, &r->missing_rights, sizeof(r->missing_rights));
            if (h != r->chain) return -1;
            expect = h;
        }
    }
    return 0;
}
