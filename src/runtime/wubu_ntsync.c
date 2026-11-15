#include "wubu_ntsync.h"
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* Keyed-event object: a small fixed table of per-key slots, each the rendezvous
 * point for exactly one release-token. `pending` is the count of offered-but-
 * unconsumed releases on that key; a waiter consumes one and bumps `consumed_gen`.
 * A releaser that asked to pair (wait≠0) blocks until a waiter bumps `consumed_gen`
 * past the value it observed when it added its token -- i.e. until its own offer
 * was actually consumed, preserving the release/acquire alternation pair locks need.
 *
 * Table is open-addressed by exact key (linear probe); pairing is correct only if
 * Wait and Release for the same key land on the same slot, which the exact-key
 * probe guarantees. A full table fails closed (WUBU_NTSYNC_ABORTED, nothing changed). */

#define NTKEY_SLOTS 32

typedef struct {
    uint64_t      key;
    long          pending;          /* offered, not yet consumed */
    long          consumed_gen;     /* bumped by every consume on this key */
    long          n_waiters;        /* threads currently blocked here */
    int           used;
    pthread_cond_t cv;
} ntkey_slot_t;

struct wubu_ntsync {
    pthread_mutex_t mtx;
    ntkey_slot_t    slot[NTKEY_SLOTS];
};

wubu_ntsync_t *wubu_ntsync_create(void) {
    wubu_ntsync_t *ke = calloc(1, sizeof(*ke));
    if (!ke) return NULL;
    if (pthread_mutex_init(&ke->mtx, NULL) != 0) { free(ke); return NULL; }
    for (int i = 0; i < NTKEY_SLOTS; i++)
        pthread_cond_init(&ke->slot[i].cv, NULL);
    return ke;
}

void wubu_ntsync_destroy(wubu_ntsync_t *ke) {
    if (!ke) return;
    for (int i = 0; i < NTKEY_SLOTS; i++)
        pthread_cond_destroy(&ke->slot[i].cv);
    pthread_mutex_destroy(&ke->mtx);
    free(ke);
}

/* Locate the slot holding `key`, or a free slot to take it. Returns index or -1
 * when the table is full and `key` is not resident. Caller holds the mutex. */
static int ntkey_slot(wubu_ntsync_t *ke, uint64_t key, int create) {
    int free_i = -1;
    for (int i = 0; i < NTKEY_SLOTS; i++) {
        if (ke->slot[i].used && ke->slot[i].key == key)
            return i;
        if (free_i < 0 && !ke->slot[i].used)
            free_i = i;
    }
    if (create && free_i >= 0) {
        ke->slot[free_i].used   = 1;
        ke->slot[free_i].key    = key;
        ke->slot[free_i].pending    = 0;
        ke->slot[free_i].consumed_gen = 0;
        ke->slot[free_i].n_waiters   = 0;
        pthread_cond_init(&ke->slot[free_i].cv, NULL);
        return free_i;
    }
    return -1;
}

int wubu_ntsync_wait(wubu_ntsync_t *ke, uint64_t key, int64_t timeout_ms) {
    if (!ke) return WUBU_NTSYNC_ABORTED;
    int rc = pthread_mutex_lock(&ke->mtx);
    if (rc != 0) return WUBU_NTSYNC_ABORTED;
    int s = ntkey_slot(ke, key, 1);
    if (s < 0) { pthread_mutex_unlock(&ke->mtx); return WUBU_NTSYNC_ABORTED; }
    ntkey_slot_t *sl = &ke->slot[s];
    int out = WUBU_NTSYNC_OK;
    for (;;) {
        if (sl->pending > 0) {         /* a release is already held for us */
            sl->pending--;
            sl->consumed_gen++;
            pthread_cond_broadcast(&sl->cv); /* wake a blocking releaser (wait=1) */
            out = WUBU_NTSYNC_OK;
            break;
        }
        if (timeout_ms == 0) {         /* pure poll, nothing available */
            out = WUBU_NTSYNC_TIMEOUT;
            break;
        }
        sl->n_waiters++;
        if (timeout_ms < 0) {
            pthread_cond_wait(&sl->cv, &ke->mtx);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec  += timeout_ms / 1000;
            ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
            if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
            int er = pthread_cond_timedwait(&sl->cv, &ke->mtx, &ts);
            if (er == ETIMEDOUT) { sl->n_waiters--; out = WUBU_NTSYNC_TIMEOUT; break; }
        }
        sl->n_waiters--;
        /* Loop: re-check pending (spurious wake or lost race => keep waiting). */
    }
    pthread_mutex_unlock(&ke->mtx);
    return out;
}

int wubu_ntsync_release(wubu_ntsync_t *ke, uint64_t key, int wait, int64_t timeout_ms) {
    if (!ke) return WUBU_NTSYNC_ABORTED;
    int rc = pthread_mutex_lock(&ke->mtx);
    if (rc != 0) return WUBU_NTSYNC_ABORTED;
    int s = ntkey_slot(ke, key, 1);
    if (s < 0) { pthread_mutex_unlock(&ke->mtx); return WUBU_NTSYNC_ABORTED; }
    ntkey_slot_t *sl = &ke->slot[s];

    sl->pending++;                       /* offer a release token */
    long since = sl->consumed_gen;
    pthread_cond_broadcast(&sl->cv);     /* wake waiters so one can consume it */

    if (wait) {
        int out = WUBU_NTSYNC_OK;
        for (;;) {
            if (sl->consumed_gen > since)  /* a waiter consumed OUR token */
                break;
            if (timeout_ms >= 0) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec  += timeout_ms / 1000;
                ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
                if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
                if (pthread_cond_timedwait(&sl->cv, &ke->mtx, &ts) == ETIMEDOUT) {
                    out = WUBU_NTSYNC_TIMEOUT;
                    /* We offered a token nobody consumed; leave it pending so a
                       future waiter still gets it (fail-closed, no lost release). */
                    break;
                }
            } else {
                pthread_cond_wait(&sl->cv, &ke->mtx);
            }
        }
        pthread_mutex_unlock(&ke->mtx);
        return out;
    }
    pthread_mutex_unlock(&ke->mtx);
    return WUBU_NTSYNC_OK;   /* token left pending; a later waiter consumes it */
}

int wubu_ntsync_live_keys(const wubu_ntsync_t *ke) {
    if (!ke) return 0;
    int n = 0;
    /* No lock: diagnostics only. */
    for (int i = 0; i < NTKEY_SLOTS; i++)
        if (ke->slot[i].used) n++;
    return n;
}