/*
 * vsl_memory.c  --  VSL Memory Management Implementation
 */

#include "vsl/vsl_internal.h"
#include "vsl/vsl_memory.h"
#include "vsl/vsl_process.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Global state */
extern VSL_STATE g_vsl;

/* -- Memory Management -------------------------------------------- */

/* Linux x86-64 MAP_FIXED. Dynamic loaders map the ELF interpreter and
 * libraries at their link addresses; ignoring the hint silently put the
 * mapping somewhere else. */
#define VSL_MAP_FIXED 0x10u

/* Bounded coalescing of freed bump-arena ranges.  OPT-IN, default OFF:
 * with the switch off vsl_mmap/vsl_munmap are byte-identical to the
 * pre-coalescing allocator, which is what every pre-existing gate runs. */
static int g_free_coalesce;
static int g_reserved_registry = 1;   /* ON by default; OFF = pre-fix control */

/* -- bounded coalesced free-range table ---------------------------- *
 * Merging touching/overlapping holes turns two adjacent single-page
 * unmaps into one reusable two-page range; the fixed owner array cannot
 * grow past VSL_MAX_FREE_RANGES (a full table simply stops tracking new
 * holes — the registry unmap still succeeds, those bytes are just not
 * recycled).  free_range_bytes is the exact sum of all tracked holes and
 * is charged on insert/coalesce and removed on reuse/invalidation. */
static void vsl_free_insert(uint64_t addr, size_t size) {
    if (!g_free_coalesce || !size) return;
    if (addr < VSL_MMAP_FLOOR) return;
    uint64_t end = addr + (uint64_t)size;   /* callers prove no wrap */
    if (end <= addr || end <= VSL_MMAP_FLOOR) return;

    /* A recyclable hole must never alias a live reserved ELF-image/brk
     * range.  Carve such ranges out before coalescing; the left/right
     * remnants are inserted separately so free_range_bytes stays exact. */
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
        VSL_MMAP *mm = &g_vsl.mmaps[i];
        if (!(mm->flags & (VSL_MMAP_RESERVED_IMAGE | VSL_MMAP_RESERVED_BRK)))
            continue;
        uint64_t s = mm->addr, e = s + mm->size;
        if (e <= addr || s >= end) continue;
        if (s > addr) vsl_free_insert(addr, (size_t)(s - addr));
        if (e < end)  vsl_free_insert(e, (size_t)(end - e));
        return;
    }

    for (;;) {
        int merged = 0;
        for (uint32_t i = 0; i < g_vsl.n_free_ranges; i++) {
            uint64_t s = g_vsl.free_ranges[i].addr;
            uint64_t e = s + (uint64_t)g_vsl.free_ranges[i].size;
            if (e < addr || s > end) continue;   /* disjoint */
            if (s < addr) addr = s;
            if (e > end) end = e;
            g_vsl.free_range_bytes -= (uint64_t)(e - s);
            for (uint32_t j = i; j + 1 < g_vsl.n_free_ranges; j++)
                g_vsl.free_ranges[j] = g_vsl.free_ranges[j + 1];
            g_vsl.n_free_ranges--;
            merged = 1;
            break;
        }
        if (!merged) break;
    }
    if (g_vsl.n_free_ranges >= VSL_MAX_FREE_RANGES) return; /* bounded */
    uint32_t k = g_vsl.n_free_ranges++;
    g_vsl.free_ranges[k].addr = addr;
    g_vsl.free_ranges[k].size = (size_t)(end - addr);
    g_vsl.free_range_bytes += (uint64_t)(end - addr);
}

/* First-fit hole at or above `lower` with room for `size` (page-rounded).
 * Holes are page-aligned and [floor, mmap_base)-bounded by construction.
 * Only a hole whose start satisfies `lower` is eligible, so an advisory
 * hint can never be handed a range below it; addr==0 passes the arena
 * floor.  Returns 1 with *out set and the hole carved (exact bytes). */
