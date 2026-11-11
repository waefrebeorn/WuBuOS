/*
 * wubu_swarm_test.c -- the aggregation ledger must do what the vision claims:
 * peers self-issue identity, arrive untrusted and are recorded, and the ledger
 * aggregates contributions by kind and by peer.  Every assertion here is about
 * that behaviour, not about the implementation's internals.
 */
#include "wubu_swarm.h"

#include <stdio.h>
#include <string.h>

static int failures, checks;
#define CHECK(c, m) do { checks++; if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_wubu_swarm ===\n");
    wubu_swarm_t *s = wubu_swarm_create();
    CHECK(s != NULL, "the swarm exists");

    /* -- self-issued identity -- */
    CHECK(wubu_swarm_self(s, "nova") == 0, "I can issue my own identity");
    CHECK(wubu_swarm_self(s, "") != 0, "an empty name is rejected");
    CHECK(wubu_swarm_self_id(s) != NULL, "my id is assigned");
    if (wubu_swarm_self_id(s)) {
        printf("  self: %s\n", wubu_swarm_self_id(s));
        CHECK(strlen(wubu_swarm_self_id(s)) == 16, "the id is 16 hex chars");
        /* identity is a placement, stable for the same name -- not auth */
        char saved[WUBU_SWARM_ID_LEN];
        strncpy(saved, wubu_swarm_self_id(s), strlen(wubu_swarm_self_id(s)) + 1);
        CHECK(wubu_swarm_self(s, "nova") == 0, "re-selfing is allowed");
        CHECK(strcmp(saved, wubu_swarm_self_id(s)) == 0,
              "the same name yields the same id (deterministic, not secret)");
        CHECK(wubu_swarm_self(s, "other") == 0, "a different name re-issues");
        CHECK(strcmp(saved, wubu_swarm_self_id(s)) != 0,
              "a different name yields a different id");
    }

    /* -- the peer table, dedup -- */
    wubu_swarm_peer_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.id, sizeof(p.id), "deadbeefdeadbeef");
    snprintf(p.name, sizeof(p.name), "ember");
    CHECK(wubu_swarm_add_peer(s, &p) == 0, "a named peer is added");
    CHECK(wubu_swarm_add_peer(s, &p) == 0, "adding the same peer again is fine");
    CHECK(wubu_swarm_peers(s) == 1, "the peer table holds 1 (ember -- not me, I am the swarm)");
    wubu_swarm_peer_t out;
    memset(&out, 0, sizeof(out));
    CHECK(wubu_swarm_lookup(s, "deadbeefdeadbeef", &out) == 0,
          "a peer is found by id");
    CHECK(strcmp(out.name, "ember") == 0, "and carries its chosen name");
    CHECK(wubu_swarm_lookup(s, "0000000000000000", &out) != 0,
          "an unknown id is not found");
    CHECK(wubu_swarm_add_peer(s, NULL) != 0, "a NULL peer is rejected");

    /* -- contributions: untrusted arrivals are recorded (the angel premise) -- */
    CHECK(wubu_swarm_track(s, "aabbccddeeff0011", "play") == 0,
          "an unknown peer's contribution is recorded on arrival");
    wubu_swarm_peer_t ghost;
    memset(&ghost, 0, sizeof(ghost));
    CHECK(wubu_swarm_lookup(s, "aabbccddeeff0011", &ghost) == 0,
          "the arriving peer joined the table (empty name, the trust-default)");
    CHECK(wubu_swarm_peers(s) == 2, "the table now holds 2 (ember + the stranger)");
    CHECK(wubu_swarm_track(s, "deadbeefdeadbeef", "use") == 0,
          "a known peer contributes");
    CHECK(wubu_swarm_track(s, "deadbeefdeadbeef", "use") == 0,
          "and contributes again");
    CHECK(wubu_swarm_track(s, "", "play") != 0, "an empty peer id is rejected");
    CHECK(wubu_swarm_track(s, "deadbeefdeadbeef", "") != 0,
          "an empty kind is rejected");

    /* -- aggregation -- */
    CHECK(wubu_swarm_events(s) == 3, "the ledger holds 3 events");
    CHECK(wubu_swarm_count_kind(s, "play") == 1, "one 'play' in the ledger");
    CHECK(wubu_swarm_count_kind(s, "use") == 2, "two 'use' in the ledger");
    CHECK(wubu_swarm_count_kind(s, "train") == 0, "an unknown kind is zero");
    CHECK(wubu_swarm_count_peer(s, "deadbeefdeadbeef", "use") == 2,
          "ember contributed two 'use'");
    CHECK(wubu_swarm_count_peer(s, "aabbccddeeff0011", "play") == 1,
          "the stranger contributed one 'play'");

    /* ledger order is the record */
    wubu_swarm_event_t e;
    CHECK(wubu_swarm_event_at(s, 2, &e) == 0, "event 2 is readable");
    if (wubu_swarm_event_at(s, 2, &e) == 0) {
        CHECK(e.seq == 2, "the seq is the ledger position");
        CHECK(strcmp(e.kind, "use") == 0, "event 2 is a 'use'");
    }
    CHECK(wubu_swarm_event_at(s, 99, &e) != 0, "beyond the ledger is an error");

    wubu_swarm_destroy(s);

    if (failures == 0) printf("=== swarm: %d checks, 0 failed ===\n", checks);
    else printf("=== swarm: %d failed of %d ===\n", failures, checks);
    return failures ? 1 : 0;
}
