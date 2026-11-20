/* Freestanding ET_DYN MAIN fixture for the bounded ELF symbol-versioning
 * gate.  Linked against libvslverA.so FIRST and libvslverB.so second, it
 * carries DT_VERNEED entries requiring vsl_ver_entry@VSL_2 (from B) and
 * vsl_weakpick@VSL_1 (weak in A, strong in B).  A base-name-only loader
 * picks A's wrong version and weak symbol; the version-aware VSL loader
 * must resolve both correctly.  Output is two 8-byte magic words, then
 * exit(0) through the VSL host syscall shim.  No libc, no ld.so. */
extern long write(long fd, const void *buf, unsigned long len);
extern unsigned long vsl_ver_entry(void);
extern unsigned long vsl_weakpick(void);

__asm__(".symver vsl_ver_entry, vsl_ver_entry@VSL_2");
__asm__(".symver vsl_weakpick, vsl_weakpick@VSL_1");

void _start(void) {
    unsigned long ver = vsl_ver_entry();          /* JUMP_SLOT -> dep B, VSL_2 */
    unsigned long weak = vsl_weakpick();          /* JUMP_SLOT -> dep B, strong */
    write(1, &ver, 8);
    write(1, &weak, 8);
    __asm__ volatile("syscall" : : "a"(60L), "D"(0L)
                     : "rcx", "r11", "memory");    /* raw exit(0) */
    __builtin_unreachable();
}
