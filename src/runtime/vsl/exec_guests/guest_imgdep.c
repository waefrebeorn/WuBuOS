/* Main fixture for the per-image dependency search-directory gate.
 * DT_NEEDED is the plain soname libvslimgdep.so; two directories each hold
 * a different object with that soname.  The image writes the 8-byte magic
 * of whichever copy its OWN search list selected.  No libc, no ld.so. */
extern long write(long fd, const void *buf, unsigned long len);
extern unsigned long vsl_imgdep_magic(void);

void _start(void) {
    unsigned long magic = vsl_imgdep_magic();
    write(1, &magic, 8);
    __asm__ volatile("syscall" : : "a"(60L), "D"(0L)
                     : "rcx", "r11", "memory");    /* raw exit(0) */
    __builtin_unreachable();
}
