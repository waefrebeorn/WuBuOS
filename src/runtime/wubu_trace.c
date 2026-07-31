/*
 * wubu_trace.c -- Mega OS trace foundation (Phase A).
 * Immutable, versioned, user-owned trace store with grepable plain-text mirror.
 */
#include "wubu_trace.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>

#define TRACE_RING 4096   /* spans retained per store (ring) */

struct wubu_trace_store {
    int realm_id;
    bool consent;          /* DA-1: on-device collection allowed */
    bool frozen;           /* DA-3: user freeze switch */
    uint64_t seq;          /* monotonic span id */
    int head;              /* ring write index */
    int count;             /* valid spans (<= TRACE_RING) */
    wubu_trace_span_t ring[TRACE_RING];
};

wubu_trace_store_t *wubu_trace_create(int realm_id) {
    wubu_trace_store_t *t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->realm_id = realm_id;
    t->consent = true;     /* on-device only by default (DA-1) */
    t->frozen  = false;
    t->seq = 0;
    t->head = 0;
    t->count = 0;
    return t;
}

void wubu_trace_destroy(wubu_trace_store_t *t) { free(t); }

int wubu_trace_append(wubu_trace_store_t *t, wubu_trace_kind_t kind,
                       uint64_t parent, const char *payload) {
    if (!t) return -1;
    if (!t->consent) return -1;     /* DA-1: no collection without consent */
    if (t->frozen)  return -1;      /* DA-3: user froze the loop */
    if (kind >= WUBU_TRACE_COUNT) return -1;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ts = (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;

    wubu_trace_span_t *s = &t->ring[t->head];
    memset(s, 0, sizeof(*s));
    s->id = ++t->seq;               /* monotonic; never reused */
    s->parent = parent;
    s->ts_ms = ts;
    s->kind = kind;
    s->realm_id = t->realm_id;
    s->ver_major = WUBU_TRACE_SCHEMA_MAJOR;
    if (payload) {
        strncpy(s->data, payload, sizeof(s->data) - 1);
        s->data[sizeof(s->data) - 1] = '\0';
    }
    /* IMMUTABLE after this point: no code path rewrites s->data. */

    t->head = (t->head + 1) % TRACE_RING;
    if (t->count < TRACE_RING) t->count++;
    return 0;
}

int wubu_trace_read(const wubu_trace_store_t *t, wubu_trace_span_t *out,
                     int max, int *total) {
    if (!t || !out || max <= 0) return 0;
    int copied = 0;
    /* newest first: walk backward from head */
    for (int i = 0; i < t->count && copied < max; i++) {
        int idx = (t->head - 1 - i + TRACE_RING) % TRACE_RING;
        out[copied++] = t->ring[idx];
    }
    if (total) *total = t->count;
    return copied;
}

int wubu_trace_purge(wubu_trace_store_t *t) {
    if (!t) return 0;
    int n = t->count;
    memset(t->ring, 0, sizeof(t->ring));
    t->seq = 0; t->head = 0; t->count = 0;
    return n;   /* DA-1: user-owned, deletable */
}

void wubu_trace_set_consent(wubu_trace_store_t *t, bool may_collect) {
    if (t) t->consent = may_collect;
}
bool wubu_trace_get_consent(const wubu_trace_store_t *t) {
    return t ? t->consent : false;
}
void wubu_trace_set_frozen(wubu_trace_store_t *t, bool frozen) {
    if (t) t->frozen = frozen;
}
bool wubu_trace_is_frozen(const wubu_trace_store_t *t) {
    return t ? t->frozen : true;
}

static const char *kind_name(wubu_trace_kind_t k) {
    switch (k) {
        case WUBU_TRACE_SYSCALL: return "syscall";
        case WUBU_TRACE_PROCESS: return "process";
        case WUBU_TRACE_FILE:    return "file";
        case WUBU_TRACE_NET:     return "net";
        case WUBU_TRACE_AGENT:   return "agent";
        case WUBU_TRACE_SELFMOD: return "selfmod";
        default:                 return "?";
    }
}

int wubu_trace_mirror(const wubu_trace_store_t *t, const char *path) {
    if (!t || !path) return -1;
    /* full dump newest-first to a grepable text file (N9: ASCII, not a blob) */
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    wubu_trace_span_t buf[TRACE_RING];
    int n = wubu_trace_read(t, buf, TRACE_RING, NULL);
    for (int i = 0; i < n; i++) {
        fprintf(fp, "[%llu] realm=%d v%d %s parent=%llu %s\n",
                (unsigned long long)buf[i].ts_ms,
                buf[i].realm_id, buf[i].ver_major,
                kind_name(buf[i].kind),
                (unsigned long long)buf[i].parent,
                buf[i].data);
    }
    fclose(fp);
    return 0;
}
