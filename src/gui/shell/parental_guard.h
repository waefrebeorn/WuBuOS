/*
 * parental_guard.h -- turn the family policy into something the user FEELS: the session locks itself
 * when the clock leaves the allowed window, and the lock screen says why.
 *
 * The decision is pure (src/runtime/wubu_parental.c and its gate); this file only carries it into the
 * frame loop. It is throttled: reading a file and calling localtime every frame would be a cost paid
 * for nothing, and the answer cannot change more than once a minute.
 *
 * C11.
 */
#ifndef PARENTAL_GUARD_H
#define PARENTAL_GUARD_H

#include <stdbool.h>
#include <time.h>

/* Check at most once per `interval_s` seconds. Returns true when this call actually checked, so a
 * caller (and a gate) can tell "allowed" from "did not look". */
bool wubu_parental_enforce(time_t now, int interval_s);

/* Forget the throttle and the last lock (used by the panel after the policy changes). */
void wubu_parental_guard_reset(void);

#endif
