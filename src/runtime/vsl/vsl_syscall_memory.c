/*
 * vsl_syscall_memory.c  --  VSL Memory Management Syscalls
 * mmap, munmap, brk, mprotect, msync, mremap, etc.
 */

#include "../../egojar.h"
#include "vsl_syscall_internal.h"

/* ====================================================================
 * MEMORY MANAGEMENT SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_mmap(uint64_t addr, uint64_t size, uint64_t prot,
                      uint64_t flags, uint64_t fd, uint64_t offset) {
    return (int64_t)vsl_mmap(addr, (size_t)size, (int)prot, (int)flags, (int)fd, offset);
}

int64_t vsl_sys_munmap(uint64_t addr, uint64_t size, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    return vsl_munmap(addr, (size_t)size);
}

int64_t vsl_sys_brk(uint64_t new_brk, uint64_t b, uint64_t c,
                     uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    return vsl_brk(new_brk);
}

int64_t vsl_sys_mprotect(uint64_t addr, uint64_t len, uint64_t prot,
                          uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    /* Virtual user-arena semantics (L14): the addresses here are GUEST
     * addresses (VSL_USER_BASE..), not host pointers — host mprotect on
     * them would fail with ENOMEM (glibc RELRO hit exactly that). Apply
     * kernel validation, update tracked mmap protections, and record
     * the override for ranges the bump allocator does not track (ELF
     * image, brk heap). Enforcement comes with the WEMU paging leg. */
    if (!g_vsl.active) return -1;
    if (addr & 0xfff) return -22;                    /* EINVAL: unaligned */
    if (len == 0) return 0;
    /* Round only AFTER proving len is inside the arena: len near
     * UINT64_MAX wrapped the rounded length and the end pointer, letting
     * mprotect "succeed" outside the guest arena. */
    if (len > VSL_USER_SIZE) return -12;             /* ENOMEM */
    if (addr < 0x1000 || addr >= VSL_USER_BASE + VSL_USER_SIZE)
        return -12;
    uint64_t rounded = (len + 0xfff) & ~0xfffull;
    if (rounded > VSL_USER_BASE + VSL_USER_SIZE - addr) return -12;
    uint64_t end = addr + rounded;
    /* Accept the low ELF-image region (statics load at 0x400000, below
     * the bump arena) as well as the arena itself: glibc mprotects the
     * ELF's GNU_RELRO segment right after startup. */
    if (addr < 0x1000 || end > VSL_USER_BASE + VSL_USER_SIZE)
        return -12;                                  /* ENOMEM */
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
        VSL_MMAP *mm = &g_vsl.mmaps[i];
        if (mm->addr < end && mm->addr + mm->size > addr)
            mm->prot = (int)prot;
    }
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    if (p) {
        uint32_t i = 0;
        for (; i < p->n_mprot; i++)
            if (p->mprot[i].addr == addr && p->mprot[i].len == len) break;
        if (i == p->n_mprot && p->n_mprot < 16) p->n_mprot++;
        if (i < 16) {
            p->mprot[i].addr = addr;
            p->mprot[i].len = len;
            p->mprot[i].prot = (int)prot;
        }
    }
    return 0;
}

int64_t vsl_sys_msync(uint64_t addr, uint64_t len, uint64_t flags,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = msync((void *)addr, (size_t)len, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_mremap(uint64_t old_addr, uint64_t old_size, uint64_t new_size,
                        uint64_t flags, uint64_t new_addr, uint64_t f) {
    (void)f;
    long result = syscall(SYS_mremap, (void *)old_addr, (size_t)old_size, (size_t)new_size,
                           (unsigned long)flags, (void *)new_addr);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_madvise(uint64_t addr, uint64_t len, uint64_t advice,
                         uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = madvise((void *)addr, (size_t)len, (int)advice);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_mlock(uint64_t addr, uint64_t len,
                       uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = mlock((void *)addr, (size_t)len);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_munlock(uint64_t addr, uint64_t len,
                         uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = munlock((void *)addr, (size_t)len);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_mlockall(uint64_t flags, uint64_t b, uint64_t c,
                          uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = mlockall((int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_munlockall(uint64_t a, uint64_t b, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = munlockall();
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_shmget(uint64_t key, uint64_t size, uint64_t flags,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = shmget((key_t)key, (size_t)size, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_shmat(uint64_t shmid, uint64_t addr, uint64_t flags,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    void *result = shmat((int)shmid, (const void *)addr, (int)flags);
    return result == (void *)-1 ? -errno : (int64_t)(uintptr_t)result;
}

int64_t vsl_sys_shmdt(uint64_t addr, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = shmdt((const void *)addr);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_shmctl(uint64_t shmid, uint64_t cmd, uint64_t buf,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = shmctl((int)shmid, (int)cmd, (struct shmid_ds *)buf);
    return result < 0 ? -errno : (int64_t)result;
}