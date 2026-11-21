/* guest_munmap_coalesce.c -- bounded munmap-coalescing regression fixture.
 *
 * Freestanding ET_EXEC linked into the VSL user window.  Issues three
 * anonymous page mmaps (a,b,c contiguous from the bump pointer), unmaps the
 * two adjacent upper pages (b,c), then requests ONE two-page mmap.  When
 * the allocator coalesces the two freed pages into a single free range the
 * last mmap reuses b's address; the legacy bump allocator returns a fresh
 * address past c.  Emits a,b,c,d (32 bytes) via the write syscall and exits
 * 0.  No host backing is requested, so no guest pointer is dereferenced:
 * this gate measures the registry, not host pages.
 */
#define SYS_WRITE  1
#define SYS_MMAP   9
#define SYS_MUNMAP 11
#define SYS_EXIT   60

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

#define PROT_RW        0x03u
#define MAP_PRIVATE_F  0x02u
#define MAP_ANON_F     0x20u

static long anon_map(long sz) {
    return sys6(SYS_MMAP, 0, sz, PROT_RW,
                MAP_PRIVATE_F | MAP_ANON_F, -1, 0);
}

void _start(void) {
    long a = anon_map(0x1000);
    long b = anon_map(0x1000);
    long c = anon_map(0x1000);
    (void)sys3(SYS_MUNMAP, b, 0x1000, 0);
    (void)sys3(SYS_MUNMAP, c, 0x1000, 0);
    long d = anon_map(0x2000);          /* needs BOTH freed pages */

    unsigned char out[32];
    unsigned long v[4] = {
        (unsigned long)a, (unsigned long)b, (unsigned long)c, (unsigned long)d
    };
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 8; j++)
            out[i * 8 + j] = (unsigned char)(v[i] >> (8 * j));
    (void)sys3(SYS_WRITE, 1, (long)out, 32);
    sys1(SYS_EXIT, 0);
    for (;;) {}
}
