/* Freestanding ET_DYN fixture: the same image as guest_weakundef.c but the
 * missing symbol has STRONG (GLOBAL) binding.  It MUST fail closed under
 * both the hardened and the legacy loader -- this is the control proving
 * weak-UNDEF zero-binding did not weaken strong-UNDEF handling.  No libc. */
extern long write(long fd, const void *buf, unsigned long len);
extern unsigned long vsl_absent_strong(void);

void _start(void) {
    unsigned long addr = (unsigned long)&vsl_absent_strong;
    write(1, &addr, 8);
    __asm__ volatile("syscall" : : "a"(60L), "D"(0L)
                     : "rcx", "r11", "memory");    /* raw exit(0) */
    __builtin_unreachable();
}
