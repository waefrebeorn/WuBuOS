/* Guest: uname(buf); write(1, buf.sysname, 65); exit(0).
 * The REAL vsl_sys_uname calls host uname() and copies struct utsname. */
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
struct guest_utsname { char f[6][65]; };   /* matches glibc struct utsname */
void _start(void) {
    struct guest_utsname u;
    long rc = sys1(63, (long)&u);           /* uname */
    if (rc != 0) { sys1(60, 1); for (;;) {} }
    sys3(1, 1, (long)u.f[0], 65);           /* write sysname field */
    sys1(60, 0);                            /* exit */
    for (;;) {}
}
