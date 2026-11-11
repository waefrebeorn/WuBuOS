/*
 * wubu_sdr_test.c -- SDR routing tickets and the virtual route table.
 *
 * The load-bearing rules: a ticket is for ONE connection and it EXPIRES, a
 * tampered ticket is indistinguishable from a malformed one, and a virtual
 * address may be claimed only once.
 */
#include "wubu_sdr.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void ticket(wubu_sdr_ticket_t *t)
{
    memset(t, 0, sizeof(*t));
    t->pop_id = 0x1234;
    t->src.vip = 0x0A000001u;
    t->src.vport = 27015;
    t->dst.vip = 0x0A000002u;
    t->dst.vport = 27016;
    t->issued_at = 1000;
    t->expires_at = 2000;
}

int main(void)
{
    printf("=== test_wubu_sdr ===\n");
    wubu_sdr_ticket_t t, d;
    char buf[128];

    ticket(&t);
    CHECK(wubu_sdr_ticket_validate(&t) == NULL, "a normal ticket is legal");

    /* -- structural validation -- */
    ticket(&t); t.pop_id = 0;
    CHECK(strcmp(wubu_sdr_ticket_validate(&t), "a ticket must name a POP") == 0,
          "a zero POP is reported");
    ticket(&t); t.src.vip = 0;
    CHECK(wubu_sdr_ticket_validate(&t) != NULL, "a zero virtual address is reported");
    ticket(&t); t.dst.vport = 0;
    CHECK(strcmp(wubu_sdr_ticket_validate(&t), "a virtual port is zero") == 0,
          "a zero virtual port is reported");
    ticket(&t); t.expires_at = t.issued_at - 1;
    CHECK(strcmp(wubu_sdr_ticket_validate(&t),
                 "the ticket expires before it is issued") == 0,
          "an expiry before the issue time is reported");
    ticket(&t); t.issued_at = 0; t.expires_at = WUBU_SDR_MAX_TICKET_WINDOW + 1;
    CHECK(strcmp(wubu_sdr_ticket_validate(&t),
                 "the ticket window is longer than the maximum") == 0,
          "an over-long window is reported (a permanent authorization)");
    ticket(&t); t.expires_at = WUBU_SDR_MAX_TICKET_WINDOW;
    CHECK(wubu_sdr_ticket_validate(&t) == NULL,
          "a window of exactly the maximum is legal");
    CHECK(wubu_sdr_ticket_validate(NULL) != NULL, "a null ticket is rejected");

    /* -- encode / decode round trip -- */
    ticket(&t);
    long n = wubu_sdr_ticket_encode(&t, buf, sizeof(buf));
    CHECK(n == 48, "the encoded ticket is 48 base64 characters");
    CHECK(wubu_sdr_ticket_decode(buf, &d), "it decodes");
    CHECK(memcmp(&t, &d, sizeof(t)) == 0, "the decoded ticket is byte-identical");

    /* -- a tampered ticket is rejected -- */
    buf[0] = (buf[0] == 'A') ? 'B' : 'A';       /* flip the first character */
    CHECK(!wubu_sdr_ticket_decode(buf, &d),
          "a TAMPERED ticket is rejected (the checksum covers every field)");
    ticket(&t);
    wubu_sdr_ticket_encode(&t, buf, sizeof(buf));
    CHECK(!wubu_sdr_ticket_decode("short", &d), "a short string is rejected");
    CHECK(!wubu_sdr_ticket_decode("", &d), "an empty string is rejected");
    {
        char bad[49];
        memcpy(bad, buf, 49);
        bad[10] = '!';                           /* not in the alphabet */
        CHECK(!wubu_sdr_ticket_decode(bad, &d),
              "a non-base64 character is rejected");
    }
    CHECK(!wubu_sdr_ticket_decode(NULL, &d), "a null string is rejected");
    CHECK(!wubu_sdr_ticket_decode(buf, NULL), "a null out pointer is rejected");
    CHECK(wubu_sdr_ticket_encode(&t, buf, 10) == -1,
          "a buffer that is too small is refused");
    CHECK(wubu_sdr_ticket_encode(NULL, buf, sizeof(buf)) == -1,
          "a null ticket does not encode");

    /* -- the ticket is for ONE connection -- */
    ticket(&t);
    wubu_sdr_addr_t src = { 0x0A000001u, 27015 }, dst = { 0x0A000002u, 27016 };
    CHECK(wubu_sdr_ticket_matches(&t, &src, &dst),
          "the ticket matches its own addresses");
    wubu_sdr_addr_t wrong_dst = { 0x0A000003u, 27016 };
    CHECK(!wubu_sdr_ticket_matches(&t, &src, &wrong_dst),
          "a different destination does NOT match");
    wubu_sdr_addr_t wrong_port = { 0x0A000002u, 27017 };
    CHECK(!wubu_sdr_ticket_matches(&t, &src, &wrong_port),
          "a different port does NOT match (a replay to another port)");
    wubu_sdr_addr_t wrong_src = { 0x0A000009u, 27015 };
    CHECK(!wubu_sdr_ticket_matches(&t, &wrong_src, &dst),
          "a different source does NOT match");

    /* -- expiry -- */
    ticket(&t);
    CHECK(!wubu_sdr_ticket_is_valid(&t, 999), "not valid before it is issued");
    CHECK(wubu_sdr_ticket_is_valid(&t, 1000), "valid at the issue time");
    CHECK(wubu_sdr_ticket_is_valid(&t, 1500), "valid inside the window");
    CHECK(!wubu_sdr_ticket_is_valid(&t, 2000),
          "NOT valid AT the expiry instant (the window is half-open)");
    CHECK(!wubu_sdr_ticket_is_valid(&t, 2001), "not valid after it expires");
    CHECK(!wubu_sdr_ticket_is_valid(NULL, 1500), "a null ticket is never valid");

    /* -- the route table -- */
    {
        wubu_sdr_table_t tab;
        wubu_sdr_table_init(&tab);
        CHECK(tab.count == 0, "the table starts empty");

        wubu_sdr_route_t r;
        memset(&r, 0, sizeof(r));
        r.addr.vip = 0x0A000001u; r.addr.vport = 27015;
        r.pop_id = 0x1234; r.peer_id = 0xDEAD; r.ready = false;
        CHECK(wubu_sdr_table_add(&tab, &r) == 0, "a route is added");
        CHECK(tab.count == 1, "one route");
        CHECK(wubu_sdr_table_find(&tab, &r.addr) != NULL, "it is found by address");
        CHECK(!wubu_sdr_table_find(&tab, &r.addr)->ready,
              "a new route is NOT ready to forward");
        CHECK(wubu_sdr_table_ready_count(&tab) == 0, "nothing is ready yet");

        /* a duplicate address is a hijack attempt, not an update */
        CHECK(wubu_sdr_table_add(&tab, &r) == -1,
              "a DUPLICATE virtual address is refused (a hijack attempt)");
        CHECK(tab.count == 1, "and it did not overwrite the first route");

        /* ready is changed through set_ready, not by re-adding */
        CHECK(wubu_sdr_table_set_ready(&tab, &r.addr, true) == 0, "the route is made ready");
        CHECK(wubu_sdr_table_find(&tab, &r.addr)->ready, "it is ready now");
        CHECK(wubu_sdr_table_ready_count(&tab) == 1, "one route is ready");
        CHECK(wubu_sdr_table_set_ready(&tab, &r.addr, false) == 0, "and can be withdrawn");
        CHECK(wubu_sdr_table_ready_count(&tab) == 0, "nothing ready again");

        wubu_sdr_addr_t unknown = { 0x0A999999u, 1 };
        CHECK(wubu_sdr_table_set_ready(&tab, &unknown, true) == -1,
              "an unknown address cannot be made ready");
        CHECK(wubu_sdr_table_find(&tab, &unknown) == NULL,
              "an unknown address is not found");

        /* the table is bounded */
        for (int i = 1; i < WUBU_SDR_MAX_ROUTES; i++) {
            wubu_sdr_route_t more;
            memset(&more, 0, sizeof(more));
            more.addr.vip = 0x0B000000u + (uint32_t)i;
            more.addr.vport = (uint16_t)(30000 + i);
            more.pop_id = 1;
            if (wubu_sdr_table_add(&tab, &more) != 0) {
                CHECK(false, "the table filled earlier than its bound");
                break;
            }
        }
        CHECK(tab.count == WUBU_SDR_MAX_ROUTES, "the table reaches its bound");
        wubu_sdr_route_t extra;
        memset(&extra, 0, sizeof(extra));
        extra.addr.vip = 0x0C000000u; extra.addr.vport = 1;
        CHECK(wubu_sdr_table_add(&tab, &extra) == -1,
              "a full table refuses new routes");

        CHECK(wubu_sdr_table_ready_count(NULL) == 0, "a null table has none ready");
        CHECK(wubu_sdr_table_find(NULL, &r.addr) == NULL, "a null table finds nothing");
        CHECK(wubu_sdr_table_add(NULL, &r) == -1, "a null table cannot add");
        wubu_sdr_table_init(NULL);
    }

    if (failures == 0) printf("ALL WUBU_SDR TESTS PASSED\n");
    else printf("%d WUBU_SDR FAILURES\n", failures);
    return failures ? 1 : 0;
}
