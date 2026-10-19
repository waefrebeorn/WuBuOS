/* timer_wheel.h -- hierarchical timer wheel (6 levels x 64 slots = 2^36 ticks). */
#ifndef WUBU_TIMER_WHEEL_H
#define WUBU_TIMER_WHEEL_H

#include <stdint.h>

#define TW_LEVELS 6
#define TW_SLOTS  64
#define TW_MAX_ENTRIES 256

typedef struct tw_entry {
    uint64_t id;
    uint64_t expiry;
    struct tw_entry *next;
} tw_entry_t;

typedef struct {
    uint64_t now;
    uint64_t pending;
    uint64_t cascades;
    tw_entry_t *slots[TW_LEVELS][TW_SLOTS];
    tw_entry_t pool[TW_MAX_ENTRIES];
    int pool_used;
} tw_t;

void     tw_init(tw_t *w);
int      tw_add(tw_t *w, uint64_t id, uint64_t expiry);   /* expiry >= now */
/* Advance one tick; appends due ids to out (max ids) and returns the count. */
int      tw_advance(tw_t *w, uint64_t *out, int max);
/* Earliest expiry in the wheel, or 0 when empty. */
uint64_t tw_next(const tw_t *w);

#endif