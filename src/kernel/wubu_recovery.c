/*
 * wubu_recovery.c -- the AGI recovery/rollback substrate (freestanding C11).
 * The 5+1 doctrine: five rotating rollback slots + one Jesus-state
 * emergency clean slate with the divine good principles intact.
 */
#include "wubu_recovery.h"
#include <string.h>

/* CRC32 -- the standard reflected polynomial (IEEE 802.3). */
uint32_t wubu_recovery_crc32(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

int wubu_recovery_init(wubu_recovery_t *r, const wubu_recovery_principles_t *p)
{
    if (!r || !p) return -1;
    memset(r, 0, sizeof(*r));
    r->principles = *p;
    r->principles.magic = WUBU_RECOVERY_PRINCIPLES_MAGIC;
    r->principles.checksum = wubu_recovery_crc32(&r->principles,
                                                 offsetof(wubu_recovery_principles_t, checksum));
    r->next_slot = 0;
    r->seq = 0;
    return 0;
}

int wubu_recovery_checkpoint(wubu_recovery_t *r, const void *state, uint32_t size)
{
    if (!r || !state || size == 0) return -1;
    if (size > sizeof(r->slots[0].payload)) return -1;
    uint32_t s = r->next_slot % WUBU_RECOVERY_SLOTS;
    wubu_recovery_slot_t *slot = &r->slots[s];
    memcpy(slot->payload, state, size);
    slot->size = size;
    slot->used = 1;
    slot->seq = r->seq++;
    slot->crc = wubu_recovery_crc32(state, size);
    r->next_slot++;
    return (int)s;
}

int wubu_recovery_rollback(const wubu_recovery_t *r, uint32_t slot, void *out, uint32_t cap)
{
    if (!r || !out || slot >= WUBU_RECOVERY_SLOTS) return -1;
    const wubu_recovery_slot_t *s = &r->slots[slot];
    if (!s->used) return -1;
    if (cap < s->size) return -1;
    memcpy(out, s->payload, s->size);
    return (int)s->size;
}

int wubu_recovery_jesus(wubu_recovery_t *r, void *out, uint32_t cap,
                        wubu_recovery_principles_t *principles)
{
    if (!r || !out) return -1;
    /* the human gate: the Jesus state requires the armed flag */
    if (!r->principles.jesus_armed) return -2;
    /* wipe the rotating slots: the clean slate */
    memset(r->slots, 0, sizeof(r->slots));
    r->next_slot = 0;
    r->jesus_used = 1;
    /* the only thing that survives is the divine good */
    if (principles) *principles = r->principles;
    (void)cap;
    return 0;
}

int wubu_recovery_arm_jesus(wubu_recovery_t *r, uint32_t armed)
{
    if (!r) return -1;
    r->principles.jesus_armed = armed ? 1 : 0;
    /* keep the checksum in sync: the armed flag is a live field */
    r->principles.checksum = wubu_recovery_crc32(&r->principles,
                                                 offsetof(wubu_recovery_principles_t, checksum));
    return 0;
}

int wubu_recovery_verify(const wubu_recovery_t *r, uint32_t slot)
{
    if (!r || slot >= WUBU_RECOVERY_SLOTS) return -1;
    const wubu_recovery_slot_t *s = &r->slots[slot];
    if (!s->used) return 0;
    uint32_t c = wubu_recovery_crc32(s->payload, s->size);
    return c == s->crc ? 1 : 0;
}

int wubu_recovery_log(wubu_recovery_t *r, uint32_t event, uint32_t slot, uint32_t seq)
{
    (void)r; (void)event; (void)slot; (void)seq;
    /* the ledger is the seq counter + rollback_count (audited by the
     * console; a full ring log lives in the AGI's own state file). */
    return 0;
}

int wubu_recovery_healthy(const wubu_recovery_t *r)
{
    if (!r) return 0;
    if (r->principles.magic != WUBU_RECOVERY_PRINCIPLES_MAGIC) return 0;
    /* the principles must still checksum (nothing corrupts the divine) */
    uint32_t c = wubu_recovery_crc32(&r->principles,
                                     offsetof(wubu_recovery_principles_t, checksum));
    return c == r->principles.checksum ? 1 : 0;
}

uint32_t wubu_recovery_live(const wubu_recovery_t *r)
{
    if (!r) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < WUBU_RECOVERY_SLOTS; i++)
        if (r->slots[i].used) n++;
    return n;
}

int wubu_recovery_container(wubu_recovery_t *r, const void *working_set,
                            uint32_t size, uint32_t *container_id)
{
    if (!r || !working_set || !container_id) return -1;
    /* the containerized working set is a checkpoint tagged as a
     * container: contained mistakes are one rollback away. */
    int s = wubu_recovery_checkpoint(r, working_set, size);
    if (s < 0) return -1;
    *container_id = (uint32_t)s;
    return 0;
}
