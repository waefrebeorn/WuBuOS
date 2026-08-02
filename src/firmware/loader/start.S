/*
 * start.S -- WuBuOS WuBuFW chainloader entry.
 *
 * The firmware enters at the PE AddressOfEntryPoint using the Microsoft x64
 * ABI: RCX = ImageHandle, RDX = SystemTable, caller-provided shadow space.
 * Park the handoff registers in callee-saved r12/r13, set a dedicated early
 * stack BELOW the kernel copy (0x8F000..0x7F000, clear of the firmware page
 * tables at 0x70000 and the loader handoff at 0x90040+), zero BSS, call
 * efi_main.
 */
    .section .text.startup, "ax"
    .code64
    .global _start
_start:
    movq    %rcx, %r12            /* ImageHandle */
    movq    %rdx, %r13            /* SystemTable */

    /* Own stack: 64KB at 0x8F000, well below the kernel (1MB) and the
     * handoff (0x90040), above the firmware page tables (0x70000). */
    movq    $0x8F000, %rsp
    andq    $-16, %rsp

    /* zero BSS */
    leaq    _bss_start(%rip), %rdi
    leaq    _bss_end(%rip), %rcx
    subq    %rdi, %rcx
    xorl    %eax, %eax
1:  testq   %rcx, %rcx
    jz      2f
    movb    %al, (%rdi)
    incq    %rdi
    decq    %rcx
    jmp     1b
2:
    movq    %r12, %rcx
    movq    %r13, %rdx
    call    efi_main

3:  cli
    hlt
    jmp     3b
