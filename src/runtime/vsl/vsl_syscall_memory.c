/*
 * vsl_syscall_memory.c  --  VSL Memory Management Syscalls
 * mmap, munmap, brk, mprotect, msync, mremap, etc.
 */

#include "vsl_syscall_internal.h"
#include <sys/shm.h>

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
    int result = mprotect((void *)addr, (size_t)len, (int)prot);
    return result < 0 ? -errno : (int64_t)result;
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