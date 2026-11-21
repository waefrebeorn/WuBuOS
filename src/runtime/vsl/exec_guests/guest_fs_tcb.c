/*
 * guest_fs_tcb.c -- bounded arch_prctl(ARCH_SET_FS)/TCB fixture.
 *
 * A freestanding ET_EXEC guest that installs its own %fs base with the real
 * arch_prctl syscall (trapped by the VSL seccomp filter -> SIGSYS), then
 * reads the TCB header the engine seeds there (self pointer at %fs:0, dtv
 * pointer at %fs:8) and the dtv's module-id layout, emitting exact u64 values
 * on fd 1 before exit(0).  The gate asserts the engine really switched the
 * thread FS base and laid out a real module-id TLS table.
 */
#include <stdint.h>

#define SYS_WRITE       1L
#define SYS_EXIT        60L
#define SYS_ARCH_PRCTL  158L
#define ARCH_SET_FS     0x1002UL
#define ARCH_GET_FS     0x1003UL

static uint8_t tcb[64] __attribute__((aligned(16)));

static long raw_write(int fd, const void *buf, unsigned long n) {
    long r;
    __asm__ volatile("syscall" : "=a"(r)
                     : "a"(SYS_WRITE), "D"((long)fd), "S"(buf), "d"(n)
                     : "rcx", "r11", "memory");
    return r;
}
static long raw_arch_prctl(unsigned long opt, unsigned long arg) {
    long r;
    __asm__ volatile("syscall" : "=a"(r)
                     : "a"(SYS_ARCH_PRCTL), "D"(opt), "S"(arg)
                     : "rcx", "r11", "memory");
    return r;
}

void _start(void) {
    uint64_t tcb_addr = (uint64_t)(uintptr_t)&tcb[0];
    int64_t  set_rc = raw_arch_prctl(ARCH_SET_FS, tcb_addr);

    uint64_t self = 0, dtv = 0, got = 0;
    if (set_rc == 0) {
        __asm__ volatile("mov %%fs:0, %0" : "=r"(self));
        __asm__ volatile("mov %%fs:8, %0" : "=r"(dtv));
    }
    (void)raw_arch_prctl(ARCH_GET_FS, (uint64_t)(uintptr_t)&got);

    uint64_t cnt = 0, mod1 = 0;
    if (dtv) {
        uint64_t *d = (uint64_t *)(uintptr_t)dtv;
        cnt = d[0];                  /* module count */
        mod1 = d[1];                 /* module 1 TLS base */
    }

    /* 7 x u64 = 56 bytes of exact evidence. */
    raw_write(1, &set_rc, 8);
    raw_write(1, &self, 8);
    raw_write(1, &dtv, 8);
    raw_write(1, &got, 8);
    raw_write(1, &tcb_addr, 8);
    raw_write(1, &cnt, 8);
    raw_write(1, &mod1, 8);
    __asm__ volatile("syscall" :: "a"(SYS_EXIT), "D"(0L) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
