/*
 * vsl_syscall.h  --  VSL Syscall Bridge API
 * Opaque struct pattern - only public API exposed
 */

#ifndef WUBUOS_VSL_SYSCALL_H
#define WUBUOS_VSL_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

/* Syscall function signature: 6 register arguments (x86-64 System V ABI) */
typedef int64_t (*vsl_syscall_fn)(uint64_t, uint64_t, uint64_t,
                                   uint64_t, uint64_t, uint64_t);

/* Handle a VSL syscall.
 * rax = syscall number, rdi-r9 = arguments
 * Returns result in rax (negative = -errno). */
int64_t vsl_syscall(uint64_t num, uint64_t rdi, uint64_t rsi,
                    uint64_t rdx, uint64_t r10, uint64_t r8, uint64_t r9);

/* Dispatch syscall by number.
 * This is the main syscall handler called from VSL interrupt context. */
int64_t vsl_syscall_dispatch(uint64_t num, uint64_t *regs);

/* Get syscall statistics */
void vsl_get_syscall_stats(uint64_t *out_count, uint64_t *out_errors);

#endif /* WUBUOS_VSL_SYSCALL_H */