static int vsl_free_take(uint64_t lower, size_t size, uint64_t *out) {
    if (!g_free_coalesce || !g_vsl.n_free_ranges || !size) return 0;
    uint64_t need = ((uint64_t)size + 0xfff) & ~0xfffull;
    if (need < size) return 0;
    for (uint32_t i = 0; i < g_vsl.n_free_ranges; i++) {
        uint64_t s = g_vsl.free_ranges[i].addr;
        uint64_t e = s + (uint64_t)g_vsl.free_ranges[i].size;
        if (s < lower || (s & 0xfff)) continue;
        if ((uint64_t)g_vsl.free_ranges[i].size < need) continue;
        *out = s;
        g_vsl.free_range_bytes -= need;   /* exact consumed bytes */
        if ((uint64_t)g_vsl.free_ranges[i].size == need) {
            for (uint32_t j = i; j + 1 < g_vsl.n_free_ranges; j++)
                g_vsl.free_ranges[j] = g_vsl.free_ranges[j + 1];
            g_vsl.n_free_ranges--;
        } else {
            g_vsl.free_ranges[i].addr = s + need;
            g_vsl.free_ranges[i].size = (size_t)(e - s - need);
        }
        return 1;
    }
    return 0;
}

/* A MAP_FIXED registration replaces whatever was at [addr,end); remove
 * that intersection from the free table so a recycled hole can never
 * alias a live fixed mapping (fail-closed). */
static void vsl_free_invalidate(uint64_t addr, uint64_t end) {
    if (!g_free_coalesce) return;
    for (uint32_t i = 0; i < g_vsl.n_free_ranges; ) {
        uint64_t s = g_vsl.free_ranges[i].addr;
        uint64_t e = s + (uint64_t)g_vsl.free_ranges[i].size;
        if (e <= addr || s >= end) { i++; continue; }
        g_vsl.free_range_bytes -= (uint64_t)(e - s);
        if (s < addr) {
            g_vsl.free_ranges[i].size = (size_t)(addr - s);
            g_vsl.free_range_bytes += (uint64_t)(addr - s);
            i++;
        } else {
            for (uint32_t j = i; j + 1 < g_vsl.n_free_ranges; j++)
                g_vsl.free_ranges[j] = g_vsl.free_ranges[j + 1];
            g_vsl.n_free_ranges--;
        }
        /* A right remnant (e > end) is dropped; accounting stays exact. */
    }
}

/* -- reserved (non-bump) region registry ---------------------------- *
 * ELF-image PT_LOAD unions and the brk heap live below the recyclable
 * bump arena.  They are registered as fixed, non-recyclable entries in
 * g_vsl.mmaps[] so every allocator decision — collision walk, MAP_FIXED
 * admission and exact byte accounting — sees them.  The byte totals are
 * derived by summing live entries, never by a side counter, so they are
 * exact by construction after every insert/split/coalesce/removal. */
static int vsl_is_reserved(const VSL_MMAP *mm) {
    return (mm->flags & (VSL_MMAP_RESERVED_IMAGE | VSL_MMAP_RESERVED_BRK)) != 0;
}

/* Return 1 when a live reserved entry of `kind` touches [addr,end). */
static int vsl_reserved_touching(uint64_t addr, uint64_t end, int kind) {
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
        VSL_MMAP *mm = &g_vsl.mmaps[i];
        if (!vsl_is_reserved(mm) || (mm->flags & kind) == 0) continue;
        uint64_t s = mm->addr, e = s + mm->size;
        if (e > addr && s < end) return 1;
    }
    return 0;
}

int vsl_register_reserved(uint64_t addr, size_t size, int kind) {
    if (!g_vsl.active) return -12;
    kind &= (VSL_MMAP_RESERVED_IMAGE | VSL_MMAP_RESERVED_BRK);
    if (!kind || size == 0) return -22;
    if ((addr & 0xfff) || (size & 0xfff)) return -22;
    uint64_t end = addr + (uint64_t)size;
    uint64_t arena_end = (uint64_t)VSL_USER_BASE + (uint64_t)VSL_USER_SIZE;
    if (end <= addr || addr < VSL_USER_BASE || end > arena_end) return -12;

    /* Fail closed on any overlap with an existing live registration,
     * reserved or guest — a reserved range must never alias live memory. */
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
        uint64_t s = g_vsl.mmaps[i].addr;
        uint64_t e = s + g_vsl.mmaps[i].size;
        if (e > addr && s < end) return -12;
    }

    /* Adjacent same-kind ranges coalesce in place: byte sum stays exact and
     * a burst of small brk increments cannot consume registry slots. */
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
        VSL_MMAP *mm = &g_vsl.mmaps[i];
        if (!vsl_is_reserved(mm) || (mm->flags & kind) == 0) continue;
        if (mm->addr + mm->size == addr) { mm->size += size; return 0; }
        if (addr + (uint64_t)size == mm->addr) {
            mm->addr = addr; mm->size += size; return 0;
        }
    }
    if (g_vsl.n_mmaps >= VSL_MAX_MMAPS) return -12;
    VSL_MMAP *mm = &g_vsl.mmaps[g_vsl.n_mmaps++];
    mm->addr = addr; mm->size = size;
    mm->prot = (kind & VSL_MMAP_RESERVED_IMAGE) ? 5 : 3; /* RX / RW */
    mm->flags = VSL_MAP_FIXED | kind;
    mm->fd = -1; mm->offset = 0;
    return 0;
}

