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
        return addr;
    }

    if (!size) return (uint64_t)-22;

    if (g_vsl.n_mmaps >= VSL_MAX_MMAPS) return (uint64_t)-12;

    /* Simple allocation: bump pointer from mmap_base; a non-zero addr is
     * an ADVISORY hint, never a claim on an already-mapped range. */
    uint64_t alloc_addr = addr ? addr : proc->mmap_base;
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
        alloc_addr = next;
        if ((uint64_t)size > arena_end - alloc_addr) return (uint64_t)-12;
    }

    VSL_MMAP *mm = &g_vsl.mmaps[g_vsl.n_mmaps++];
    mm->addr = alloc_addr;
    mm->size = size;
    mm->prot = prot;
    mm->flags = flags;
    mm->fd = fd;
    mm->offset = offset;

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
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
        uint64_t s = g_vsl.mmaps[i].addr;
        uint64_t e = s + g_vsl.mmaps[i].size;
        if (e <= addr || s >= end) continue;
        if (s < addr && e > end) splits++;
    }
    if ((uint64_t)g_vsl.n_mmaps + splits > VSL_MAX_MMAPS) return -12;

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
    return 0;
}

int64_t vsl_brk(uint64_t new_brk) {
    VSL_PROC *proc = vsl_get_process(g_vsl.current_pid);
    if (!proc) return -1;

    if (new_brk == 0) return (int64_t)proc->brk; /* query */

    if (new_brk < VSL_USER_BASE || (uint64_t)new_brk >= (uint64_t)VSL_USER_BASE + (uint64_t)VSL_USER_SIZE)
        return -1;

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