/* Freestanding ET_DYN fixture for the hardened weak-UNDEF gate.
 *
 * vsl_absent_weak is an UNDEFINED WEAK symbol with no definition in the
 * image, in the fixed host table, or in any dependency.  Standard ELF
 * semantics bind such a symbol to address 0.  The guest writes the RESOLVED
 * ADDRESS (it never calls it, so a zero binding is a clean run) and exits 0.
 * Under the pre-change loader the missing definition failed closed with
 * VSL_EXEC_ERR_RELOC -- the differential control.  No libc, no ld.so. */
extern long write(long fd, const void *buf, unsigned long len);
extern unsigned long vsl_absent_weak(void) __attribute__((weak));

void _start(void) {
    unsigned long addr = (unsigned long)&vsl_absent_weak;
    write(1, &addr, 8);
    __asm__ volatile("syscall" : : "a"(60L), "D"(0L)
                     : "rcx", "r11", "memory");    /* raw exit(0) */
    __builtin_unreachable();
}