int vsl_unregister_reserved(uint64_t addr, size_t size, int kind) {
    kind &= (VSL_MMAP_RESERVED_IMAGE | VSL_MMAP_RESERVED_BRK);
    if (!kind || size == 0 || (addr & 0xfff) || (size & 0xfff)) return -22;
    uint64_t end = addr + (uint64_t)size;
    if (end <= addr) return -22;
    int removed = 0;
    for (uint32_t i = 0; i < g_vsl.n_mmaps; ) {
        VSL_MMAP *mm = &g_vsl.mmaps[i];
        if (!vsl_is_reserved(mm) || (mm->flags & kind) == 0) { i++; continue; }
        uint64_t s = mm->addr, e = s + mm->size;
        if (e <= addr || s >= end) { i++; continue; }
        if (s >= addr && e <= end) {
            for (uint32_t j = i; j + 1 < g_vsl.n_mmaps; j++)
                g_vsl.mmaps[j] = g_vsl.mmaps[j + 1];
            g_vsl.n_mmaps--;
            removed = 1;
            continue;
        }
        if (s < addr && e > end) {          /* split: keep the right remnant */
            VSL_MMAP *r = &g_vsl.mmaps[g_vsl.n_mmaps++];
            *r = *mm;
            r->addr = end; r->size = (size_t)(e - end);
            mm->size = (size_t)(addr - s);
        } else if (s < addr) {              /* trim the right edge */
            mm->size = (size_t)(addr - s);
        } else {                            /* trim the left edge */
            mm->addr = end; mm->size = (size_t)(e - end);
        }
        removed = 1;
        i++;
    }
    return removed ? 0 : -22;
}

int vsl_clear_reserved(int kind) {
    kind &= (VSL_MMAP_RESERVED_IMAGE | VSL_MMAP_RESERVED_BRK);
    if (!kind) return -22;
    for (uint32_t i = 0; i < g_vsl.n_mmaps; ) {
        VSL_MMAP *mm = &g_vsl.mmaps[i];
        if (vsl_is_reserved(mm) && (mm->flags & kind)) {
            for (uint32_t j = i; j + 1 < g_vsl.n_mmaps; j++)
                g_vsl.mmaps[j] = g_vsl.mmaps[j + 1];
            g_vsl.n_mmaps--;
            continue;
        }
        i++;
    }
    return 0;
}

uint32_t vsl_reserved_count(void) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++)
        if (vsl_is_reserved(&g_vsl.mmaps[i])) n++;
    return n;
}

uint64_t vsl_reserved_bytes(void) {
    uint64_t sum = 0;
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++)
        if (vsl_is_reserved(&g_vsl.mmaps[i]))
            sum += (uint64_t)g_vsl.mmaps[i].size;
    return sum;
}

uint64_t vsl_reserved_bytes_kind(int kind) {
    kind &= (VSL_MMAP_RESERVED_IMAGE | VSL_MMAP_RESERVED_BRK);
    uint64_t sum = 0;
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
        VSL_MMAP *mm = &g_vsl.mmaps[i];
        if (vsl_is_reserved(mm) && (mm->flags & kind))
            sum += (uint64_t)mm->size;
    }
    return sum;
}

