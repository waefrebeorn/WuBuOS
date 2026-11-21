/*
 * guest_libc_shim.c -- bounded GUEST libc shim for the VSL ld.so path.
 *
 * A real dynamically linked gcc binary imports versioned symbols from
 * libc.so.6 (puts@GLIBC_2.2.5, __libc_start_main@GLIBC_2.34,
 * __cxa_finalize@GLIBC_2.2.5).  VSL's bounded loader resolves a VERSIONED
 * import ONLY from a merged DT_NEEDED export table -- never from the fixed
 * host table.  This freestanding ET_DYN object is that provider: it is linked
 * with soname libc.so.6 and carries real DT_VERDEF/DT_VERSYM records, so the
 * main image's versioned imports bind to guest bytes that execute inside the
 * VSL trap window.  No host libc, no ld.so, no %fs: every observed byte is
 * either a raw syscall (trapped by VSL) or a direct guest call.
 *
 * puts() writes the NUL-terminated string (plus '\n') to fd 1 with the write
 * syscall; VSL's vsl_sys_write captures it through the existing memfd path.
 * __libc_start_main() calls the real main() pointer handed over by crt1's
 * _start, then exits via exit_group (routed through the real VSL exit
 * handler).  __cxa_finalize() is a no-op destructor hook.
 *
 * VSL_SHIM_BADVER makes the puts/cxa version name WRONG (VSL_BAD_9) so a gate
 * can prove the versioned lookup fails closed with the exact symbol name.
 */
typedef __SIZE_TYPE__ size_t;

#ifndef VSL_SHIM_PUTS_VER
#define VSL_SHIM_PUTS_VER "GLIBC_2.2.5"
#endif
#ifndef VSL_SHIM_CXA_VER
#define VSL_SHIM_CXA_VER "GLIBC_2.2.5"
#endif

static long sys_write(unsigned long fd, const void *buf, unsigned long n) {
    long r;
    __asm__ volatile("syscall" : "=a"(r)
                     : "a"(1UL), "D"(fd), "S"(buf), "d"(n)
                     : "rcx", "r11", "memory");
    return r;
}
static void sys_exit_group(int code) {
    __asm__ volatile("syscall" :: "a"(231UL), "D"((long)code)
                     : "rcx", "r11", "memory");
    __builtin_unreachable();
}
static size_t shim_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int shim_puts(const char *s) {
    size_t n = shim_strlen(s);
    (void)sys_write(1, s, n);
    (void)sys_write(1, "\n", 1);
    return (int)n;
}

typedef int (*vsl_main_fn)(int, char **, char **);
int shim_start(vsl_main_fn m, int argc, char **argv,
               void *init, void *fini, void *rtld_fini, void *stack_end) {
    (void)init; (void)fini; (void)rtld_fini; (void)stack_end;
    int rc = m(argc, argv, (char **)0);
    sys_exit_group(rc);
    return rc;
}

void shim_cxa_finalize(void *p) { (void)p; }

__asm__(".symver shim_puts,puts@@" VSL_SHIM_PUTS_VER);
__asm__(".symver shim_start,__libc_start_main@@GLIBC_2.34");
__asm__(".symver shim_cxa_finalize,__cxa_finalize@@" VSL_SHIM_CXA_VER);
