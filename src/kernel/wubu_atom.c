/* wubu_atom.c -- see wubu_atom.h. Freestanding C11, no heap.
 *
 * The ATOM marker is a small fixed block whose CRC (reusing the kernel's
 * wubu_recovery_crc32) makes a torn write detectable: a recovered marker whose CRC
 * doesn't match is refused (fail-closed) rather than booting an unknown slot. */
#include "wubu_atom.h"
#include "wubu_recovery.h"   /* wubu_recovery_crc32 -- the one CRC in the kernel */
#include <string.h>

static void scan_crc(const void *base, size_t off0, size_t len, uint32_t *out)
{
    *out = wubu_recovery_crc32((const uint8_t *)base + off0, (uint32_t)len);
}

int wubu_atom_init(wubu_atom_t *a)
{
    if (!a) return -1;
    memset(a, 0, sizeof(*a));
    a->magic   = WUBU_ATOM_MAGIC;
    a->current = 0;
    a->var[0].state = WUBU_ATOM_IDLE;
    a->var[1].state = WUBU_ATOM_IDLE;
    scan_crc(a, offsetof(wubu_atom_t, var), sizeof(a->var), &a->crc);
    return 0;
}

int wubu_atom_begin_update(wubu_atom_t *a)
{
    if (!a || a->magic != WUBU_ATOM_MAGIC) return -1;
    int other = a->current == 0 ? 1 : 0;
    /* Never layer a pending marker over one already pending: that loses the
     * rollback bookkeeping the boot is voting on. */
    if (a->var[other].state == WUBU_ATOM_PENDING) return -1;
    a->var[other].state = WUBU_ATOM_PENDING;
    a->var[other].tries = 0;
    scan_crc(a, offsetof(wubu_atom_t, var), sizeof(a->var), &a->crc);
    return other;
}

int wubu_atom_commit(wubu_atom_t *a)
{
    if (!a || a->magic != WUBU_ATOM_MAGIC) return -1;
    int other = a->current == 0 ? 1 : 0;
    if (a->var[other].state != WUBU_ATOM_PENDING) return -1;
    /* The pending slot becomes the one the next boot votes on. */
    a->current = (uint32_t)other;
    scan_crc(a, offsetof(wubu_atom_t, var), sizeof(a->var), &a->crc);
    return 0;
}

int wubu_atom_boot(wubu_atom_t *a)
{
    if (!a || a->magic != WUBU_ATOM_MAGIC) return -1;
    wubu_atom_slot_t *s = &a->var[a->current];
    /* Only a pending variant is being voted on; an idle one is simply settled. */
    if (s->state == WUBU_ATOM_PENDING) s->tries++;
    scan_crc(a, offsetof(wubu_atom_t, var), sizeof(a->var), &a->crc);
    return 0;
}

int wubu_atom_boot_ok(wubu_atom_t *a)
{
    if (!a || a->magic != WUBU_ATOM_MAGIC) return -1;
    /* The voted slot is good; its sibling is settled again. */
    a->var[a->current].state  = WUBU_ATOM_IDLE;
    a->var[a->current].tries  = 0;
    a->var[a->current ^ 1].state = WUBU_ATOM_IDLE;
    a->var[a->current ^ 1].tries = 0;
    scan_crc(a, offsetof(wubu_atom_t, var), sizeof(a->var), &a->crc);
    return 0;
}

int wubu_atom_rollback_target(const wubu_atom_t *a)
{
    if (!a || a->magic != WUBU_ATOM_MAGIC) return -1;
    const wubu_atom_slot_t *s = &a->var[a->current];
    if (s->state != WUBU_ATOM_PENDING) return -1;   /* settled image: nothing to roll */
    if (s->tries >= WUBU_ATOM_MAX_TRIES)
        return (int)(a->current ^ 1);               /* boot the OTHER slot instead */
    return -1;                                      /* still within its trial budget */
}

int wubu_atom_marker_crc(const wubu_atom_t *a, uint32_t *out)
{
    if (!a || !out) return -1;
    *out = a->crc;
    return 0;
}

int wubu_atom_marker_valid(const wubu_atom_t *a)
{
    if (!a) return 0;
    if (a->magic != WUBU_ATOM_MAGIC) return 0;
    if (a->current > 1) return 0;
    uint32_t want;
    scan_crc(a, offsetof(wubu_atom_t, var), sizeof(a->var), &want);
    return want == a->crc;
}