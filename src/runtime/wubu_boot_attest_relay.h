/*
 * wubu_boot_attest_relay.h -- publish the sealed boot report over SDR.
 *
 * The boot attestation spell (src/spells/networking/aggregation/bootattest)
 * builds and verifies the REPORT.  The SDR relay
 * (src/runtime/wubu_sdr_relay.c) decides about PACKETS.  This module is the
 * seam between them: it takes a sealed report and offers it to every peer the
 * relay table knows, through the relay's own decision, and hands the bytes to
 * a transmit callback.
 *
 * Why a callback and not a socket: the tree's SDR layer is deliberately a
 * DECISION layer (ticket, route, rate limit) with no transport of its own --
 * the same split the swarm spell documents ("the swarm is carried over SDR,
 * but this module itself is pure").  Who may receive a boot report is the
 * relay's call; how the bytes move is the caller's.
 *
 * "Always reported to everyone peer wise": the report is offered to EVERY
 * route in the table, not to one address.  A route that is not ready, or a
 * ticket that does not match, is skipped by the relay's own rules -- the
 * report does not get to bypass them.
 *
 * C11, opaque-free (the relay table is already an open struct), minimal
 * includes.
 */
#ifndef WUBU_BOOT_ATTEST_RELAY_H
#define WUBU_BOOT_ATTEST_RELAY_H

#include <stddef.h>
#include <stdint.h>

#include "wubu_sdr.h"
#include "wubu_sdr_relay.h"
/* The boot attestation is a spell; the runtime reaches it by path so the
 * spell stays self-contained (it needs no -I beyond its own directory). */
#include "../spells/networking/aggregation/bootattest/wubu_boot_attest.h"

/* One transmit: `peer_id` is the route's peer, `frame`/`len` the sealed boot
 * report.  Return 0 on success, -1 if the transport refused it. */
typedef int (*wubu_boot_attest_tx)(void *ctx, uint64_t peer_id,
                                   const uint8_t *frame, size_t len);

/* Offer a sealed boot report to every route in `routes`.  The relay decision
 * is taken per peer (from -> route->addr, with `ticket`), so a peer that is
 * not routable, not ready, or not ticketed gets nothing and the report is
 * never allowed past the relay's rules.  Returns the number of peers the
 * report was OFFERED to (the relay said FORWARD), or -1 when the report is
 * not sealed or the arguments are bad.  `tx` is called once per forward. */
int wubu_boot_attest_publish(const wubu_boot_attest_t *report,
                             const wubu_sdr_table_t *routes,
                             const wubu_sdr_addr_t *from,
                             const wubu_sdr_ticket_t *ticket,
                             uint64_t now,
                             wubu_boot_attest_tx tx, void *ctx);

#endif /* WUBU_BOOT_ATTEST_RELAY_H */
