/* styx_flush.c -- see styx_flush.h. */
#include "styx_flush.h"

#include <string.h>

void styx_flush_init(styx_flushtab_t *t)
{
    if (t) memset(t, 0, sizeof *t);
}

static styx_flush_slot_t *find(styx_flushtab_t *t, uint16_t tag)
{
    for (int i = 0; i < STYX_FLUSH_MAX; i++)
        if (t->slots[i].in_use && t->slots[i].tag == tag) return &t->slots[i];
    return NULL;
}

int styx_flush_begin(styx_flushtab_t *t, uint16_t tag)
{
    if (!t || find(t, tag)) return -1;
    for (int i = 0; i < STYX_FLUSH_MAX; i++) {
        if (!t->slots[i].in_use) {
            t->slots[i].in_use = 1;
            t->slots[i].tag = tag;
            t->slots[i].cancelled = 0;
            return 0;
        }
    }
    return -1;
}

int styx_flush_cancel(styx_flushtab_t *t, uint16_t tag)
{
    styx_flush_slot_t *s = t ? find(t, tag) : NULL;
    if (!s) return -1;
    s->cancelled = 1;
    return 0;
}

int styx_flush_is_cancelled(const styx_flushtab_t *t, uint16_t tag)
{
    for (int i = 0; i < STYX_FLUSH_MAX; i++)
        if (t->slots[i].in_use && t->slots[i].tag == tag) return t->slots[i].cancelled;
    return 0;
}

int styx_flush_end(styx_flushtab_t *t, uint16_t tag)
{
    styx_flush_slot_t *s = t ? find(t, tag) : NULL;
    if (!s) return -1;
    memset(s, 0, sizeof *s);
    return 0;
}

int styx_flush_in_flight(const styx_flushtab_t *t)
{
    int n = 0;
    if (!t) return 0;
    for (int i = 0; i < STYX_FLUSH_MAX; i++) if (t->slots[i].in_use) n++;
    return n;
}
