/*
 * wubu_user.c  --  the ring-3 boundary (gap H4)
 *
 * wubu_user_enter constructs a user frame on the CURRENT kernel stack
 * and iretq's to ring 3. The frame: RIP, CS=0x23, RFLAGS (IF set),
 * RSP, SS=0x2B. The first instruction runs at CPL 3; a syscall traps
 * back to the kernel's syscall_entry (STAR/LSTAR), and the C6
 * sanitizer + sysretq return to ring 3. The ring-3 selftest is a tiny
 * position-independent loop that issues syscall 0 (get_uptime).
 */
#include "wubu_user.h"

void wubu_user_enter(uint64_t entry, uint64_t user_stack_top)
{
    /* build the iretq frame on the kernel stack (SS, RSP, RFLAGS, CS,
     * RIP pushed in reverse) */
    __asm__ __volatile__(
        "pushq %0\n"                 /* SS */
        "pushq %1\n"                 /* user RSP */
        "pushq %2\n"                 /* RFLAGS: IF | reserved */
        "pushq %3\n"                 /* CS */
        "pushq %4\n"                 /* RIP */
        "iretq\n"
        :
        : "r"((uint64_t)WUBU_USER_SS),
          "r"(user_stack_top),
          "r"(0x202ull),             /* IF set */
          "r"((uint64_t)WUBU_USER_CS),
          "r"(entry)
        : "memory");
    /* never returns */
    for (;;) __asm__ __volatile__("pause");
}

/* ring-3 selftest: position-independent; syscall 0 (get_uptime) with
 * the result parked in a fixed memory cell so the kernel can verify
 * the roundtrip happened. The cell is the user stack's first word. */
void wubu_user_selftest(void)
{
    uint64_t cell;
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(cell));
    /* the cell travels in RBX (callee-saved, not clobbered by syscall);
     * syscall 0 = get_uptime; the result is parked at the user cell. */
    __asm__ __volatile__(
        "mov $0, %%rax\n"
        "syscall\n"
        "mov %%rax, (%%rbx)\n"
        :
        : "b"(cell), "D"(0)
        : "rax", "memory");
    for (;;) __asm__ __volatile__("pause");
}
