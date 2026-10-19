/* styx_flush.h -- cancellation for in-flight 9P requests (Tflush). */
#ifndef WUBU_STYX_FLUSH_H
#define WUBU_STYX_FLUSH_H

#include <stdint.h>

#define STYX_FLUSH_MAX 32

typedef struct {
    uint16_t tag;
    int      in_use;
    int      cancelled;
} styx_flush_slot_t;

typedef struct {
    styx_flush_slot_t slots[STYX_FLUSH_MAX];
} styx_flushtab_t;

void styx_flush_init(styx_flushtab_t *t);
/* Mark a request in flight. Returns 0, or -1 if the tag is already in flight. */
int  styx_flush_begin(styx_flushtab_t *t, uint16_t tag);
/* Tflush(oldt): cancel that request. Returns 0 if it was in flight. */
int  styx_flush_cancel(styx_flushtab_t *t, uint16_t tag);
int  styx_flush_is_cancelled(const styx_flushtab_t *t, uint16_t tag);
/* The request finished: free the slot so the tag can be reused. */
int  styx_flush_end(styx_flushtab_t *t, uint16_t tag);
int  styx_flush_in_flight(const styx_flushtab_t *t);

#endif
