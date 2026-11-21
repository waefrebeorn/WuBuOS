/* guest_munmap_release.c -- mid-run host-page release differential fixture.
 *
 * Freestanding ET_EXEC linked into the VSL user window.  All numbers are raw
 * Linux x86-64 syscalls, trapped and dispatched by VSL; no libc, no ld.so.
 *
 * Sequence (all addresses page-aligned, one 4-page anonymous mapping):
 *   1. mmap 4 pages -> real host pages are materialized at the returned addr.
 *   2. write a distinct 64-bit marker into EACH page (proves all four are
 *      genuinely backed; a registry-only mapping faults here).
 *   3. munmap the MIDDLE two pages -> the host pages for that range must be
 *      released while the outer two pages survive (a split in the owner table).
 *   4. read the two surviving pages -> their markers must be intact.
 *   5. munmap each surviving page -> the whole 4-page mapping is now freed.
 *   6. MAP_FIXED mmap ONE page back at the ORIGINAL address.  This only works
 *      if the host pages were really released on the host: with the release
 *      switch OFF they are still mapped, MAP_FIXED_NOREPLACE refuses, and the
 *      guest sees -ENOMEM instead of the original address.
 *   7. write/read back one marker in the reused page, emit the observations
 *      (56 bytes) and exit(0).
 *
 * The probe asserts the guest output AND the engine's exact byte accounting
 * (released bytes/ranges, live budget after release, no fault).
 */
#define SYS_WRITE 1
#define SYS_MMAP  9
#define SYS_MUNMAP 11
#define SYS_EXIT  60

static long sys1(long n, long a) {
    long r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a)
                     : "rcx", "r11", "memory");
    return r;
}
static long sys3(long n, long a, long b, long c) {
    long r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return r;
}
static long sys6(long n, long a, long b, long c, long d, long e, long f) {
    register long r10 __asm__("r10") = d;
    register long r8  __asm__("r8")  = e;
    register long r9  __asm__("r9")  = f;
    long r;
    __asm__ volatile("syscall" : "=a"(r)
                     : "a"(n), "D"(a), "S"(b), "d"(c),
                       "r"(r10), "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory");
    return r;
}

#define PROT_RW         0x03u
#define MAP_PRIVATE_F   0x02u
#define MAP_ANON_F      0x20u
#define MAP_FIXED_F     0x10u
#define SZ4             0x4000u
#define SZ1             0x1000u

void _start(void) {
    /* 1. one 4-page anonymous mapping */
    long p = sys6(SYS_MMAP, 0, SZ4, PROT_RW,
                  MAP_PRIVATE_F | MAP_ANON_F, -1, 0);

    /* 2. real stores into every page */
    long v0 = 0, v3 = 0, v2 = 0;
    if (p > 0) {
        *(volatile unsigned long *)(p + 0x0000) = 0x1111111111111111UL;
        *(volatile unsigned long *)(p + 0x1000) = 0x2222222222222222UL;
        *(volatile unsigned long *)(p + 0x2000) = 0x3333333333333333UL;
        *(volatile unsigned long *)(p + 0x3000) = 0x4444444444444444UL;
    }

    /* 3. release the middle two pages (split in the backing owner table) */
    long rc_mid = (p > 0) ? sys3(SYS_MUNMAP, p + 0x1000, 0x2000, 0) : -1;

    /* 4. the two outer pages must survive with their markers intact */
    if (p > 0) {
        v0 = *(volatile unsigned long *)(p + 0x0000);
        v3 = *(volatile unsigned long *)(p + 0x3000);
    }

    /* 5. release both surviving pages -> the whole mapping is freed */
    long rc_left  = (p > 0) ? sys3(SYS_MUNMAP, p + 0x0000, SZ1, 0) : -1;
    long rc_right = (p > 0) ? sys3(SYS_MUNMAP, p + 0x3000, SZ1, 0) : -1;

    /* 6. re-map ONE page FIXED at the original address: succeeds only when
     *    the host pages were genuinely released mid-run. */
    long p2 = (p > 0) ? sys6(SYS_MMAP, p, SZ1, PROT_RW,
                             MAP_PRIVATE_F | MAP_ANON_F | MAP_FIXED_F,
                             -1, 0) : -1;
    if (p2 > 0) {
        *(volatile unsigned long *)p2 = 0x5555555555555555UL;
        v2 = *(volatile unsigned long *)p2;
    }

    /* 7. emit 7 x 8-byte little-endian words (56 bytes) */
    unsigned char out[56];
    unsigned long vals[7] = {
        (unsigned long)p, (unsigned long)rc_mid, (unsigned long)p2,
        (unsigned long)v0, (unsigned long)v3,
        (unsigned long)rc_left, (unsigned long)rc_right
    };
    for (int i = 0; i < 7; i++)
        for (int j = 0; j < 8; j++)
            out[i * 8 + j] = (unsigned char)(vals[i] >> (8 * j));
    /* marker from the reused page is folded into the top byte of the last
     * word only when the reuse succeeded; the probe reads it from v2 via a
     * second short write below. */
    (void)sys3(SYS_WRITE, 1, (long)out, 56);
    unsigned char tail[8];
    unsigned long u2 = (unsigned long)v2;
    for (int j = 0; j < 8; j++) tail[j] = (unsigned char)(u2 >> (8 * j));
    (void)sys3(SYS_WRITE, 1, (long)tail, 8);
    sys1(SYS_EXIT, 0);
    for (;;) {}
}
