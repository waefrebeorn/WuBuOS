/* Freestanding ET_DYN fixture for the VSL explicit constructor-call budget.
 *
 * 40 .init_array + 40 .fini_array entries = 80 calls, while each phase stays
 * under the fixed 64-entry per-phase cap.  With vsl_dyn_set_init_fini(1) the
 * engine must stop at exactly 64 charged calls and fail closed with
 * VSL_EXEC_ERR_RELOC before running ANY constructor or the entry point.
 * With init/fini OFF the same bytes are an ordinary guest that exits 0, so
 * the refusal is attributable to the budget, not to the fixture.
 */
typedef void (*ctor_t)(void);

static long sys_exit(long code) {
    long r;
    __asm__ volatile("syscall" : "=a"(r)
                     : "a"(60L), "D"(code)
                     : "rcx", "r11", "memory");
    return r;
}

#define DEF(n)  static void ctor_##n(void) { __asm__ volatile("" ::: "memory"); }
#define SLOT(n) ctor_##n,

DEF(0)  DEF(1)  DEF(2)  DEF(3)  DEF(4)  DEF(5)  DEF(6)  DEF(7)
DEF(8)  DEF(9)  DEF(10) DEF(11) DEF(12) DEF(13) DEF(14) DEF(15)
DEF(16) DEF(17) DEF(18) DEF(19) DEF(20) DEF(21) DEF(22) DEF(23)
DEF(24) DEF(25) DEF(26) DEF(27) DEF(28) DEF(29) DEF(30) DEF(31)
DEF(32) DEF(33) DEF(34) DEF(35) DEF(36) DEF(37) DEF(38) DEF(39)

__attribute__((section(".init_array"), used))
static ctor_t vsl_inits[40] = {
    SLOT(0)  SLOT(1)  SLOT(2)  SLOT(3)  SLOT(4)  SLOT(5)  SLOT(6)  SLOT(7)
    SLOT(8)  SLOT(9)  SLOT(10) SLOT(11) SLOT(12) SLOT(13) SLOT(14) SLOT(15)
    SLOT(16) SLOT(17) SLOT(18) SLOT(19) SLOT(20) SLOT(21) SLOT(22) SLOT(23)
    SLOT(24) SLOT(25) SLOT(26) SLOT(27) SLOT(28) SLOT(29) SLOT(30) SLOT(31)
    SLOT(32) SLOT(33) SLOT(34) SLOT(35) SLOT(36) SLOT(37) SLOT(38) SLOT(39)
};

__attribute__((section(".fini_array"), used))
static ctor_t vsl_finis[40] = {
    SLOT(0)  SLOT(1)  SLOT(2)  SLOT(3)  SLOT(4)  SLOT(5)  SLOT(6)  SLOT(7)
    SLOT(8)  SLOT(9)  SLOT(10) SLOT(11) SLOT(12) SLOT(13) SLOT(14) SLOT(15)
    SLOT(16) SLOT(17) SLOT(18) SLOT(19) SLOT(20) SLOT(21) SLOT(22) SLOT(23)
    SLOT(24) SLOT(25) SLOT(26) SLOT(27) SLOT(28) SLOT(29) SLOT(30) SLOT(31)
    SLOT(32) SLOT(33) SLOT(34) SLOT(35) SLOT(36) SLOT(37) SLOT(38) SLOT(39)
};

void _start(void) { sys_exit(0); }
