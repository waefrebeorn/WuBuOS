/* styx_tags.h -- 9P tag table: fixed pool, reuse only after release. */
#ifndef WUBU_STYX_TAGS_H
#define WUBU_STYX_TAGS_H

#include <stdint.h>

#define STYX_TAG_SLOTS 256

typedef struct {
    uint16_t tag;
    uint32_t req_id;      /* caller's request identity */
    int      in_use;
} styx_tag_slot_t;

typedef struct {
    styx_tag_slot_t slots[STYX_TAG_SLOTS];
    int             live;
} styx_tagtable_t;

void     styx_tags_init(styx_tagtable_t *t);
/* Allocate a free tag for req_id; returns 0 on success, -1 when full. */
int      styx_tags_alloc(styx_tagtable_t *t, uint32_t req_id, uint16_t *tag);
/* Find the request a reply tag belongs to; returns req_id or -1. */
int      styx_tags_lookup(const styx_tagtable_t *t, uint16_t tag, uint32_t *req_id);
/* Release a tag after its reply has been sent. */
int      styx_tags_release(styx_tagtable_t *t, uint16_t tag);

#endif
