/*
 * start.S -- WuBuOS EFI payload entry.
 *
 * The firmware enters at the PE AddressOfEntryPoint using the Microsoft x64
 * ABI: RCX = ImageHandle, RDX = SystemTable, caller-provided shadow space.
 * Those two registers must survive BSS clearing, so they are parked in
 * callee-saved r12/r13 first.
 */

    .section .text.startup, "ax"
    .code64
    .global _start
_start:
    movq    %rcx, %r12            /* ImageHandle */
    movq    %rdx, %r13            /* SystemTable */

    andq    $-16, %rsp
    subq    $40, %rsp             /* 32B shadow space + 8 for alignment */

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

    addq    $40, %rsp
3:  cli
    hlt
    jmp     3b
