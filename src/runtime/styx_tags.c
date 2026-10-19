/* styx_tags.c -- see styx_tags.h. NOTAG (0xFFFF) is never allocated. */
#include "styx_tags.h"

#include <string.h>

void styx_tags_init(styx_tagtable_t *t)
{
    if (!t) return;
    memset(t, 0, sizeof *t);
    for (int i = 0; i < STYX_TAG_SLOTS; i++) t->slots[i].tag = (uint16_t)i;
}

int styx_tags_alloc(styx_tagtable_t *t, uint32_t req_id, uint16_t *tag)
{
    if (!t || !tag) return -1;
    for (int i = 0; i < STYX_TAG_SLOTS; i++) {
        if (!t->slots[i].in_use) {
            t->slots[i].in_use = 1;
            t->slots[i].req_id = req_id;
            t->live++;
            *tag = t->slots[i].tag;
            return 0;
        }
    }
    return -1;
}

int styx_tags_lookup(const styx_tagtable_t *t, uint16_t tag, uint32_t *req_id)
{
    if (!t) return -1;
    for (int i = 0; i < STYX_TAG_SLOTS; i++) {
        if (t->slots[i].in_use && t->slots[i].tag == tag) {
            if (req_id) *req_id = t->slots[i].req_id;
            return 0;
        }
    }
    return -1;
}

int styx_tags_release(styx_tagtable_t *t, uint16_t tag)
{
    if (!t) return -1;
    for (int i = 0; i < STYX_TAG_SLOTS; i++) {
        if (t->slots[i].in_use && t->slots[i].tag == tag) {
            t->slots[i].in_use = 0;
            t->slots[i].req_id = 0;
            t->live--;
            return 0;
        }
    }
    return -1;
}
