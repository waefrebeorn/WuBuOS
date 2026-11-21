/*
 * guest_filemap.c -- bounded file-backed guest mmap fixture.
 *
 * Freestanding ET_EXEC linked into the VSL window.  It reads a 32-byte
 * control block (four little-endian u64s) from /tmp/vsl_filemap_ctrl.bin:
 *   [0] offset, [1] length, [2] prot, [3] flags
 * opens /tmp/vsl_filemap_data.bin, issues one mmap(fd=that file) with those
 * parameters, then writes the 8-byte mmap return followed by `length` bytes
 * read from the returned address to stdout, and exits 0.  The gate supplies
 * the control block, so ONE guest exercises positive byte-exact reads and
 * every fail-closed admission rule without rebuilding the fixture.
 *
 * No libc: everything is a raw Linux x86-64 syscall issued from guest text,
 * so every call is trapped by VSL's IP-scoped seccomp filter and dispatched
 * through the ONE Linux syscall table.
 */
#define SYS_READ  0
#define SYS_WRITE 1
#define SYS_OPEN  2
#define SYS_CLOSE 3
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

static const char ctrl_path[] = "/tmp/vsl_filemap_ctrl.bin";
static const char data_path[] = "/tmp/vsl_filemap_data.bin";

void _start(void) {
    unsigned long p[4] = { 0, 0, 0, 0 };
    long cfd = sys3(SYS_OPEN, (long)ctrl_path, 0, 0);   /* O_RDONLY */
    if (cfd >= 0) {
        (void)sys3(SYS_READ, cfd, (long)p, 32);
        (void)sys1(SYS_CLOSE, cfd);
    }
    long dfd = sys3(SYS_OPEN, (long)data_path, 0, 0);
    long addr = sys6(SYS_MMAP, 0, p[1], p[2], p[3], dfd, p[0]);
    (void)sys3(SYS_WRITE, 1, (long)&addr, 8);           /* mmap return */
    if (addr > 0) {
        /* Direct guest dereference: faults (SIGSEGV) if the mapping was not
         * materialized, unlike a write() syscall which only returns EFAULT. */
        volatile unsigned char first = *(volatile unsigned char *)addr;
        (void)sys3(SYS_WRITE, 1, addr, p[1]);           /* mapped bytes */
        (void)sys3(SYS_WRITE, 1, (long)&first, 1);      /* echoed first byte */
    }
    (void)sys1(SYS_CLOSE, dfd);
    sys1(SYS_EXIT, 0);
    for (;;) {}
}
