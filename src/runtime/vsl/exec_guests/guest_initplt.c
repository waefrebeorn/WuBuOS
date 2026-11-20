/* Freestanding ET_DYN MAIN fixture for the VSL lazy-PLT + init/fini gate.
 *
 * Built as a real gcc -shared object with TWO imported functions:
 * vsl_test_shim is called TWICE through one DT_JMPREL JUMP_SLOT (the resolver
 * must run exactly once for that slot), and write is called once through a
 * second JUMP_SLOT WITH ARGUMENTS (fd=1, buffer pointer, length) to prove the
 * resolver trampoline preserves every argument register.  With lazy binding
 * enabled both slots are bound on their first call and never resolved again.
 *
 * .init_array writes a marker before main; .fini_array writes a marker during
 * unload.  Both use raw syscalls so they are trapped by the VSL engine like
 * any guest syscall (no libc, no ld.so).  Both are OPT-IN in the loader; the
 * pre-change loader skips them entirely, so the markers go missing.
 *
 * Exact output (in order), 34 bytes:
 *   "VSL-INIT\n"        (9)  written by init_array before _start
 *   8 + 8 raw magic words (16) written by _start (the 2nd via lazy write@plt)
 *   "VSL-FINI\n"        (9)  written by fini_array on unload
 */
extern unsigned long vsl_test_shim(void);
extern long write(long fd, const void *buf, unsigned long len);

static long sys_write(long fd, const void *buf, unsigned long len) {
    long r;
    __asm__ volatile("syscall" : "=a"(r)
                     : "a"(1L), "D"(fd), "S"(buf), "d"(len)
                     : "rcx", "r11", "memory");
    return r;
}
static void sys_exit(long code) {
    __asm__ volatile("syscall" : : "a"(60L), "D"(code)
                     : "rcx", "r11", "memory");
    __builtin_unreachable();
}

static void init_marker(void) { (void)sys_write(1, "VSL-INIT\n", 9); }
static void fini_marker(void) { (void)sys_write(1, "VSL-FINI\n", 9); }

typedef void (*ctor_t)(void);
__attribute__((section(".init_array"), used)) static ctor_t vsl_init = init_marker;
__attribute__((section(".fini_array"), used)) static ctor_t vsl_fini = fini_marker;

void _start(void) {
    unsigned long a = vsl_test_shim();       /* PLT call #1 (lazy bind) */
    unsigned long b = vsl_test_shim();       /* PLT call #2 (already bound) */
    (void)sys_write(1, &a, 8);               /* raw syscall, no lazy slot */
    (void)write(1, &b, 8);                   /* lazy PLT call WITH arguments */
    sys_exit(0);
}
