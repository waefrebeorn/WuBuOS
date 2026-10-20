/*
 * wubu_dualcontrol.c -- kill-switch rearm dual authorization (AGIEDR #42).
 * See wubu_dualcontrol.h. Every state change and rejected vote lands in
 * the hash-chained kernel audit log (wubu_audit.c).
 */
#include "wubu_dualcontrol.h"
#include "wubu_audit.h"

#include <string.h>
#include <stdio.h>

static wubu_dualcontrol_state_t g_dc_state;
static uint64_t g_dc_voters[2];     /* the distinct principals on record */
static int      g_dc_nvoters;
static int      g_dc_records;      /* audit records this module emitted */

static void dc_audit(const char *what, uint64_t principal)
{
    char msg[WUBU_AUDIT_MSG_SZ];
    snprintf(msg, sizeof(msg), "dualcontrol: %s by %llu", what,
             (unsigned long long)principal);
    wubu_audit_record(WUBU_AUDIT_SECURITY, 0, 0, msg);
    g_dc_records++;
}

void wubu_dualcontrol_init(void)
{
    g_dc_state = WUBU_DC_TRIPPED;   /* boot tripped: rearm is deliberate */
    g_dc_voters[0] = g_dc_voters[1] = 0;
    g_dc_nvoters = 0;
    g_dc_records = 0;
}

void wubu_dualcontrol_reset(void)
{
    wubu_dualcontrol_init();
    dc_audit("boot state (tripped, awaiting two-party rearm)", 0);
}

int wubu_dualcontrol_state(void)
{
    return (int)g_dc_state;
}

int wubu_dualcontrol_trip(const char *reason)
{
    if (!reason) return -2;
    if (g_dc_state == WUBU_DC_TRIPPED) {
        dc_audit("trip rejected (already tripped)", 0);
        return -1;
    }
    g_dc_state = WUBU_DC_TRIPPED;
    g_dc_voters[0] = g_dc_voters[1] = 0;
    g_dc_nvoters = 0;
    char msg[WUBU_AUDIT_MSG_SZ];
    snprintf(msg, sizeof(msg), "dualcontrol: TRIPPED (%.80s)", reason);
    wubu_audit_record(WUBU_AUDIT_SECURITY, 0, 0, msg);
    g_dc_records++;
    return 0;
}

int wubu_dualcontrol_vote_rearm(uint64_t principal_id)
{
    if (g_dc_state != WUBU_DC_TRIPPED) {
        dc_audit("rearm vote rejected (switch armed)", principal_id);
        return -1;
    }
    /* The same principal twice does not satisfy the two-party rule. */
    for (int i = 0; i < g_dc_nvoters; i++) {
        if (g_dc_voters[i] == principal_id) {
            dc_audit("rearm vote rejected (duplicate principal)",
                     principal_id);
            return 1;                       /* still need a second party */
        }
    }
    if (g_dc_nvoters < 2)
        g_dc_voters[g_dc_nvoters++] = principal_id;
    dc_audit("rearm vote accepted", principal_id);
    if (g_dc_nvoters >= 2) {
        g_dc_state = WUBU_DC_ARMED;
        g_dc_voters[0] = g_dc_voters[1] = 0;
        g_dc_nvoters = 0;
        dc_audit("REARMED (two-party rule satisfied)", principal_id);
        return 0;
    }
    return 1;                               /* one more distinct principal */
}

uint64_t wubu_dualcontrol_voter(int i)
{
    if (i != 0 && i != 1) return 0;
    return g_dc_voters[i];
}

int wubu_dualcontrol_audit_records(void)
{
    return g_dc_records;
}
