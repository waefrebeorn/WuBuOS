/*
 * teardown.S -- WuBuOS WuBuFW chainloader: 64-bit EFI -> 32-bit protected
 * mode handoff into the kernel's crt0.S.
 *
 * After ExitBootServices we are still in long mode with the firmware's
 * identity page tables. crt0.S (_start) expects plain 32-bit protected mode
 * with paging OFF; it then builds its own tables and long-mode jump. So:
 *
 *   1. load a flat 32-bit GDT,
 *   2. far-return into a .code32 stub (compatibility mode; EFI identity
 *      map keeps VA == physical),
 *   3. clear CR0.PG then CR4.PAE (standard long-mode exit),
 *   4. jump to the kernel's physical entry (passed in RDI).
 *
 * The loader is flat-linked at a fixed base and identity-mapped, so the
 * link-time absolute addresses below are also the physical addresses.
 */
    .section .text
    .code64
    .global wubufw_handoff
wubufw_handoff:
    cli
    movq    %rdi, %rbx            /* kernel physical entry (32-bit) */

    /* Flat GDT with a 32-bit code segment (selector 0x08). */
    leaq    loader_gdt_ptr(%rip), %rax
    lgdt    (%rax)

    /* Far return into compatibility mode: push CS, push offset. */
    pushq   $0x08
    leaq    teardown32(%rip), %rax
    pushq   %rax
    lretq

    .code32
teardown32:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    %ax, %fs
    movw    %ax, %gs

    /* Leave paging: CR0.PG = 0 first, then CR4.PAE = 0. */
    movl    %cr0, %eax
    andl    $0x7FFFFFFF, %eax
    movl    %eax, %cr0

    movl    %cr4, %eax
    andl    $0xFFFFFFDF, %eax     /* clear PAE (bit 5) */
    movl    %eax, %cr4

    /* Jump to the kernel's 32-bit entry (crt0.S _start, physical). */
    movl    %ebx, %eax
    jmp     *%eax

    .section .data
    .align 8
loader_gdt:
    .quad   0x0000000000000000    /* null */
    .quad   0x00CF9A000000FFFF    /* 0x08: 32-bit code, base 0, limit 4G */
    .quad   0x00CF92000000FFFF    /* 0x10: 32-bit data, base 0, limit 4G */
    .quad   0x00AF9B000000FFFF    /* 0x18: 64-bit code (unused) */
loader_gdt_end:
    .align 8
loader_gdt_ptr:
    .word   loader_gdt_end - loader_gdt - 1
    .quad   loader_gdt
