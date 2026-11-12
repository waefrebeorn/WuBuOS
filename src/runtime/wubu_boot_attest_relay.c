/*
 * wubu_boot_attest_relay.c -- publish the sealed boot report over SDR.
 *
 * C11, minimal includes.  The report comes from the boot attestation spell;
 * the decision comes from the SDR relay; this module only walks the route
 * table and hands over the bytes.
 */
#include "wubu_boot_attest_relay.h"

#include <string.h>

/* The relay table is a fixed array with no iterator, and its route type is
 * already public (wubu_sdr.h), so walking it here is the same read the relay
 * itself does.  WUBU_SDR_MAX_ROUTES is the bound. */
int wubu_boot_attest_publish(const wubu_boot_attest_t *report,
                             const wubu_sdr_table_t *routes,
                             const wubu_sdr_addr_t *from,
                             const wubu_sdr_ticket_t *ticket,
                             uint64_t now,
                             wubu_boot_attest_tx tx, void *ctx)
{
    if (!report || !routes || !tx) return -1;

    uint8_t frame[WUBU_BOOT_ATTEST_FRAME_SZ];
    long len = wubu_boot_attest_encode(report, frame, sizeof(frame));
    if (len <= 0) return -1;                 /* not sealed, or a bad buffer */

    int offered = 0;
    for (uint32_t i = 0; i < routes->count && i < WUBU_SDR_MAX_ROUTES; i++) {
        const wubu_sdr_route_t *r = &routes->routes[i];
        if (!r->ready) continue;             /* the relay would black-hole it */

        /* The same decision a data packet gets: no route, no ticket, an
         * expired or mismatched ticket, a loopback, a rate-limited peer --
         * the boot report buys its way past none of them. */
        wubu_sdr_decision_t d = wubu_sdr_relay(routes, from, &r->addr,
                                               ticket, now);
        if (d != WUBU_SDR_FORWARD) continue;

        offered++;
        tx(ctx, r->peer_id, frame, (size_t)len);
    }
    return offered;
}
