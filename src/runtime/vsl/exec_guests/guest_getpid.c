/* Guest: getpid(); write(1, &pid, 8); exit(0).
 * The REAL vsl_sys_getpid returns g_vsl.current_pid (the VSL pid),
 * NOT the host pid -- the gate asserts exactly that semantic. */
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
void _start(void) {
    long pid = sys1(39, 0);                 /* getpid */
    sys3(1, 1, (long)&pid, 8);              /* write raw 8-byte value */
    sys1(60, 0);                            /* exit */
    for (;;) {}
}
