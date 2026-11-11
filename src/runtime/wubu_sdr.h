/*
 * wubu_sdr.h -- Steam Datagram Relay: routing tickets and virtual routes.
 *
 * Steam SDR gives every user a VIRTUAL IP and routes UDP packets through a POP
 * (point of presence).  A client does not learn its peer's real address -- it
 * learns a virtual one, and a ROUTING TICKET authorizes the connection.
 *
 * The rules that matter, all of them security-relevant:
 *
 *   * A ticket is for ONE connection: it names a source and destination
 *     virtual address, so it cannot be replayed against another peer or port.
 *   * A ticket EXPIRES.  A ticket that never expires is a permanent
 *     authorization, which is exactly what a relay must not hand out.
 *   * A ticket's window must be sane: issued_at <= expires_at, and the window
 *     is capped (a 1-year ticket is not a session ticket).
 *   * A route maps a virtual address to a POP and a peer.  Two routes may not
 *     claim the same virtual address -- a hijacked address is a hijacked
 *     session.
 *   * A route must be READY before packets are forwarded to it, or the POP
 *     black-holes them.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_SDR_H
#define WUBU_SDR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A virtual address: a 32-bit VIP plus a 16-bit virtual port. */
typedef struct {
    uint32_t vip;
    uint16_t vport;
} wubu_sdr_addr_t;

typedef struct {
    uint32_t pop_id;
    wubu_sdr_addr_t src;
    wubu_sdr_addr_t dst;
    uint64_t issued_at;      /* unix seconds */
    uint64_t expires_at;     /* unix seconds */
} wubu_sdr_ticket_t;

#define WUBU_SDR_MAX_TICKET_WINDOW 86400u   /* one day */

/* NULL when the ticket is well formed, else a static reason. */
const char *wubu_sdr_ticket_validate(const wubu_sdr_ticket_t *t);

/* Encode to text (base64 of the canonical little-endian fields plus a
 * checksum).  Returns the number of bytes written, or -1 when the buffer is
 * too small. */
long wubu_sdr_ticket_encode(const wubu_sdr_ticket_t *t, char *out, size_t n);

/* Decode a ticket.  Returns false on a malformed string, a bad checksum, or a
 * structurally invalid ticket -- all three must be indistinguishable to a
 * caller, so none of them leaks WHY. */
bool wubu_sdr_ticket_decode(const char *in, wubu_sdr_ticket_t *out);

/* Is the ticket usable at `now`?  Not before issued_at, not at/after
 * expires_at. */
bool wubu_sdr_ticket_is_valid(const wubu_sdr_ticket_t *t, uint64_t now);

/* Does this ticket authorize a packet from `src` to `dst`?  BOTH must match. */
bool wubu_sdr_ticket_matches(const wubu_sdr_ticket_t *t,
                             const wubu_sdr_addr_t *src,
                             const wubu_sdr_addr_t *dst);

/* -- the route table ------------------------------------------------ */

#define WUBU_SDR_MAX_ROUTES 64

typedef struct {
    wubu_sdr_addr_t addr;
    uint32_t        pop_id;
    uint64_t        peer_id;
    bool            ready;
} wubu_sdr_route_t;

typedef struct {
    uint32_t          count;
    wubu_sdr_route_t  routes[WUBU_SDR_MAX_ROUTES];
} wubu_sdr_table_t;

void wubu_sdr_table_init(wubu_sdr_table_t *t);

/* Add a route.  Returns -1 when the table is full, or when the address is
 * already claimed (a duplicate is a hijack attempt, not an update). */
int wubu_sdr_table_add(wubu_sdr_table_t *t, const wubu_sdr_route_t *r);

/* Find the route for a virtual address, or NULL. */
const wubu_sdr_route_t *wubu_sdr_table_find(const wubu_sdr_table_t *t,
                                            const wubu_sdr_addr_t *a);

/* Mark a route ready.  Returns -1 for an unknown address. */
int wubu_sdr_table_set_ready(wubu_sdr_table_t *t,
                             const wubu_sdr_addr_t *a, bool ready);

/* How many routes are ready to forward? */
uint32_t wubu_sdr_table_ready_count(const wubu_sdr_table_t *t);

#endif /* WUBU_SDR_H */
