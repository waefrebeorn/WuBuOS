/*
 * guest_copy.c -- REAL dynamically linked glibc binary with a genuine
 * R_X86_64_COPY relocation of a libc DATA symbol into the main image.
 *
 * Built by mk/tests.mk with plain gcc (no -static/-nostdlib), so it has
 * PT_INTERP /lib64/ld-linux-x86-64.so.2 and DT_NEEDED libc.so.6.  Reading the
 * libc data symbol `optind` makes the linker emit:
 *
 *     R_X86_64_COPY optind@GLIBC_2.2.5   (4 bytes, into the main image)
 *
 * (verified with `readelf -r`).  VSL maps the real ld.so from the guest
 * rootfs, resolves the DT_NEEDED libc.so.6 PROVIDER from that same guest
 * rootfs, and copies the provider's versioned `optind@@GLIBC_2.2.5` bytes
 * into this image's COPY destination -- bounded by the provider's st_size.
 *
 * The only code import is GLOB_DAT __libc_start_main@GLIBC_2.34 (plus weak
 * __gmon_start__/_ITM_* -> 0).  The guest libc shim provides the former.  The
 * copied int is emitted both as one raw byte on fd 1 and as the exit code, so
 * the gate can prove the COPY wrote the provider's exact sentinel (0x5a)
 * rather than the pre-relocation zero.
 */

extern int optind;   /* libc.so.6 data, served by R_X86_64_COPY */

static long sys_write(unsigned long fd, const void *buf, unsigned long n) {
    long r;
    __asm__ volatile("syscall" : "=a"(r)
                     : "a"(1UL), "D"(fd), "S"(buf), "d"(n)
                     : "rcx", "r11", "memory");
    return r;
}

int main(void) {
    int v = optind;                 /* loaded from the COPY destination */
    unsigned char b = (unsigned char)v;
    (void)sys_write(1, &b, 1);
    return v & 0xff;
}
