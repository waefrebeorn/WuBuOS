/* Guest: write(1, msg, len); exit(0). Output must be captured verbatim. */
static long sys1(long n, long a) {
    long r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a) : "rcx", "r11", "memory");
    return r;
}
static long sys3(long n, long a, long b, long c) {
    long r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return r;
}
static const char msg[] = "VSL-L11: real write via vsl_sys_write\n";
void _start(void) {
    long n = sys3(1, 1, (long)msg, (long)(sizeof(msg) - 1));  /* write */
    sys1(60, n == (long)(sizeof(msg) - 1) ? 0 : 3);           /* exit */
    for (;;) {}
}