uint64_t vsl_mmap(uint64_t addr, size_t size, int prot, int flags,
                  int fd, uint64_t offset) {
    /* Failure returns -ENOMEM, NOT 0: the syscall contract is -errno;
     * 0 looks like a successful mapping at NULL to glibc (it checks
     * MAP_FAILED == -1). Found by the libc-return-semantics sibling
     * scan after the getcwd pointer-vs-length bug. */
    if (!g_vsl.active) return (uint64_t)-12;

    VSL_PROC *proc = vsl_get_process(g_vsl.current_pid);
    if (!proc) return (uint64_t)-12;
    uint64_t arena_end = (uint64_t)VSL_USER_BASE + (uint64_t)VSL_USER_SIZE;

    if (flags & VSL_MAP_FIXED) {
        /* MAP_FIXED demands a page-aligned, non-zero address; Linux
         * replaces any mapping that overlaps the requested range. */
        if (!addr || (addr & 0xfff)) return (uint64_t)-22;
        if (!size || addr < VSL_USER_BASE || addr >= arena_end ||
            (uint64_t)size > arena_end - addr) return (uint64_t)-12;
        uint64_t end = addr + (uint64_t)size;

        /* Fail closed instead of aliasing a reserved ELF-image/brk range:
         * a fixed mapping may never overwrite a live reserved registration. */
        if (vsl_reserved_touching(addr, end,
                VSL_MMAP_RESERVED_IMAGE | VSL_MMAP_RESERVED_BRK))
            return (uint64_t)-12;

        /* Count the resulting table size BEFORE touching it: a failed
         * MAP_FIXED must leave the arena exactly as it was. */
        uint32_t remove = 0, add = 1;
        for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
            uint64_t s = g_vsl.mmaps[i].addr;
            uint64_t e = s + g_vsl.mmaps[i].size;
            if (e <= addr || s >= end) continue;
            if (s >= addr && e <= end) remove++;
            else { if (s < addr) add++; if (e > end) add++; }
        }
        if (g_vsl.n_mmaps - remove + add > VSL_MAX_MMAPS) return (uint64_t)-12;

        for (uint32_t i = 0; i < g_vsl.n_mmaps; ) {
            VSL_MMAP *mm = &g_vsl.mmaps[i];
            uint64_t s = mm->addr, e = s + mm->size;
            if (e <= addr || s >= end) { i++; continue; }
            if (s >= addr && e <= end) {
                for (uint32_t j = i; j + 1 < g_vsl.n_mmaps; j++)
                    g_vsl.mmaps[j] = g_vsl.mmaps[j + 1];
                g_vsl.n_mmaps--;
                continue;
            }
            int split = (s < addr && e > end);
            int other = (int)prot, oflags = flags, ofd = fd;
            uint64_t ooff = offset;
            if (split) {
                other = mm->prot; oflags = mm->flags;
                ofd = mm->fd; ooff = mm->offset;
            }
            if (s < addr) { mm->size = (size_t)(addr - s); }
            else           { mm->addr = end; mm->size = (size_t)(e - end); }
            if (split) {
                VSL_MMAP *r = &g_vsl.mmaps[g_vsl.n_mmaps++];
                r->addr = end; r->size = (size_t)(e - end);
                r->prot = other; r->flags = oflags;
                r->fd = ofd; r->offset = ooff;
            }
            i++;
        }
        VSL_MMAP *mm = &g_vsl.mmaps[g_vsl.n_mmaps++];
        mm->addr = addr; mm->size = size; mm->prot = prot;
        mm->flags = flags; mm->fd = fd; mm->offset = offset;
        if (end > proc->mmap_base) proc->mmap_base = end;
        vsl_free_invalidate(addr, end);   /* fixed mapping wins over holes */
        return addr;
    }

    if (!size) return (uint64_t)-22;

    if (g_vsl.n_mmaps >= VSL_MAX_MMAPS) return (uint64_t)-12;

    /* OPT-IN: reuse a coalesced free range before advancing the bump
     * pointer.  With the switch off this block is a no-op and the code
     * below is byte-identical to the pre-coalescing allocator. */
    uint64_t alloc_addr = 0;
    int from_free = 0;
    if (g_free_coalesce) {
        uint64_t lower;
        if (!addr) {
            lower = VSL_MMAP_FLOOR;
        } else {
            if (addr > arena_end - 0xfff) return (uint64_t)-12;
            lower = (addr + 0xfff) & ~0xfffull;
        }
        if (vsl_free_take(lower, size, &alloc_addr)) from_free = 1;
    }

    if (!from_free) {
    /* Simple allocation: bump pointer from mmap_base; a non-zero addr is
     * an ADVISORY hint, never a claim on an already-mapped range. */
    alloc_addr = addr ? addr : proc->mmap_base;
    if (alloc_addr < VSL_USER_BASE) alloc_addr = VSL_USER_BASE;
    /* Round a huge hint only after bounding it: addr+0xfff wrapped to 0
     * and a huge size wrapped past arena_end, registering a bogus mapping. */
    if (alloc_addr > arena_end - 0xfff) return (uint64_t)-12;
    alloc_addr = (alloc_addr + 0xfff) & ~0xfffull;
    if ((uint64_t)size > arena_end - alloc_addr) return (uint64_t)-12;

    /* Walk past any colliding registration. Leaving the hint unresolved
     * handed the SAME address to two callers; dynamic loaders then wrote
     * one library's segments over another's. */
    for (;;) {
        uint64_t next = 0;
        for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
            uint64_t s = g_vsl.mmaps[i].addr;
            uint64_t e = s + g_vsl.mmaps[i].size;
            if (e <= alloc_addr || s >= alloc_addr + (uint64_t)size) continue;
            if (e > next) next = e;
        }
        if (!next) break;
        /* A colliding mapping can end on a non-page boundary: an earlier
         * mmap's size need not be page-sized, so `next = s + size` may be
         * unaligned.  A real kernel mmap always returns a page-aligned
         * address, and the backed-guest path requires one (an unaligned
         * return is refused as "invalid range" and the guest faults).
         * Advance to the page-aligned start of the next free page instead
         * of handing back the raw end.  Bound BEFORE rounding so a near
         * arena-end end cannot wrap to 0 and register a bogus mapping. */
        if (next > arena_end - 0xfff) return (uint64_t)-12;
        alloc_addr = (next + 0xfff) & ~0xfffull;
        if ((uint64_t)size > arena_end - alloc_addr) return (uint64_t)-12;
    }
    } /* !from_free */

    VSL_MMAP *mm = &g_vsl.mmaps[g_vsl.n_mmaps++];
    mm->addr = alloc_addr;
    mm->size = size;
    mm->prot = prot;
    mm->flags = flags;
    mm->fd = fd;
    mm->offset = offset;

    /* A recycled hole is below the high-water mark: never move it up.  A
     * resolved advisory hint can also be BELOW the high-water mark; a
     * monotonic guard keeps the bump pointer from regressing into ranges
     * that were already handed out (and keeps reserved ranges above floor
     * out of the recyclable window). */
    if (!from_free && alloc_addr + (uint64_t)size > proc->mmap_base)
        proc->mmap_base = alloc_addr + size;
    return alloc_addr;
}

