/* Guest: exit_group(77) -- the second exit path must unwind too. */
static long sys1(long n, long a) {
    long r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a) : "rcx", "r11", "memory");
    return r;
}
void _start(void) {
    sys1(231, 77);          /* exit_group(77) */
    for (;;) {}
}
