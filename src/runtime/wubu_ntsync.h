/* wubu_ntsync.h -- NT keyed-event object semantics (the SteamOS ntsync take).
 *
 * SteamOS's neptune kernel ships `ntsync.ko`, Valve's NT synchronization kernel
 * primitive. WuBuOS's NT personality (the "slerm the NT stack" goal) maps most of
 * the sync surface onto Linux primitives already (mutant -> recursive pthread mutex,
 * semaphore -> sem_t, event -> eventfd, in vsl_nt_sync.c). The ONE surface that is
 * only approximated by the futex fallback is the KEYED EVENT, whose whole point is
 * exact one-release/one-waiter pairing on an arbitrary caller key.
 *
 * This module is that pairing primitive: a concurrent keyed-event object holding,
 * per key, a pending-release imbalance and a waiter set. Semantics (Wine/ReactOS
 * keyed-event model, the "release is held until a waiter consumes it" variant --
 * strictly more robust than the Release-with-Wait=False "drop it" variant and what
 * SRW locks actually need):
 *   - Wait(key, timeout): if a release is pending for `key`, consume the NEWEST
 *     pending one and return acquired immediately; otherwise block, waking exactly
 *     the waiters on that key.
 *   - Release(key, wait, timeout): make one release pending; wake one waiter that is
 *     blocked on the same key. If `wait` is set and no waiter is present, the
 *     releaser blocks until some waiter consumes the offered release (guarantees
 *     the release/acquire alternation used by pair locks).
 *
 * C11, pthreads. Fail-closed: bad key sign / bad slot returns -1 and changes nothing.
 * Blocking is real (cond + mutex), matching the multithreaded VSL bridge. */
#ifndef WUBU_NTSYNC_H
#define WUBU_NTSYNC_H

#include <stdint.h>

typedef struct wubu_ntsync wubu_ntsync_t;

/* Create a keyed-event object. Returns NULL on alloc failure. */
wubu_ntsync_t *wubu_ntsync_create(void);
void           wubu_ntsync_destroy(wubu_ntsync_t *ke);

/* Constants returned (0 acquired, -1 not-acquired/timeout, -2 aborted). */
#define WUBU_NTSYNC_OK      0
#define WUBU_NTSYNC_TIMEOUT -1
#define WUBU_NTSYNC_ABORTED -2

/* Wait on `key`. timeout_ms < 0 = infinite. Returns WUBU_NTSYNC_OK when a pending
 * release for `key` was consumed; WUBU_NTSYNC_TIMEOUT if none arrived in time. */
int wubu_ntsync_wait(wubu_ntsync_t *ke, uint64_t key, int64_t timeout_ms);

/* Release `key`. If `wait` and no waiter is present on `key`, block until a waiter
 * consumes the offered release (or timeout_ms elapses). Returns WUBU_NTSYNC_OK when
 * a release was offered to a waiter / consumed; WUBU_NTSYNC_TIMEOUT when `wait` was
 * set and timed out before any waiter consumed it. */
int wubu_ntsync_release(wubu_ntsync_t *ke, uint64_t key, int wait, int64_t timeout_ms);

/* Number of distinct keys currently live in the object (for diagnostics/tests). */
int wubu_ntsync_live_keys(const wubu_ntsync_t *ke);

#endif /* WUBU_NTSYNC_H */