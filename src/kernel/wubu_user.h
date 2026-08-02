/*
 * wubu_user.h  --  the ring-3 boundary (gap H4)
 *
 * Everything ran in ring 0. This module is the user/kernel split's
 * entry point: it builds a ring-3 task frame (user CS/SS/RSP/RFLAGS),
 * returns to user mode via iretq, and relies on the syscall path
 * (STAR/LSTAR + the C6-sanitized sysretq) for the way back in.
 * The GDT already carries the user segments (the syscall init wires
 * STAR with 0x23/0x2B); the TSS provides the kernel stack on the
 * interrupt entry.
 *
 * Freestanding C11.
 */
#ifndef WUBU_USER_H
#define WUBU_USER_H

#include <stdint.h>

/* User segment selectors (GDT indices 4/5, RPL 3). */
#define WUBU_USER_CS  0x23
#define WUBU_USER_SS  0x2B

/* Build a ring-3 frame and iretq into it. `entry` runs at ring 3 with
 * the user stack at `user_stack_top`; the syscall path returns to the
 * kernel on the next trap. Never returns (unless the entry returns,
 * which faults -- the user code must syscall or loop). */
void wubu_user_enter(uint64_t entry, uint64_t user_stack_top);

/* The ring-3 test payload: does a syscall roundtrip (syscall 0 =
 * get_uptime) and loops. Used by the H4 boundary test. */
void wubu_user_selftest(void);

#endif
