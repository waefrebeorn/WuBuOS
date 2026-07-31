/*
 * wubu_trace.h -- Mega OS trace foundation (Phase A of MEGA_COMPAT_PLANNING.md).
 *
 * The "borg-OS" captures every realm action as a structured, versioned, IMMUTABLE
 * trace so the self-improvement loop (Phase C) can learn from it. Design rules
 * (answered the triple devil's advocate):
 *   - Boundary-captured, not hookable in-realm (DA-1/DA-2).
 *   - User-owned, on-device, deletable by default; no exfil without consent (DA-1).
 *   - Immutable append: the observed agent can NEVER rewrite its own trace (DA-2).
 *   - Versioned schema so bytropix/kernel contract changes fail CLOSED (DA-2).
 *
 * Opaque struct, C11, minimal includes.
 */
#ifndef WUBU_TRACE_H
#define WUBU_TRACE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Schema version negotiated with bytropix (kernel) at boot. Bump on breaking
 * change; consumer fails closed if kernel emits a different major. */
#define WUBU_TRACE_SCHEMA_MAJOR 1
#define WUBU_TRACE_SCHEMA_MINOR 0

typedef enum {
    WUBU_TRACE_SYSCALL = 0,   /* realm syscall crossing the boundary */
    WUBU_TRACE_PROCESS,       /* process/thread spawn in a realm */
    WUBU_TRACE_FILE,          /* file open/read/write in a realm */
    WUBU_TRACE_NET,            /* network connect/accept at the boundary */
    WUBU_TRACE_AGENT,          /* agent reasoning/tool-call step */
    WUBU_TRACE_SELFMOD,        /* a self-improvement change (gated event) */
    WUBU_TRACE_COUNT
} wubu_trace_kind_t;

/* A single typed span. Stored append-only; never mutated after write. */
typedef struct {
    uint64_t   id;          /* monotonic span id (per realm) */
    uint64_t   parent;      /* parent span id (0 = root) */
    uint64_t   ts_ms;       /* timestamp (monotonic) */
    wubu_trace_kind_t kind;
    int        realm_id;    /* which realm emitted it (-1 = host/operator) */
    int        ver_major;   /* schema major at emit time (for drift proof) */
    char       data[256];   /* human-grepable one-line payload */
} wubu_trace_span_t;

typedef struct wubu_trace_store wubu_trace_store_t;

/* Create a per-realm (or host) trace store. realm_id<0 means operator/host.
 * Returns NULL on alloc failure. */
wubu_trace_store_t *wubu_trace_create(int realm_id);
void wubu_trace_destroy(wubu_trace_store_t *t);

/* Append one span. IMMUTABLE: once written it is never rewritten by the
 * observed agent. Returns 0 on success, -1 on overflow/closed. */
int wubu_trace_append(wubu_trace_store_t *t, wubu_trace_kind_t kind,
                       uint64_t parent, const char *payload);

/* Read back spans (most-recent first) into caller buffer. Returns #copied. */
int wubu_trace_read(const wubu_trace_store_t *t, wubu_trace_span_t *out,
                     int max, int *total);

/* User ownership (DA-1): purge ALL traces for this store. Returns #purged. */
int wubu_trace_purge(wubu_trace_store_t *t);

/* Consent (DA-1): if false, wubu_trace_append() is a no-op and purge is forced.
 * Default true (on-device only). Exfil is a separate, explicit opt-in. */
void wubu_trace_set_consent(wubu_trace_store_t *t, bool may_collect);
bool wubu_trace_get_consent(const wubu_trace_store_t *t);

/* Freeze (DA-3): when frozen, no new spans are appended (user can stop the
 * self-improvement loop cold). */
void wubu_trace_set_frozen(wubu_trace_store_t *t, bool frozen);
bool wubu_trace_is_frozen(const wubu_trace_store_t *t);

/* Plain-text mirror so traces are grepable, never a binary blob (N9 rule).
 * Mirrors to /tmp/wubu-trace/<realm>.log; returns 0 on success. */
int wubu_trace_mirror(const wubu_trace_store_t *t, const char *path);

#endif /* WUBU_TRACE_H */
