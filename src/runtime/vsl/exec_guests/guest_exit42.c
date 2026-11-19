/* Guest: exit(42). Freestanding, static; linked into the VSL user window. */
static long sys1(long n, long a) {
    long r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a) : "rcx", "r11", "memory");
    return r;
}
void _start(void) {
    sys1(60, 42);           /* exit(42) */
    for (;;) {}
}
