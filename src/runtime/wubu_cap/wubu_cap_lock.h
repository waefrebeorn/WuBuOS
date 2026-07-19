/*
 * wubu_cap_lock.h -- minimal host lock shim for the capability core.
 *
 * On bare metal this maps to a real spinlock; on the hosted build it maps to
 * a pthread mutex so the capability core links and tests cleanly without any
 * kernel dependency. The capability core only ever calls the four macros
 * below, so swapping the backend is a one-file change.
 */
#ifndef WUBU_CAP_LOCK_H
#define WUBU_CAP_LOCK_H

#if defined(WUBU_CAP_KERNEL)
/* Bare-metal: wire to the real kernel spinlock here. */
#include "kernel/sync/spinlock.h"
#define wubu_cap_lock_t            spinlock_t
#define WUBU_CAP_LOCK_INIT()       SPINLOCK_INITIALIZER("wubu_cap")
#define wubu_cap_lock_acquire_impl SPINLOCK_ACQUIRE
#define wubu_cap_lock_release_impl SPINLOCK_RELEASE
#else
/* Hosted: pthread mutex shim. */
#include <pthread.h>
typedef pthread_mutex_t wubu_cap_lock_t;
#define WUBU_CAP_LOCK_INIT()       PTHREAD_MUTEX_INITIALIZER
static inline void wubu_cap_lock_acquire_impl(wubu_cap_lock_t *l) { pthread_mutex_lock(l); }
static inline void wubu_cap_lock_release_impl(wubu_cap_lock_t *l) { pthread_mutex_unlock(l); }
#endif

#endif /* WUBU_CAP_LOCK_H */
