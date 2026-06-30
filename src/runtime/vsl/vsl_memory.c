/*
 * vsl_memory.c  --  VSL Memory Management Implementation
 */

#define _GNU_SOURCE
#include "vsl/vsl_internal.h"
#include "vsl/vsl_memory.h"
#include "vsl/vsl_process.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Global state */
extern VSL_STATE g_vsl;

/* -- Memory Management -------------------------------------------- */

uint64_t vsl_mmap(uint64_t addr, size_t size, int prot, int flags,
                  int fd, uint64_t offset) {
    if (!g_vsl.active) return 0;
    if (g_vsl.n_mmaps >= VSL_MAX_MMAPS) return 0;

    VSL_PROC *proc = vsl_get_process(g_vsl.current_pid);
    if (!proc) return 0;

    /* Simple allocation: bump pointer from mmap_base */
    uint64_t alloc_addr = addr ? addr : proc->mmap_base;
    if (alloc_addr < VSL_USER_BASE) alloc_addr = VSL_USER_BASE;
    if ((uint64_t)alloc_addr + (uint64_t)size > (uint64_t)VSL_USER_BASE + (uint64_t)VSL_USER_SIZE) return 0;

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
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
        if (g_vsl.mmaps[i].addr == addr && g_vsl.mmaps[i].size == size) {
            /* Remove by shifting */
            for (uint32_t j = i; j < g_vsl.n_mmaps - 1; j++)
                g_vsl.mmaps[j] = g_vsl.mmaps[j + 1];
            g_vsl.n_mmaps--;
            return 0;
        }
    }
    return -1;
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