/*
 * test_wubu_dualcontrol.c -- asserts the kill-switch two-party rearm rule
 * (AGIEDR #42): trip, distinct-principal votes, audit integration.
 */
#include "wubu_dualcontrol.h"
#include "wubu_audit.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

int main(void)
{
    wubu_audit_reset();
    wubu_dualcontrol_init();
    CHECK(wubu_dualcontrol_state() == WUBU_DC_TRIPPED);  /* boots tripped */
    CHECK(wubu_dualcontrol_voter(0) == 0 && wubu_dualcontrol_voter(1) == 0);
    CHECK(wubu_dualcontrol_voter(2) == 0);
    size_t audit0 = wubu_audit_count();

    /* rearm requires TWO DISTINCT principals */
    CHECK(wubu_dualcontrol_vote_rearm(1001) == 1);       /* one more needed */
    CHECK(wubu_dualcontrol_state() == WUBU_DC_TRIPPED);
    CHECK(wubu_dualcontrol_voter(0) == 1001);
    /* the SAME principal again does not count */
    CHECK(wubu_dualcontrol_vote_rearm(1001) == 1);
    CHECK(wubu_dualcontrol_state() == WUBU_DC_TRIPPED);
    CHECK(wubu_dualcontrol_voter(0) == 1001);            /* not overwritten */
    /* a second distinct principal rearms */
    CHECK(wubu_dualcontrol_vote_rearm(2002) == 0);
    CHECK(wubu_dualcontrol_state() == WUBU_DC_ARMED);
    CHECK(wubu_dualcontrol_voter(0) == 0 && wubu_dualcontrol_voter(1) == 0);
    /* votes with nothing to rearm are rejected */
    CHECK(wubu_dualcontrol_vote_rearm(3003) == -1);

    /* ---- trip + re-arm cycle ---- */
    CHECK(wubu_dualcontrol_trip("agent diverged") == 0);
    CHECK(wubu_dualcontrol_state() == WUBU_DC_TRIPPED);
    CHECK(wubu_dualcontrol_trip("again") == -1);         /* already tripped */
    CHECK(wubu_dualcontrol_trip(NULL) == -2);
    CHECK(wubu_dualcontrol_vote_rearm(1001) == 1);
    CHECK(wubu_dualcontrol_vote_rearm(2002) == 0);
    CHECK(wubu_dualcontrol_state() == WUBU_DC_ARMED);

    /* ---- the audit chain carries the whole history ---- */
    size_t audit1 = wubu_audit_count();
    CHECK(audit1 > audit0);
    CHECK(audit1 - audit0 == (size_t)wubu_dualcontrol_audit_records());
    int saw_trip = 0, saw_rearm = 0, saw_dup = 0;
    for (size_t i = 0; i < wubu_audit_count(); i++) {
        const wubu_audit_rec_t *r = wubu_audit_at(i);
        if (!r) break;
        if (strstr(r->msg, "TRIPPED")) saw_trip = 1;
        if (strstr(r->msg, "REARMED")) saw_rearm = 1;
        if (strstr(r->msg, "duplicate principal")) saw_dup = 1;
    }
    CHECK(saw_trip && saw_rearm && saw_dup);
    CHECK(wubu_audit_verify() == 0);                     /* chain intact */

    /* ---- reset restores the boot state ---- */
    wubu_dualcontrol_reset();
    CHECK(wubu_dualcontrol_state() == WUBU_DC_TRIPPED);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
