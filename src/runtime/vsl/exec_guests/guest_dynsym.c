/* Real ET_DYN (.so) fixture for the VSL bounded symbol-resolution gate.
 *
 * Every undefined symbol here is resolved by VSL's fixed host table, NOT by
 * glibc/ld.so:
 *   write         -> host write() shim (JUMP_SLOT via the PLT/GOT)
 *   vsl_test_shim -> test syscall shim (JUMP_SLOT via the PLT/GOT)
 *   vsl_test_data -> host data string (GLOB_DAT via the GOT)
 * g_self/g_self_ptr exercise a definition inside the image (GLOB_DAT/64 to
 * bias + st_value).  Output lets the gate assert resolved values exactly. */
typedef unsigned long size_t_v;

extern long write(long fd, const void *buf, unsigned long len);
extern unsigned long long vsl_test_shim(void);
extern const char vsl_test_data[];

static const unsigned long long g_self = 0x4141414141414141ULL; /* "AAAAAAAA" */
unsigned long long g_self_ptr = (unsigned long long)(void *)&g_self;

void _start(void) {
    /* Imported host data through GLOB_DAT + imported write through JUMP_SLOT. */
    write(1, vsl_test_data, 16);
    /* Imported test shim through JUMP_SLOT: its 8-byte magic is echoed. */
    unsigned long long v = vsl_test_shim();
    write(1, &v, 8);
    /* Defined symbol reached through its own dynamic-relocated pointer. */
    write(1, (const void *)g_self_ptr, 8);

    /* Raw exit(0), no libc. */
    __asm__ volatile("syscall" : : "a"(60L), "D"(0L) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
