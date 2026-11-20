/* Freestanding ET_DYN MAIN fixture for the VSL DT_NEEDED gate.
 *
 * Linked as a real gcc -shared object AGAINST guest_dep.elf (soname
 * libvsldep.so), so its PT_DYNAMIC carries DT_NEEDED "libvsldep.so" and a
 * JUMP_SLOT for vsl_dep_entry.  Under the OLD loader the DT_NEEDED was
 * ignored and vsl_dep_entry was an unknown symbol -> hard ERR_RELOC; this
 * fixture is the differential negative control.
 *
 * Output (exact, in order):
 *   14 bytes "VSLDEP-CALLED\n" written by the dependency's called function,
 *    8 bytes of that function's real return magic,
 *   11 bytes "VSLMAIN-OK\n" written by the main guest.
 * No libc, no ld.so: only VSL relocation + dependency loading is involved. */
extern long write(long fd, const void *buf, unsigned long len);
extern unsigned long vsl_dep_entry(void);

void _start(void) {
    unsigned long magic = vsl_dep_entry();       /* JUMP_SLOT into the dep */
    write(1, &magic, 8);                         /* host write shim */
    write(1, "VSLMAIN-OK\n", 11);                /* host write shim */

    __asm__ volatile("syscall" : : "a"(60L), "D"(0L)
                     : "rcx", "r11", "memory");  /* raw exit(0) */
    __builtin_unreachable();
}
