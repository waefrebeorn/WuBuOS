/* styx_locks.h -- 9P byte-range locks (Tlock/Tunlock). */
#ifndef WUBU_STYX_LOCKS_H
#define WUBU_STYX_LOCKS_H

#include <stdint.h>

#define STYX_LOCK_MAX 32
#define STYX_LOCK_RDLCK 0
#define STYX_LOCK_WRLCK 1
#define STYX_LOCK_UNLCK 2

typedef struct {
    uint64_t qid;      /* file identity */
    int32_t  owner;    /* client identity */
    uint64_t start;
    uint64_t length;   /* 0 == to end of file */
    int      type;
    int      in_use;
} styx_lock_t;

typedef struct {
    styx_lock_t locks[STYX_LOCK_MAX];
} styx_locktab_t;

void styx_locks_init(styx_locktab_t *t);
/* 0 on success, -1 on conflict or full table. */
int  styx_lock(styx_locktab_t *t, uint64_t qid, int32_t owner,
               uint64_t start, uint64_t length, int type);
int  styx_unlock(styx_locktab_t *t, uint64_t qid, int32_t owner,
                 uint64_t start, uint64_t length);
int  styx_lock_count(const styx_locktab_t *t);

#endif
