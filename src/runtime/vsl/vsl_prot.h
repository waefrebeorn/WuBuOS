/*
 * vsl_prot.h -- narrow, read-only protection query for the VSL user arena.
 *
 * This is the single owner-facing lookup consulted by the WEMU guest-memory
 * write hook (src/runtime/wubu_wemu_bridge.c). It reads the prot records
 * that vsl_mprotect() already maintains in g_vsl (mmaps[] + per-process
 * mprot[]); it does NOT keep a second table.
 */
#ifndef WUBUOS_VSL_PROT_H
#define WUBUOS_VSL_PROT_H

#include <stdint.h>

/* Linux mprotect() bit values (the encoding vsl_mprotect stores). */
#define VSL_PROT_NONE  0
#define VSL_PROT_READ  1
#define VSL_PROT_WRITE 2
#define VSL_PROT_EXEC  4

/* Effective protection mask (VSL_PROT_READ|WRITE|EXEC bits) for a guest VA,
 * or -1 when the VA is NOT inside a tracked VSL arena mapping (legacy guest
 * memory => treat permissive). This is the SINGLE owner lookup; it answers
 * read, write and execute questions from the one mmap/mprot record set. */
int vsl_prot_query(uint64_t va);

/* Convenience predicate over the SAME lookup (no second protection table).
 * Returns 1 when access of kind `bit` is allowed at `va`. Legacy/unmanaged
 * VAs (vsl_prot_query == -1) are permissive and always allowed. */
int vsl_prot_allows(uint64_t va, int bit);

#endif /* WUBUOS_VSL_PROT_H */
