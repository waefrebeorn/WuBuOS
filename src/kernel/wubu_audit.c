/*
 * wubu_audit.c -- hash-chained kernel audit log with tamper detection
 * (SECURITY #775). See wubu_audit.h for the contract and owner
 * boundaries. Freestanding C11: no malloc, no hosted APIs.
 */
#include "wubu_audit.h"
#include "wubu_sha256.h"

#include <string.h>

/* The ring (static: a kernel audit log must survive heap trouble). */
static wubu_audit_rec_t g_audit[WUBU_AUDIT_CAP];
static size_t g_audit_n;          /* retained records */
static size_t g_audit_next;       /* next slot (mod CAP) */
static uint64_t g_audit_seq;      /* monotonic, crosses evictions */
static uint8_t g_audit_chain[WUBU_AUDIT_HASH_SZ];  /* the running head */

/* hash_i = sha256(prev_hash || seq || ts || event || pid || msg).
 * The field order is part of the wire contract: changing it changes
 * every hash, so a verify() failure is unambiguous. The context buffer
 * is byte-allocated because wubu_sha256_ctx is opaque by contract. */
static void rec_hash(const wubu_audit_rec_t *r, uint8_t out[WUBU_AUDIT_HASH_SZ])
{
    uint8_t ctx_buf[WUBU_SHA256_CTX_SZ];
    wubu_sha256_ctx *ctx = (wubu_sha256_ctx *)ctx_buf;
    wubu_sha256_init(ctx);
    wubu_sha256_update(ctx, r->prev_hash, sizeof(r->prev_hash));
    wubu_sha256_update(ctx, &r->seq,  sizeof(r->seq));
    wubu_sha256_update(ctx, &r->ts,   sizeof(r->ts));
    wubu_sha256_update(ctx, &r->event, sizeof(r->event));
    wubu_sha256_update(ctx, &r->pid,  sizeof(r->pid));
    wubu_sha256_update(ctx, r->msg,   sizeof(r->msg));
    wubu_sha256_final(ctx, out);
}

void wubu_audit_reset(void)
{
    memset(g_audit, 0, sizeof(g_audit));
    memset(g_audit_chain, 0, sizeof(g_audit_chain));
    g_audit_n = 0;
    g_audit_next = 0;
    g_audit_seq = 0;
}

uint64_t wubu_audit_record(uint32_t event, uint32_t pid, uint64_t ts,
                           const char *msg)
{
    if (!msg) return 0;
    wubu_audit_rec_t *r = &g_audit[g_audit_next];
    memset(r, 0, sizeof(*r));
    r->seq   = ++g_audit_seq;
    r->ts    = ts;
    r->event = event;
    r->pid   = pid;
    for (size_t i = 0; i + 1 < sizeof(r->msg) && msg[i]; i++)
        r->msg[i] = msg[i];
    memcpy(r->prev_hash, g_audit_chain, sizeof(r->prev_hash));
    rec_hash(r, r->hash);
    memcpy(g_audit_chain, r->hash, sizeof(g_audit_chain));

    g_audit_next = (g_audit_next + 1) % WUBU_AUDIT_CAP;
    if (g_audit_n < WUBU_AUDIT_CAP) g_audit_n++;
    return r->seq;
}

size_t wubu_audit_count(void)
{
    return g_audit_n;
}

const wubu_audit_rec_t *wubu_audit_at(size_t i)
{
    if (i >= g_audit_n) return NULL;
    /* slot 0 of the retained window is the oldest record: the ring
     * started at 0 and wrapped, so the oldest sits at g_audit_next
     * once the ring is full, else at 0. */
    size_t first = (g_audit_n == WUBU_AUDIT_CAP) ? g_audit_next : 0;
    return &g_audit[(first + i) % WUBU_AUDIT_CAP];
}

void wubu_audit_head(uint8_t out[WUBU_AUDIT_HASH_SZ])
{
    if (!out) return;
    memcpy(out, g_audit_chain, WUBU_AUDIT_HASH_SZ);
}

int wubu_audit_verify(void)
{
    const wubu_audit_rec_t *prev = NULL;
    for (size_t i = 0; i < g_audit_n; i++) {
        const wubu_audit_rec_t *r = wubu_audit_at(i);
        uint8_t h[WUBU_AUDIT_HASH_SZ];
        rec_hash(r, h);
        if (memcmp(h, r->hash, sizeof(h)) != 0)
            return -1;               /* this record was altered */
        if (prev) {
            if (memcmp(r->prev_hash, prev->hash, sizeof(h)) != 0)
                return -1;           /* the link was broken (reorder) */
            if (r->seq <= prev->seq)
                return -1;           /* order violation */
        }
        prev = r;
    }
    return 0;
}
