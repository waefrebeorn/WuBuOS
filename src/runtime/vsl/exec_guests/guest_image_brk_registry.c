/* guest_image_brk_registry.c -- ELF-image / brk registry differential guest.
 *
 * Freestanding ET_EXEC linked into the VSL user window.  Grows the brk heap
 * by one page, then asks for one anonymous page with the OLD break as its
 * advisory hint.  With the heap registered in the allocator registry the
 * collision walk skips the live heap and returns old+0x1000; with the
 * registry off (pre-fix behaviour) the hint is handed back unchanged and
 * aliases the heap.  Emits old_brk, new_brk and mmap_result (24 bytes) and
 * exits 0.  No host pages are requested from the guest side: this measures
 * the registry and the engine's exact accounting, not guest dereferences.
 */
#define SYS_WRITE  1
#define SYS_MMAP   9
#define SYS_BRK    12
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

void _start(void) {
    long old_brk = sys1(SYS_BRK, 0);                 /* query */
    long new_brk = sys1(SYS_BRK, old_brk + 0x1000);  /* grow one page */
    long mapped  = sys6(SYS_MMAP, old_brk, 0x1000, PROT_RW,
                        MAP_PRIVATE_F | MAP_ANON_F, -1, 0);

    unsigned char out[24];
    unsigned long v[3] = {
        (unsigned long)old_brk, (unsigned long)new_brk,
        (unsigned long)mapped
    };
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 8; j++)
            out[i * 8 + j] = (unsigned char)(v[i] >> (8 * j));
    (void)sys3(SYS_WRITE, 1, (long)out, 24);
    sys1(SYS_EXIT, 0);
    for (;;) {}
}
