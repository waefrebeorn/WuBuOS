/*
 * wubu_dualcontrol.h -- kill-switch rearm dual authorization (AGIEDR #42).
 *
 * The AGI kill switch, once tripped, requires TWO DISTINCT authorized
 * principals to vote before it can be re-armed: one operator must never
 * be able to untrip the safety alone. Every transition -- and every
 * REJECTED vote -- is recorded in the kernel audit chain
 * (wubu_audit_record, WUBU_AUDIT_SECURITY), so the rearm history is
 * tamper-evident end to end.
 *
 * C11, freestanding-compatible (static state, no malloc).
 */
#ifndef WUBU_DUALCONTROL_H
#define WUBU_DUALCONTROL_H

#include <stdint.h>

typedef enum {
    WUBU_DC_ARMED   = 0,
    WUBU_DC_TRIPPED = 1
} wubu_dualcontrol_state_t;

/* Start ARMED (boot state). Safe to call repeatedly. */
void wubu_dualcontrol_init(void);

/* Back to the boot state without history (tests + deliberate teardown). */
void wubu_dualcontrol_reset(void);

/* Current state. */
int wubu_dualcontrol_state(void);

/* Trip the kill switch. reason is recorded in the audit chain.
 * 0 = tripped; -1 = already tripped; -2 = bad args. */
int wubu_dualcontrol_trip(const char *reason);

/* A principal votes to rearm. Two DISTINCT principals are required:
 * the same principal voting twice does not count. Returns the number
 * of votes STILL NEEDED after this one (0 = the switch is rearmed),
 * or -1 when there is nothing to rearm / bad args. */
int wubu_dualcontrol_vote_rearm(uint64_t principal_id);

/* Diagnostics: the two principal ids on record for the current rearm
 * (0 when unset), and the total audit records this module emitted. */
uint64_t wubu_dualcontrol_voter(int i);
int      wubu_dualcontrol_audit_records(void);

#endif /* WUBU_DUALCONTROL_H */
