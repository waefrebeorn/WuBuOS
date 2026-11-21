/*
 * vsl_memory.h  --  VSL Memory Management API
 * Opaque struct pattern - only public API exposed
 */

#ifndef WUBUOS_VSL_MEMORY_H
#define WUBUOS_VSL_MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* Forward declare opaque type */
struct VSL_MMAP;
typedef struct VSL_MMAP VSL_MMAP;

/* Map memory in VSL address space.
 * Returns virtual address, or 0 on failure. */
uint64_t vsl_mmap(uint64_t addr, size_t size, int prot, int flags,
                  int fd, uint64_t offset);

/* Unmap memory in VSL address space.
 * Returns 0 on success, -1 on failure. */
int vsl_munmap(uint64_t addr, size_t size);

/* Set VSL heap break (brk).
 * Returns new brk, or -1 on failure. */
int64_t vsl_brk(uint64_t new_brk);

/* Get current brk */
uint64_t vsl_get_brk(void);

/* Bounded coalescing of the mmap bump arena (OPT-IN, default OFF).
 * When enabled, a munmap inside [VSL_MMAP_FLOOR, mmap_base) records a
 * bounded, coalesced free range that a later bump mmap may reuse.  Every
 * fail-closed admission rule, page alignment and the MAP_FIXED/brk/ELF
 * isolation are unchanged; with the switch OFF the allocator is
 * byte-identical to the pre-coalescing behaviour. */
void vsl_dyn_set_free_coalesce(int on);
int  vsl_dyn_free_coalesce(void);
uint32_t vsl_free_range_count(void);
uint64_t vsl_free_range_bytes(void);

/* Reserved (non-bump) guest ranges: ELF-image PT_LOAD union and brk heap.
 *
 * These ranges are registered in the SAME allocator registry as guest mmaps
 * but as fixed, non-recyclable entries.  Two consequences are intentional:
 *   - the bump allocator's collision walk sees them, so a guest mmap (with
 *     or without an advisory hint) can never alias the loaded image or the
 *     heap;
 *   - their exact page-rounded byte count is queryable, so the host budget
 *     table can account for real host pages materialized for them and prove
 *     a full release with no leak.
 *
 * Registration is fail-closed: a null/unaligned/out-of-arena range, a full
 * registry or an alias of an existing live mapping registers NOTHING and
 * returns -errno.  Reserved entries are never inserted into the coalescing
 * free table and a guest vsl_munmap cannot trim or remove them. */
#define VSL_MMAP_RESERVED_IMAGE 0x40000000u
#define VSL_MMAP_RESERVED_BRK   0x80000000u

int      vsl_register_reserved(uint64_t addr, size_t size, int kind);
int      vsl_unregister_reserved(uint64_t addr, size_t size, int kind);
int      vsl_clear_reserved(int kind);          /* all entries of one kind */
uint32_t vsl_reserved_count(void);
uint64_t vsl_reserved_bytes(void);              /* exact sum, all kinds */
uint64_t vsl_reserved_bytes_kind(int kind);     /* exact sum, one kind */
/* ON by default.  OFF reproduces pre-fix behaviour: vsl_brk does not
 * register heap growth.  Used by the exec leg's negative control. */
void     vsl_dyn_set_reserved_registry(int on);
int      vsl_dyn_reserved_registry(void);

/* mmap region accessors */
uint64_t vsl_mmap_get_addr(const VSL_MMAP *mm);
size_t vsl_mmap_get_size(const VSL_MMAP *mm);
int vsl_mmap_get_prot(const VSL_MMAP *mm);
int vsl_mmap_get_flags(const VSL_MMAP *mm);
int vsl_mmap_get_fd(const VSL_MMAP *mm);
uint64_t vsl_mmap_get_offset(const VSL_MMAP *mm);

#endif /* WUBUOS_VSL_MEMORY_H */