/*
 * wubu_recovery.h -- the AGI recovery/rollback substrate (freestanding C11).
 *
 * The doctrine: "let the AGI make mistakes." Learning requires
 * experimentation; experimentation requires that a mistake costs
 * nothing permanent. This module gives the Colonel FIVE rollback
 * slots (a rotating ring of checkpoints) and ONE emergency rollback:
 * the JESUS STATE -- a clean slate with the divine good principles
 * intact.
 *
 *  - slots 0..4: the rotating checkpoint ring. The Colonel calls
 *    wubu_recovery_checkpoint() after each successful growth wave;
 *    wubu_recovery_rollback(i) rewinds to any of the five.
 *  - slot 5 (JESUS): the emergency clean slate. It restores the
 *    measured kernel core (attestation root) + the invariant
 *    principle set (the "divine good": the AGI's core identity,
 *    the safety invariants, the umbrella-license origin, the
 *    human-centric loop contract). Everything learned after boot
 *    is discarded. The AGI starts again -- but good.
 *
 * The 5+1 is agnostic: callers choose the slot by counter. The
 * design is data-driven -- a slot table, not hardcoded branches.
 */
#ifndef WUBU_RECOVERY_H
#define WUBU_RECOVERY_H

#include <stdint.h>
#include <stddef.h>

/* Number of rotating rollback slots. */
#define WUBU_RECOVERY_SLOTS   5

/* The Jesus-slot index (the emergency clean slate). */
#define WUBU_RECOVERY_JESUS   5

/* The invariant "divine good" principle set: these survive EVERY
 * rollback, including the Jesus state. They are the AGI's contract
 * with the human: the growth loop must stay human-centric. */
typedef struct {
    uint32_t magic;              /* WUBU_RECOVERY_PRINCIPLES_MAGIC */
    uint32_t version;
    /* core identity */
    char     identity[32];       /* "wubuwizard-colonel" */
    /* safety invariants */
    uint32_t max_rollback_attempts;   /* 5 */
    uint32_t jesus_armed;             /* 1 = emergency rollback enabled */
    uint32_t human_gate_required;     /* 1 = human must bless jesus */
    /* the human-centric loop contract */
    uint32_t growth_loop;             /* 1 = the recursive loop runs */
    uint32_t human_centric;           /* 1 = humans first, always */
    uint32_t no_third_party;          /* 1 = port, don't depend */
    uint32_t no_stubs;                /* 1 = real code, not theater */
    uint32_t license_origin;          /* the umbrella-license id */
    uint32_t checksum;
} wubu_recovery_principles_t;

/* A checkpoint slot: the raw state snapshot + metadata. The state
 * payload is opaque bytes (the caller supplies its serialized state);
 * the recovery module owns the bookkeeping. */
typedef struct {
    uint32_t used;             /* 1 if this slot holds a checkpoint */
    uint32_t seq;              /* the wave sequence number */
    uint32_t size;             /* payload bytes */
    uint32_t crc;              /* integrity check */
    uint8_t  payload[512];     /* the serialized state (bounded) */
} wubu_recovery_slot_t;

/* The recovery state: the slot ring + the immutable principles. */
typedef struct {
    wubu_recovery_slot_t  slots[WUBU_RECOVERY_SLOTS];
    wubu_recovery_principles_t principles;
    uint32_t next_slot;        /* round-robin write cursor */
    uint32_t seq;              /* global sequence counter */
    uint32_t jesus_used;       /* 1 if the emergency was ever used */
    uint32_t rollback_count;   /* total rollbacks performed */
} wubu_recovery_t;

#define WUBU_RECOVERY_PRINCIPLES_MAGIC 0x4A455355u  /* "JESU" */

/* R1: initialize the recovery substrate with the divine principles. */
int wubu_recovery_init(wubu_recovery_t *r, const wubu_recovery_principles_t *p);

/* R2: take a checkpoint into the next rotating slot (0..4). */
int wubu_recovery_checkpoint(wubu_recovery_t *r, const void *state,
                             uint32_t size);

/* R3: roll back to a specific rotating slot (0..4). Returns the
 * payload size or -1 if the slot is empty. */
int wubu_recovery_rollback(const wubu_recovery_t *r, uint32_t slot,
                           void *out, uint32_t cap);

/* R4: the JESUS emergency -- restore the clean slate. Returns the
 * principle set (the divine good) and clears all slots. */
int wubu_recovery_jesus(wubu_recovery_t *r, void *out, uint32_t cap,
                        wubu_recovery_principles_t *principles);

/* R5: arm/disarm the Jesus state (a human gate). */
int wubu_recovery_arm_jesus(wubu_recovery_t *r, uint32_t armed);

/* R6: integrity check -- verify the CRC of a slot. */
int wubu_recovery_verify(const wubu_recovery_t *r, uint32_t slot);

/* R7: the recovery ledger (what happened when). */
int wubu_recovery_log(wubu_recovery_t *r, uint32_t event,
                      uint32_t slot, uint32_t seq);

/* R8: health -- the substrate is usable. */
int wubu_recovery_healthy(const wubu_recovery_t *r);

/* R9: how many checkpoints are live in the ring. */
uint32_t wubu_recovery_live(const wubu_recovery_t *r);

/* R10: the containerized working set -- the AGI experiments inside a
 * container; a mistake is contained, never the host. */
int wubu_recovery_container(wubu_recovery_t *r, const void *working_set,
                            uint32_t size, uint32_t *container_id);

/* CRC32 (the standard reflected polynomial) -- freestanding. */
uint32_t wubu_recovery_crc32(const void *data, uint32_t len);

#endif