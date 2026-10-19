/* styx_locks.c -- see styx_locks.h. Overlap = same qid and ranges intersect. */
#include "styx_locks.h"

#include <string.h>

static const uint64_t END = ~0ull;

static void range(const styx_lock_t *l, uint64_t *a, uint64_t *b)
{
    *a = l->start;
    *b = (l->length == 0) ? END : l->start + l->length - 1;
}

static int overlaps(uint64_t s1, uint64_t e1, uint64_t s2, uint64_t e2)
{
    return !(e1 < s2 || e2 < s1);
}

void styx_locks_init(styx_locktab_t *t)
{
    if (t) memset(t, 0, sizeof *t);
}

int styx_lock(styx_locktab_t *t, uint64_t qid, int32_t owner,
              uint64_t start, uint64_t length, int type)
{
    if (!t) return -1;

    uint64_t e = (length == 0) ? END : start + length - 1;

    if (type != STYX_LOCK_UNLCK) {
        for (int i = 0; i < STYX_LOCK_MAX; i++) {
            styx_lock_t *l = &t->locks[i];
            if (!l->in_use || l->qid != qid || l->owner == owner) continue;
            uint64_t a, b;
            range(l, &a, &b);
            if (!overlaps(start, e, a, b)) continue;
            /* Two readers may share; anything involving a writer conflicts. */
            if (l->type == STYX_LOCK_WRLCK || type == STYX_LOCK_WRLCK) return -1;
        }
    }

    if (type == STYX_LOCK_UNLCK)
        return styx_unlock(t, qid, owner, start, length);

    /* Re-locking the same range updates the existing entry instead of stacking
     * a duplicate (which would make one unlock free two slots). */
    for (int i = 0; i < STYX_LOCK_MAX; i++) {
        styx_lock_t *l = &t->locks[i];
        if (l->in_use && l->qid == qid && l->owner == owner &&
            l->start == start && l->length == length) {
            l->type = type;
            return 0;
        }
    }

    for (int i = 0; i < STYX_LOCK_MAX; i++) {
        if (!t->locks[i].in_use) {
            t->locks[i].in_use = 1;
            t->locks[i].qid = qid;
            t->locks[i].owner = owner;
            t->locks[i].start = start;
            t->locks[i].length = length;
            t->locks[i].type = type;
            return 0;
        }
    }
    return -1;
}

int styx_unlock(styx_locktab_t *t, uint64_t qid, int32_t owner,
                uint64_t start, uint64_t length)
{
    if (!t) return -1;
    uint64_t e = (length == 0) ? END : start + length - 1;
    int hit = 0;

    for (int i = 0; i < STYX_LOCK_MAX; i++) {
        styx_lock_t *l = &t->locks[i];
        if (!l->in_use || l->qid != qid || l->owner != owner) continue;
        uint64_t a, b;
        range(l, &a, &b);
        if (!overlaps(start, e, a, b)) continue;
        /* Only a lock fully contained in the request is removed outright;
         * partial unlocks are reported as unsupported rather than guessed. */
        if (a == start && b == e) { l->in_use = 0; hit = 1; }
    }
    return hit ? 0 : -1;
}

int styx_lock_count(const styx_locktab_t *t)
{
    int n = 0;
    if (!t) return 0;
    for (int i = 0; i < STYX_LOCK_MAX; i++) if (t->locks[i].in_use) n++;
    return n;
}
