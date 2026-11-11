/*
 * wubu_swarm_wire.h -- how a contribution rides the SDR relay.
 *
 * What this is FOR: the swarm ledger records contributions, but the vision is
 * that they are TRACKED OVER SDR -- carried from one peer to another on the
 * relay layers already built.  This module is the on-wire shape of a
 * contribution: a fixed 44-byte frame that any relay (SDR or otherwise) can
 * carry as opaque bytes, and that a receiving swarm can decode back into the
 * ledger.
 *
 * It is deliberately transport-agnostic and shape-fail-closed:
 *   - encode() accepts a peer id (16 hex) and a kind, and emits the frame;
 *   - decode() validates magic, the peer shape and the kind, and refuses a
 *     malformed frame rather than guessing.
 * The sequence number is NOT on the wire: the receiving ledger assigns one, so
 * the same frame can be replayed to many swarms and each ledger orders it.
 *
 * The frame (all fields fixed, 44 bytes):
 *   0..3   magic "WMSW"
 *   4..19  peer id, 16 hex chars
 *   20..43 kind, space-padded to 24
 *
 * C11, libc only.  Part of the swarm spell (seedbook networking/aggregation).
 */
#ifndef WUBU_SWARM_WIRE_H
#define WUBU_SWARM_WIRE_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_SWARM_WIRE_LEN    44
#define WUBU_SWARM_WIRE_PEER   16   /* hex chars, no nul */
#define WUBU_SWARM_WIRE_KIND   24

/* Emit the wire frame for a contribution.  peer must be exactly 16 chars of
 * hex; kind non-empty and at most 24 chars.  Returns 0, or -1 on bad shape. */
int wubu_swarm_wire_encode(const char *peer, const char *kind,
                           uint8_t out[WUBU_SWARM_WIRE_LEN]);

/* Parse a received frame.  On success fills peer (16+NUL) and kind (trimmed,
 * NUL-terminated) and returns 0.  On a bad magic or a malformed body returns
 * -1 and leaves the outputs untouched. */
int wubu_swarm_wire_decode(const uint8_t in[WUBU_SWARM_WIRE_LEN],
                           char peer[WUBU_SWARM_WIRE_PEER + 1],
                           char kind[WUBU_SWARM_WIRE_KIND + 1]);

#endif /* WUBU_SWARM_WIRE_H */
