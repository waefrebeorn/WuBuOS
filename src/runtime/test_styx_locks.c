/*
 * test_styx_locks.c -- per-fid byte-range lock table (GOALS-1000 #719:
 * styx_locks had no test target). POSIX-style semantics per styx_locks.c:
 * two readers share, anything involving a writer conflicts, re-locking
 * the same range updates in place, unlock removes ONLY an exact-range
 * match by the owning fid, and a full table refuses.
 */
#include "styx_locks.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

int main(void)
{
    styx_locktab_t t;
    styx_locks_init(&t);
    CHECK(styx_lock_count(&t) == 0);

    /* two readers over overlapping ranges share */
    CHECK(styx_lock(&t, 1, 1, 0, 100, STYX_LOCK_RDLCK) == 0);
    CHECK(styx_lock_count(&t) == 1);
    CHECK(styx_lock(&t, 1, 2, 50, 60, STYX_LOCK_RDLCK) == 0);
    CHECK(styx_lock_count(&t) == 2);

    /* a writer conflicts with a reader's overlapping range */
    CHECK(styx_lock(&t, 1, 3, 50, 60, STYX_LOCK_WRLCK) == -1);
    CHECK(styx_lock_count(&t) == 2);
    /* a reader does NOT conflict with a writer's non-overlapping range */
    CHECK(styx_lock(&t, 1, 3, 200, 10, STYX_LOCK_RDLCK) == 0);
    CHECK(styx_lock_count(&t) == 3);

    /* a different qid never conflicts (locks are per-file) */
    CHECK(styx_lock(&t, 2, 1, 0, 100, STYX_LOCK_WRLCK) == 0);
    CHECK(styx_lock_count(&t) == 4);

    /* the same owner re-locking the exact range updates in place
     * (no duplicate stacking: one unlock must free one slot) */
    CHECK(styx_lock(&t, 3, 9, 0, 10, STYX_LOCK_RDLCK) == 0);
    CHECK(styx_lock_count(&t) == 5);
    CHECK(styx_lock(&t, 3, 9, 0, 10, STYX_LOCK_WRLCK) == 0);
    CHECK(styx_lock_count(&t) == 5);
    /* ...and the update is visible: another reader now conflicts */
    CHECK(styx_lock(&t, 3, 10, 5, 2, STYX_LOCK_RDLCK) == -1);

    /* unlock rejects a partial range (reported, not guessed) */
    CHECK(styx_unlock(&t, 3, 9, 0, 5) == -1);
    CHECK(styx_lock_count(&t) == 5);
    /* unlock rejects a different owner */
    CHECK(styx_unlock(&t, 3, 10, 0, 10) == -1);
    CHECK(styx_lock_count(&t) == 5);
    /* exact-range unlock by the owner frees exactly that slot */
    CHECK(styx_unlock(&t, 3, 9, 0, 10) == 0);
    CHECK(styx_lock_count(&t) == 4);
    /* the freed range is lockable again */
    CHECK(styx_lock(&t, 3, 10, 5, 2, STYX_LOCK_RDLCK) == 0);
    CHECK(styx_lock_count(&t) == 5);

    /* UNLCK passed through styx_lock routes to unlock */
    CHECK(styx_lock(&t, 3, 10, 5, 2, STYX_LOCK_UNLCK) == 0);
    CHECK(styx_lock_count(&t) == 4);

    /* NULL table is refused at the contract boundary */
    CHECK(styx_lock(NULL, 1, 1, 0, 1, STYX_LOCK_RDLCK) == -1);
    CHECK(styx_unlock(NULL, 1, 1, 0, 1) == -1);

    /* the table exhausts and says so */
    styx_locks_init(&t);
    for (int i = 0; i < STYX_LOCK_MAX; i++) {
        CHECK(styx_lock(&t, 100 + i, 1, 0, 1, STYX_LOCK_RDLCK) == 0);
    }
    CHECK(styx_lock_count(&t) == STYX_LOCK_MAX);
    CHECK(styx_lock(&t, 999, 1, 0, 1, STYX_LOCK_RDLCK) == -1);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
