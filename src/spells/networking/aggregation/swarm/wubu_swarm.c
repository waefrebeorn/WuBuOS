/*
 * wubu_swarm.c -- the aggregation ledger.  See wubu_swarm.h for the premise.
 */
#include "wubu_swarm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct wubu_swarm {
    wubu_swarm_peer_t   me;
    bool                has_self;

    wubu_swarm_peer_t   peers[WUBU_SWARM_MAX_PEERS];
    uint16_t            peer_count;

    wubu_swarm_event_t  events[WUBU_SWARM_MAX_EVENTS];
    uint32_t            event_count;
};

/* FNV-1a, 64-bit.  Honest identity, not security: collisions corrupt the
 * ledger's view of WHO contributed, but the angel system holds that honest
 * peers do not collide. */
static uint64_t fnv1a(const char *s)
{
    uint64_t h = 1469598103934665603ULL;
    for (; *s; s++) { h ^= (uint8_t)*s; h *= 1099511628211ULL; }
    return h;
}

static void hex16(uint64_t v, char out[WUBU_SWARM_ID_LEN])
{
    const char *d = "0123456789abcdef";
    for (int i = 15; i >= 0; i--) { out[i] = d[v & 0xF]; v >>= 4; }
    out[16] = '\0';
}

wubu_swarm_t *wubu_swarm_create(void)
{
    return (wubu_swarm_t *)calloc(1, sizeof(wubu_swarm_t));
}

void wubu_swarm_destroy(wubu_swarm_t *s)
{
    if (s) free(s);
}

int wubu_swarm_self(wubu_swarm_t *s, const char *name)
{
    if (!s || !name || !*name) return -1;
    memset(&s->me, 0, sizeof(s->me));
    hex16(fnv1a(name), s->me.id);
    snprintf(s->me.name, sizeof(s->me.name), "%s", name);
    s->has_self = true;
    return 0;
}

const char *wubu_swarm_self_id(const wubu_swarm_t *s)
{
    if (!s || !s->has_self) return NULL;
    return s->me.id;
}

static int peer_index(const wubu_swarm_t *s, const char *id)
{
    for (int i = 0; i < s->peer_count; i++)
        if (strcmp(s->peers[i].id, id) == 0) return i;
    return -1;
}

int wubu_swarm_add_peer(wubu_swarm_t *s, const wubu_swarm_peer_t *p)
{
    if (!s || !p || !p->id[0]) return -1;
    if (strlen(p->id) + 1 > WUBU_SWARM_ID_LEN) return -1;
    if (peer_index(s, p->id) >= 0) return 0;          /* dedup, idempotent */
    if (s->peer_count >= WUBU_SWARM_MAX_PEERS) return -1;
    s->peers[s->peer_count] = *p;
    s->peer_count++;
    return 0;
}

int wubu_swarm_peers(const wubu_swarm_t *s)
{
    return s ? s->peer_count : 0;
}

int wubu_swarm_lookup(const wubu_swarm_t *s, const char *id,
                      wubu_swarm_peer_t *out)
{
    if (!s || !id || !out) return -1;
    int i = peer_index(s, id);
    if (i < 0) return -1;
    *out = s->peers[i];
    return 0;
}

int wubu_swarm_track(wubu_swarm_t *s, const char *peer_id, const char *kind)
{
    if (!s || !peer_id || !peer_id[0] || !kind || !kind[0]) return -1;
    if (strlen(peer_id) + 1 > WUBU_SWARM_ID_LEN) return -1;
    if (strlen(kind) + 1 > WUBU_SWARM_KIND_LEN) return -1;
    if (s->event_count >= WUBU_SWARM_MAX_EVENTS) return -1;

    /* An arriving contribution from an unknown peer is assumed honest and is
     * recorded; the peer joins with an empty name.  This IS the angel design. */
    if (peer_index(s, peer_id) < 0 && s->peer_count < WUBU_SWARM_MAX_PEERS) {
        wubu_swarm_peer_t p;
        memset(&p, 0, sizeof(p));
        snprintf(p.id, sizeof(p.id), "%s", peer_id);
        wubu_swarm_add_peer(s, &p);
    }

    wubu_swarm_event_t *e = &s->events[s->event_count];
    memset(e, 0, sizeof(*e));
    snprintf(e->peer, sizeof(e->peer), "%s", peer_id);
    snprintf(e->kind, sizeof(e->kind), "%s", kind);
    e->seq = s->event_count;
    s->event_count++;
    return 0;
}

int wubu_swarm_events(const wubu_swarm_t *s)
{
    return s ? (int)s->event_count : 0;
}

int wubu_swarm_event_at(const wubu_swarm_t *s, uint32_t i,
                        wubu_swarm_event_t *out)
{
    if (!s || !out || i >= s->event_count) return -1;
    *out = s->events[i];
    return 0;
}

int wubu_swarm_count_kind(const wubu_swarm_t *s, const char *kind)
{
    if (!s || !kind) return 0;
    int n = 0;
    for (uint32_t i = 0; i < s->event_count; i++)
        if (strcmp(s->events[i].kind, kind) == 0) n++;
    return n;
}

int wubu_swarm_count_peer(const wubu_swarm_t *s, const char *peer_id,
                          const char *kind)
{
    if (!s || !peer_id || !kind) return 0;
    int n = 0;
    for (uint32_t i = 0; i < s->event_count; i++) {
        const wubu_swarm_event_t *e = &s->events[i];
        if (strcmp(e->peer, peer_id) == 0 && strcmp(e->kind, kind) == 0) n++;
    }
    return n;
}
