/*
 * tasking_switch.S — x86_64 Assembly Context Switch
 *
 * Saves all callee-saved registers from old context,
 * restores all callee-saved registers to new context.
 * Per System V AMD64 ABI: RBX, RSP, RBP, R12-R15 are callee-saved.
 * Also saves RIP (via return address) and RFLAGS.
 *
 * Called as: void task_switch_asm(TaskContext *old_ctx, TaskContext *new_ctx);
 *   RDI = old_ctx, RSI = new_ctx
 *
 * TaskContext layout (matches tasking.h):
 *   0:   r15
 *   8:   r14
 *   16:  r13
 *   24:  r12
 *   32:  r11
 *   40:  r10
 *   48:  r9
 *   56:  r8
 *   64:  rdi
 *   72:  rsi
 *   80:  rbp
 *   88:  rdx
 *   96:  rcx
 *   104: rbx
 *   112: rax
 *   120: rip
 *   128: rsp
 *   136: rflags
 *
 * Total: 18 * 8 = 144 bytes
 */

.text
.global task_switch_asm
.hidden task_switch_asm

task_switch_asm:
    /* ── Save old context (RDI) ─────────────────────────────────── */
    /* We need to save registers before we clobber RDI/RSI */

    /* Save callee-saved registers first */
    movq %r15,  0(%rdi)
    movq %r14,  8(%rdi)
    movq %r13, 16(%rdi)
    movq %r12, 24(%rdi)
    movq %r11, 32(%rdi)
    movq %r10, 40(%rdi)
    movq %r9,  48(%rdi)
    movq %r8,  56(%rdi)

    /* Save caller-saved that we want to preserve */
    movq %rdi, 64(%rdi)   /* RDI (old_ctx pointer) - will be overwritten */
    movq %rsi, 72(%rdi)   /* RSI (new_ctx pointer) */
    movq %rbp, 80(%rdi)   /* RBP */
    movq %rdx, 88(%rdi)   /* RDX */
    movq %rcx, 96(%rdi)   /* RCX */
    movq %rbx, 104(%rdi)  /* RBX */
    movq %rax, 112(%rdi)  /* RAX */

    /* Save RSP - current stack pointer */
    leaq 8(%rsp), %rax    /* Skip return address on stack */
    movq %rax, 128(%rdi)

    /* Save RFLAGS */
    pushfq
    popq %rax
    movq %rax, 136(%rdi)

    /* Save RIP (return address) - it's at (%rsp) after pushfq/pop */
    movq (%rsp), %rax
    movq %rax, 120(%rdi)

    /* ── Restore new context (RSI) ──────────────────────────────── */
    /* RSI already has new_ctx pointer */

    /* Restore callee-saved */
    movq  0(%rsi), %r15
    movq  8(%rsi), %r14
    movq 16(%rsi), %r13
    movq 24(%rsi), %r12
    movq 32(%rsi), %r11
    movq 40(%rsi), %r10
    movq 48(%rsi), %r9
    movq 56(%rsi), %r8

    /* Restore caller-saved */
    movq 64(%rsi), %rdi
    movq 72(%rsi), %rsi
    movq 80(%rsi), %rbp
    movq 88(%rsi), %rdx
    movq 96(%rsi), %rcx
    movq 104(%rsi), %rbx
    movq 112(%rsi), %rax

    /* Restore RFLAGS */
    movq 136(%rsi), %rax
    pushq %rax
    popfq

    /* Restore RSP - new stack pointer */
    movq 128(%rsi), %rsp

    /* Restore RIP - jump to new instruction pointer */
    movq 120(%rsi), %rax
    jmp *%rax

/* End of task_switch_asm */

.section .note.GNU-stack,"",@progbits