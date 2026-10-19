/*
 * timer_wheel.c -- hierarchical timer wheel, 6 levels x 64 slots (2^36 ticks).
 *
 * Level L covers the range [now + 64^L, now + 64^(L+1)). An entry is placed at
 * the coarsest level whose span still contains it, and cascades down as the
 * clock reaches each level boundary. That is what makes a 2^32-tick sleep cost
 * the same to insert as a 2-tick sleep.
 */
#include "timer_wheel.h"

#include <string.h>

void tw_init(tw_t *w)
{
    memset(w, 0, sizeof *w);
}

static int level_for(uint64_t delta)
{
    int level = 0;
    uint64_t span = TW_SLOTS;
    while (level + 1 < TW_LEVELS && delta >= span) {
        level++;
        span *= TW_SLOTS;
    }
    return level;
}

int tw_add(tw_t *w, uint64_t id, uint64_t expiry)
{
    if (!w || expiry < w->now) return -1;
    if (w->pool_used >= TW_MAX_ENTRIES) return -1;

    uint64_t delta = expiry - w->now;
    int level = level_for(delta);
    unsigned slot = (unsigned)((expiry >> (6 * level)) & (TW_SLOTS - 1));

    tw_entry_t *e = &w->pool[w->pool_used++];
    e->id = id;
    e->expiry = expiry;
    e->next = w->slots[level][slot];
    w->slots[level][slot] = e;
    w->pending++;
    return 0;
}

/* Move every entry in a coarser slot down to the level that now owns it. */
static void cascade(tw_t *w, int level, unsigned slot)
{
    tw_entry_t *e = w->slots[level][slot];
    w->slots[level][slot] = NULL;

    while (e) {
        tw_entry_t *next = e->next;
        uint64_t delta = e->expiry > w->now ? e->expiry - w->now : 0;
        int l = level_for(delta);
        unsigned s = (unsigned)((e->expiry >> (6 * l)) & (TW_SLOTS - 1));
        e->next = w->slots[l][s];
        w->slots[l][s] = e;
        w->cascades++;
        e = next;
    }
}

int tw_advance(tw_t *w, uint64_t *out, int max)
{
    if (!w) return 0;

    /* Order matters: step the clock, cascade entries that just became due at
     * this granularity, and only then fire level 0. Firing before cascading
     * leaves freshly-cascaded entries in a slot that has already been swept,
     * which delays them by a whole rotation. */
    w->now++;

    for (int l = 1; l < TW_LEVELS; l++) {
        if ((w->now & ((1ull << (6 * l)) - 1)) == 0) {
            unsigned s = (unsigned)((w->now >> (6 * l)) & (TW_SLOTS - 1));
            cascade(w, l, s);
        }
    }

    unsigned slot0 = (unsigned)(w->now & (TW_SLOTS - 1));
    int fired = 0;
    tw_entry_t *e = w->slots[0][slot0];
    w->slots[0][slot0] = NULL;

    while (e) {
        tw_entry_t *next = e->next;
        if (e->expiry <= w->now) {
            if (fired < max) out[fired++] = e->id;
            w->pending--;
        } else {
            uint64_t delta = e->expiry - w->now;
            int l = level_for(delta);
            unsigned s = (unsigned)((e->expiry >> (6 * l)) & (TW_SLOTS - 1));
            e->next = w->slots[l][s];
            w->slots[l][s] = e;
        }
        e = next;
    }
    return fired;
}

uint64_t tw_next(const tw_t *w)
{
    uint64_t best = 0;
    for (int l = 0; l < TW_LEVELS; l++)
        for (int s = 0; s < TW_SLOTS; s++)
            for (const tw_entry_t *e = w->slots[l][s]; e; e = e->next)
                if (best == 0 || e->expiry < best) best = e->expiry;
    return best;
}