int vsl_munmap(uint64_t addr, size_t size) {
    if (!g_vsl.active) return -1;
    /* Linux: len==0 and unaligned addresses are EINVAL; unmapping a
     * range with no mappings is a successful no-op, and partial ranges
     * trim/split the registry instead of requiring an exact match. */
    if (!size || (addr & 0xfff)) return -22;
    uint64_t end = addr + (((uint64_t)size + 0xfff) & ~0xfffull);
    if (end <= addr) return -22;

    uint32_t splits = 0;
    int touched = 0;
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
        VSL_MMAP *mm = &g_vsl.mmaps[i];
        if (vsl_is_reserved(mm)) continue;   /* image/brk are not guest-unmappable */
        uint64_t s = mm->addr;
        uint64_t e = s + mm->size;
        if (e <= addr || s >= end) continue;
        touched = 1;
        if (s < addr && e > end) splits++;
    }
    if ((uint64_t)g_vsl.n_mmaps + splits > VSL_MAX_MMAPS) return -12;

    for (uint32_t i = 0; i < g_vsl.n_mmaps; ) {
        VSL_MMAP *mm = &g_vsl.mmaps[i];
        uint64_t s = mm->addr, e = s + mm->size;
        if (e <= addr || s >= end) { i++; continue; }
        if (vsl_is_reserved(mm)) { i++; continue; }
        if (s >= addr && e <= end) {
            for (uint32_t j = i; j + 1 < g_vsl.n_mmaps; j++)
                g_vsl.mmaps[j] = g_vsl.mmaps[j + 1];
            g_vsl.n_mmaps--;
            continue;
        }
        int split = (s < addr && e > end);
        int other = mm->prot, oflags = mm->flags, ofd = mm->fd;
        uint64_t ooff = mm->offset;
        if (s < addr) { mm->size = (size_t)(addr - s); }
        else           { mm->addr = end; mm->size = (size_t)(e - end); }
        if (split) {
            VSL_MMAP *r = &g_vsl.mmaps[g_vsl.n_mmaps++];
            r->addr = end; r->size = (size_t)(e - end);
            r->prot = other; r->flags = oflags;
            r->fd = ofd; r->offset = ooff;
        }
        i++;
    }
    /* OPT-IN: record only ranges that really unmapped a registration,
     * clamped to the recyclable bump arena.  The registry unmap above is
     * unchanged and always succeeds; the free table is best-effort and
     * bounded. */
    if (touched && g_free_coalesce) {
        uint64_t lo = addr, hi = end;
        uint64_t arena_end = VSL_USER_BASE + VSL_USER_SIZE;
        if (lo < VSL_MMAP_FLOOR) lo = VSL_MMAP_FLOOR;
        if (hi > arena_end) hi = arena_end;
        if (hi > lo) vsl_free_insert(lo, (size_t)(hi - lo));
    }
    return 0;
}

