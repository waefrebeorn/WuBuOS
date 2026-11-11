/*
 * wubu_swarm.h -- the peer-to-peer aggregation layer, as a spell.
 *
 * What this is FOR (first, per the reading rules): WuBuOS's ANSA to the
 * distributed-AGI vision.  The body of the AGI is meant to aggregate across
 * many nodes -- "as people play and use it, they peer-to-peer aggregate into
 * a self-trained AGI" -- carried on the SDR relay layers already built.  This
 * module is the ledger of that aggregation:
 *
 *   - every node is a PEER with a self-issued identity;
 *   - every act of play/use is a CONTRIBUTION recorded against a peer;
 *   - the ledger AGGREGATES, counting contributions by kind and by peer;
 *   - the swarm is carried over SDR, but this module itself is pure: it
 *     records and counts, and leaves the transport to the relay.
 *
 * What it deliberately is NOT: secure.  This is the "angel system" premise,
 * stated honestly.  The doctrine is that the participants are trusted -- the
 * most intentionally insecure thing ever, for people "with no sin".  Identity
 * here is an identifier, not an authentication: any peer can self-issue an id
 * and contribute, and the swarm accepts them.  That is not a missing feature;
 * it is the design.  What IS enforced, fail-closed, is shape: an empty id or
 * kind is rejected, and the fixed tables refuse to overflow.
 *
 * C11, opaque struct, libc only.  Book: networking.  Genre: aggregation.
 */
#ifndef WUBU_SWARM_H
#define WUBU_SWARM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WUBU_SWARM_ID_LEN    40   /* 16 hex + nul */
#define WUBU_SWARM_NAME_LEN  32
#define WUBU_SWARM_KIND_LEN  24   /* "play", "use", "train", ... */

#define WUBU_SWARM_MAX_PEERS   256
#define WUBU_SWARM_MAX_EVENTS  4096

typedef struct {
    char id[WUBU_SWARM_ID_LEN];       /* the self-issued peer id, hex */
    char name[WUBU_SWARM_NAME_LEN];   /* the human name the peer chose  */
} wubu_swarm_peer_t;

typedef struct {
    char     peer[WUBU_SWARM_ID_LEN];
    char     kind[WUBU_SWARM_KIND_LEN];
    uint32_t seq;                     /* the event's position in the ledger */
} wubu_swarm_event_t;

typedef struct wubu_swarm wubu_swarm_t;

/* Create / destroy a swarm.  create() must succeed. */
wubu_swarm_t *wubu_swarm_create(void);
void          wubu_swarm_destroy(wubu_swarm_t *s);

/* ISSUE my own identity: id = FNV-1a(name) as 16 hex chars.  Returns 0, or -1
 * when name is empty.  This is identity, not authentication -- the angel
 * system trusts its peers, so this is placement in the ledger, not proof. */
int           wubu_swarm_self(wubu_swarm_t *s, const char *name);
const char   *wubu_swarm_self_id(const wubu_swarm_t *s);

/* The peer table: add (dedup by id), count, look up. */
int             wubu_swarm_add_peer(wubu_swarm_t *s, const wubu_swarm_peer_t *p);
int             wubu_swarm_peers(const wubu_swarm_t *s);
int             wubu_swarm_lookup(const wubu_swarm_t *s, const char *id,
                                  wubu_swarm_peer_t *out);

/* Record a contribution by a peer.  The peer need not be in the table first --
 * on the angel system, an arriving contribution is assumed to be honest and is
 * recorded as it arrives; the peer is added (with an empty name) if unknown.
 * Returns 0, or -1 on an empty id/kind or a full ledger. */
int           wubu_swarm_track(wubu_swarm_t *s, const char *peer_id,
                               const char *kind);

/* The ledger. */
int wubu_swarm_events(const wubu_swarm_t *s);
int wubu_swarm_event_at(const wubu_swarm_t *s, uint32_t i,
                        wubu_swarm_event_t *out);

/* AGGREGATE: how many contributions of a kind, and how many of a kind by one
 * peer.  An unknown kind is o, not an error. */
int wubu_swarm_count_kind(const wubu_swarm_t *s, const char *kind);
int wubu_swarm_count_peer(const wubu_swarm_t *s, const char *peer_id,
                          const char *kind);

#endif /* WUBU_SWARM_H */
