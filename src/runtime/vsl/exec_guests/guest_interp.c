/*
 * guest_interp.c -- bounded PT_INTERP interpreter fixture.
 *
 * Linked as a freestanding ET_DYN shared object and placed in the guest
 * rootfs as /libvslinterp.so.  When the engine loads a main image carrying
 * PT_INTERP, it maps/relocates THIS image, transfers control to its _start
 * and lays out the kernel-shaped auxv on the guest stack.  _start records
 * RSP, calls interp_dump(), which walks argc/argv/envp/auxv and emits the
 * raw little-endian u64 values of AT_PHDR(3), AT_PHENT(4), AT_PHNUM(5),
 * AT_BASE(7), AT_ENTRY(9), AT_PAGESZ(6) and AT_RANDOM(25) as 7x8=56 bytes
 * on fd 1, then exits(0).  The gate compares those values against the main
 * image's real link geometry, so this is a real auxv-layout assertion.
 *
 * All syscalls are issued from guest text inside the VSL window, so they
 * go through seccomp TRAP -> vsl_syscall_dispatch (the real handlers).
 */
#include <stdint.h>

volatile uint64_t g_vsl_interp_sp __attribute__((visibility("hidden")));

static long raw_write(int fd, const void *buf, unsigned long n) {
    long r;
    __asm__ volatile("syscall"
                     : "=a"(r)
                     : "a"(1L), "D"(fd), "S"(buf), "d"(n)
                     : "rcx", "r11", "memory");
    return r;
}

void vsl_interp_dump(void);

__attribute__((naked, used)) void _start(void) {
    __asm__ volatile(
        "mov %%rsp, g_vsl_interp_sp(%%rip)\n\t"
        "and $-16, %%rsp\n\t"
        "call vsl_interp_dump\n\t"
        "mov $60, %%eax\n\t"
        "xor %%edi, %%edi\n\t"
        "syscall\n\t"
        "hlt\n\t"
        ::: "memory");
}

void vsl_interp_dump(void) {
    uint64_t *sp = (uint64_t *)(uintptr_t)g_vsl_interp_sp;
    uint64_t argc = *sp++;
    sp += argc;                 /* argv[0 .. argc-1] */
    sp++;                       /* argv NULL */
    while (*sp++) { }           /* envp until NULL */
    uint64_t *aux = sp;         /* auxv: alternating tag/value, ends AT_NULL */
    static const uint64_t want[7] = { 3, 4, 5, 7, 9, 6, 25 };
    for (unsigned i = 0; i < 7; i++) {
        uint64_t v = 0;
        for (uint64_t *a = aux; *a != 0; a += 2)
            if (*a == want[i]) { v = a[1]; break; }
        raw_write(1, &v, 8);
    }
}