int64_t vsl_brk(uint64_t new_brk) {
    VSL_PROC *proc = vsl_get_process(g_vsl.current_pid);
    if (!proc) return -1;

    if (new_brk == 0) return (int64_t)proc->brk; /* query */

    if (new_brk < VSL_USER_BASE || (uint64_t)new_brk >= (uint64_t)VSL_USER_BASE + (uint64_t)VSL_USER_SIZE)
        return -1;

    /* Keep the heap in the allocator registry with exact, page-granular
     * byte accounting: growing brk registers the new page-rounded range,
     * shrinking trims it.  A growth that cannot be registered fails closed
     * — the guest break never moves without an exact registry entry. */
    uint64_t old = proc->brk;
    if (!g_reserved_registry) {          /* pre-fix control: no registration */
        proc->brk = new_brk;
        return (int64_t)new_brk;
    }
    if (new_brk > old) {
        uint64_t lo = (old + 0xfff) & ~0xfffull;
        uint64_t hi = (new_brk + 0xfff) & ~0xfffull;
        if (hi > lo &&
            vsl_register_reserved(lo, (size_t)(hi - lo),
                                  VSL_MMAP_RESERVED_BRK) != 0)
            return -1;
    } else if (new_brk < old) {
        uint64_t lo = (new_brk + 0xfff) & ~0xfffull;
        uint64_t hi = (old + 0xfff) & ~0xfffull;
        if (hi > lo)
            (void)vsl_unregister_reserved(lo, (size_t)(hi - lo),
                                          VSL_MMAP_RESERVED_BRK);
    }

    proc->brk = new_brk;
    return (int64_t)new_brk;
}

uint64_t vsl_get_brk(void) {
    VSL_PROC *proc = vsl_get_process(g_vsl.current_pid);
    return proc ? proc->brk : 0;
}

/* -- mmap Accessors ----------------------------------------------- */

uint64_t vsl_mmap_get_addr(const VSL_MMAP *mm) {
    return mm ? mm->addr : 0;
}

size_t vsl_mmap_get_size(const VSL_MMAP *mm) {
    return mm ? mm->size : 0;
}

int vsl_mmap_get_prot(const VSL_MMAP *mm) {
    return mm ? mm->prot : 0;
}

int vsl_mmap_get_flags(const VSL_MMAP *mm) {
    return mm ? mm->flags : 0;
}

int vsl_mmap_get_fd(const VSL_MMAP *mm) {
    return mm ? mm->fd : -1;
}

uint64_t vsl_mmap_get_offset(const VSL_MMAP *mm) {
    return mm ? mm->offset : 0;
}

/* -- bounded free-range controls ----------------------------------- */

void vsl_dyn_set_free_coalesce(int on) { g_free_coalesce = on ? 1 : 0; }
int vsl_dyn_free_coalesce(void) { return g_free_coalesce; }
void vsl_dyn_set_reserved_registry(int on) { g_reserved_registry = on ? 1 : 0; }
int  vsl_dyn_reserved_registry(void) { return g_reserved_registry; }
uint32_t vsl_free_range_count(void) { return g_vsl.n_free_ranges; }
uint64_t vsl_free_range_bytes(void) { return g_vsl.free_range_bytes; }