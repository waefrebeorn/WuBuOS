/*
 * vsl_prot.c -- read-only protection lookup over VSL's existing records.
 * Single source of truth: g_vsl.mmaps[] and g_vsl.procs[].mprot[], the same
 * records vsl_mprotect() writes. No duplicate protection table exists here.
 */
#include "vsl_prot.h"
#include "vsl_internal.h"

int vsl_prot_query(uint64_t va) {
    if (!g_vsl.active) return -1;
    /* Only the virtual user arena is a managed VSL mapping. Everything
     * else (legacy flat guest RAM, ELF image below the arena, stack, ...)
     * stays permissive by construction. */
    if (va < VSL_USER_BASE || va >= VSL_USER_BASE + VSL_USER_SIZE) return -1;

    int prot = -1;
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
        VSL_MMAP *mm = &g_vsl.mmaps[i];
        if (va >= mm->addr && va < mm->addr + (uint64_t)mm->size) {
            prot = mm->prot & 7;
            break;
        }
    }
    /* mprotect overrides win over the original mmap protection (they also
     * cover arena ranges the bump allocator does not track, e.g. brk heap). */
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    if (p) {
        for (uint32_t i = 0; i < p->n_mprot && i < 16; i++) {
            uint64_t a = p->mprot[i].addr;
            uint64_t rounded = ((uint64_t)p->mprot[i].len + 0xfff) & ~0xfffull;
            if (rounded == 0) rounded = 0x1000;
            if (va >= a && va < a + rounded)
                prot = p->mprot[i].prot & 7;
        }
    }
    return prot;
}

int vsl_prot_allows(uint64_t va, int bit) {
    int prot = vsl_prot_query(va);
    if (prot < 0) return 1;          /* legacy/unmanaged memory: permissive */
    return (prot & bit) ? 1 : 0;
}
