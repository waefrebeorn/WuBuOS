/* guest_mmap_align.c -- page-aligned mmap-bump regression fixture.
 *
 * Freestanding ET_EXEC linked into the VSL user window.  Issues two
 * anonymous mmaps: the first has a NON-page-sized length (0x1801), and the
 * second's advisory address is set just inside that first mapping's tail so
 * the legacy bump allocator is forced to advance past an unaligned end.  Each
 * returned address is then dereferenced (a real store, not a write syscall):
 * if backing was refused for an unaligned return the guest faults instead of
 * completing.  Emits both 8-byte returns plus a marker byte from each mapping
 * (18 bytes total) and exits 0.
 *
 * No libc: raw Linux x86-64 syscalls, all trapped and dispatched by VSL.
 */
#define SYS_WRITE 1
#define SYS_MMAP  9
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
#define SZ1             0x1801u   /* deliberately NOT page-sized */
#define SZ2             0x1000u

void _start(void) {
    long a1 = sys6(SYS_MMAP, 0, SZ1, PROT_RW,
                   MAP_PRIVATE_F | MAP_ANON_F, -1, 0);
    /* Advisory hint lands inside the first mapping's unaligned tail. */
    long hint = (a1 > 0) ? (a1 + 0x1000) : 0;
    long a2 = sys6(SYS_MMAP, hint, SZ2, PROT_RW,
                   MAP_PRIVATE_F | MAP_ANON_F, -1, 0);

    unsigned char m1 = 0, m2 = 0;
    if (a1 > 0) {
        *(volatile unsigned char *)a1 = 0xA1;
        m1 = *(volatile unsigned char *)a1;
    }
    if (a2 > 0) {
        *(volatile unsigned char *)a2 = 0xB2;
        m2 = *(volatile unsigned char *)a2;
    }

    unsigned char out[18];
    unsigned long u1 = (unsigned long)a1, u2 = (unsigned long)a2;
    for (int i = 0; i < 8; i++) { out[i]     = (unsigned char)(u1 >> (8 * i));
                                  out[8 + i] = (unsigned char)(u2 >> (8 * i)); }
    out[16] = m1;
    out[17] = m2;
    (void)sys3(SYS_WRITE, 1, (long)out, 18);
    sys1(SYS_EXIT, 0);
    for (;;) {}
}
