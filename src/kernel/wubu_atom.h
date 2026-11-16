/* wubu_atom.h -- OS image A/B update + auto-rollback (the SteamOS ATOM take).
 *
 * SteamOS 3.10 ships as a RAUC/ATOM bundle: two rootfs slots, an update applied
 * to the INACTIVE slot, a marker flipped at next boot, and the boot itself voting:
 * boot the pending slot -> if it comes up, mark it good; if it fails (watchdog /
 * N boot attempts with no success), REVERT the marker so the old slot boots again.
 * Rollback is thus boot-status-driven, not a button. This module is that spine, in
 * C11, freestanding (no heap, no libc beyond the <string.h> CRCs), mirroring RAUC's
 * slot-status model (idle/pending/booted + a count of boot attempts).
 *
 * The persisted artifact is the whole wubu_atom_t -- a single opaque marker the
 * bootloader writes; CRC covers it so a torn write is detected and fails closed.
 */
#ifndef WUBU_ATOM_H
#define WUBU_ATOM_H

#include <stdint.h>

#define WUBU_ATOM_MAGIC    0x41544F4Du   /* "ATOM" */
#define WUBU_ATOM_MAX_TRIES 3

/* Slot state (RAUC-like). */
enum {
    WUBU_ATOM_IDLE    = 0,   /* the slot's image is settled */
    WUBU_ATOM_PENDING = 1,   /* an update was applied, awaiting a boot vote */
    WUBU_ATOM_BAD     = 2    /* booted and failed -- do not return to it */
};

typedef struct {
    uint32_t state;       /* IDLE / PENDING / BAD */
    uint32_t tries;       /* boot attempts since it became pending */
    uint32_t crc;         /* integrity of this slot record */
} wubu_atom_slot_t;

typedef struct {
    uint32_t magic;       /* WUBU_ATOM_MAGIC */
    uint32_t current;     /* which slot index (0 or 1) is the one to boot */
    wubu_atom_slot_t var[2];   /* rootfs A (0) and B (1) */
    uint32_t crc;         /* integrity of the whole marker */
} wubu_atom_t;

/* Initialise the marker: both slots idle, slot 0 is current. Returns 0. */
int wubu_atom_init(wubu_atom_t *a);

/* Begin an update: the INACTIVE slot (the one that is not `current`) is marked
 * PENDING. Fail-closed on bad args. Returns the targeted slot index or -1. */
int wubu_atom_begin_update(wubu_atom_t *a);

/* Commit the completed update: this slot is the one the NEXT boot must try.
 * Marks `current` = the pending slot. Returns 0. */
int wubu_atom_commit(wubu_atom_t *a);

/* Called from the bootloader on the slot actually booted. Records it as the one
 * being voted on and counts the attempt. Returns 0. */
int wubu_atom_boot(wubu_atom_t *a);

/* The boot succeeded: the slot that is `current` and was pending becomes IDLE
 * (good), any slot that is not it is IDLE too. Returns 0. */
int wubu_atom_boot_ok(wubu_atom_t *a);

/* After a failed boot (watchdog), report whether the other slot must be booted
 * instead. True when the current slot is PENDING and its boot attempts have
 * reached WUBU_ATOM_MAX_TRIES. Returns the slot to boot (0/1) or -1 if none. */
int wubu_atom_rollback_target(const wubu_atom_t *a);

/* Persist/recover: serialize the marker (with CRC) and re-check a recovered one.
 * CRCs are computed by the module so a caller just memcpy's the bytes. */
int wubu_atom_marker_crc(const wubu_atom_t *a, uint32_t *out);
int wubu_atom_marker_valid(const wubu_atom_t *a);

#endif /* WUBU_ATOM_H */