/*
 * crt0.S  --  WuBuOS Bare-Metal Startup (Limine Protocol)
 * GAS/AT&T syntax for GNU Assembler
 * 
 * Entry point for Limine bootloader. Sets up:
 *   - Long mode (if booted in 32-bit)
 *   - Initial page tables (identity + higher half)
 *   - Stack
 *   - Passes Limine boot info to kernel_main
 */

.code32
.section .multiboot_header

/* Limine boot protocol header */
/* Request ID: Framebuffer (0xc7b1dd30dfc30df4c8b88, 0x6e1b0a4b5b8d7c0a) */
/* Request ID: Memmap    (0x67cf3d9d378a806f, 0xc323c3c6f73f3e2a) */

.align 8
limine_magic:
    .long 0xf26b9c2b          /* Limine magic */
    .long 0x00000000          /* Revision */
    .long limine_requests_start - limine_magic  /* Checksum */

limine_requests_start:
    .quad limine_fb_req       /* Framebuffer request */
    .quad limine_memmap_req   /* Memmap request */
    .quad limine_terminal_req /* Terminal request */
    .quad limine_smp_req      /* SMP request */
    .quad 0                   /* End marker */

limine_terminal_req:
    .quad 0xc8ac59310c2b0844
    .quad 0x13d2b5a15d21a53b
    .quad 0

limine_smp_req:
    .quad 0x95a67b819a1b857e
    .quad 0xa0b61b723b6a73e0
    .quad 0
    .quad 0

limine_fb_req:
    .quad 0xc7b1dd30df4c8b88
    .quad 0x6e1b0a4b5b8d7c0a
    .quad 0

limine_memmap_req:
    .quad 0x67cf3d9d378a806f
    .quad 0xc323c3c6f73f3e2a
    .quad 0

.section .text
.global _start
.extern kernel_main
.extern _kernel_start
.extern _kernel_end
.extern _bss_start
.extern _bss_end
.extern _stack_top

_start:
    /* ------------------------------------------------------------
     * We're in 32-bit protected mode (Limine puts us here)
     * Switch to long mode
     * ------------------------------------------------------------ */

    cli                     /* Disable interrupts */

    /* Check CPUID for long mode support */
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb .no_long_mode

    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx  /* LM bit */
    jz .no_long_mode

    /* Check for 1GB pages (PDPE) */
    testl $(1 << 26), %edx
    jz .no_1gb_pages

    /* ------------------------------------------------------------
     * Set up page tables (4-level: PML4 -> PDP -> PD -> PT)
     * Identity map first 4GB, higher half kernel at 0xffffffff80000000
     * ------------------------------------------------------------ */

    /* Allocate page tables at _kernel_end (aligned to 4KB) */
    movl $_kernel_end, %edi
    addl $0xfff, %edi
    andl $~0xfff, %edi
    movl %edi, page_table_base

    /* PML4 at page_table_base */
    /* PDP  at page_table_base + 4KB */
    /* PD   at page_table_base + 8KB */
    /* PT   at page_table_base + 12KB */

    movl %edi, %eax
    movl %eax, page_table_base
    addl $0x1000, %eax
    movl %eax, pdp_base
    addl $0x1000, %eax
    movl %eax, pd_base
    addl $0x1000, %eax
    movl %eax, pt_base

    /* Zero page tables (4 pages = 16KB) */
    movl $4096, %ecx
    xorl %eax, %eax
    movl page_table_base, %edi
    rep stosl

    /* PML4[0] -> PDP (identity map, first 4GB) */
    movl pdp_base, %eax
    orl $0x3, %eax
    movl page_table_base, %edi
    movl %eax, (%edi)

    /* PML4[511] -> PDP (higher half kernel) */
    movl pdp_base, %eax
    orl $0x3, %eax
    leal 4088(%edi), %edx
    movl %eax, (%edx)

    /* PDP[0] -> PD (first 1GB) */
    movl pd_base, %eax
    orl $0x3, %eax
    movl pdp_base, %edi
    movl %eax, (%edi)

    /* PDP[511] -> PD (last 1GB for higher half) */
    movl pd_base, %eax
    orl $0x3, %eax
    leal 4088(%edi), %edx
    movl %eax, (%edx)

    /* PD[0..511] -> 2MB pages (identity map 1GB) */
    movl pd_base, %edi
    movl $0x83, %eax
    movl $512, %ecx
.pd_loop:
    movl %eax, (%edi)
    addl $0x200000, %eax
    addl $8, %edi
    loop .pd_loop

    /* PD[511] entries for higher half (map kernel at 0xffffffff80000000) */
    movl _kernel_start, %eax
    andl $~0x1fffff, %eax
    orl $0x83, %eax
    movl pd_base, %edi
    addl $(511*8), %edi
    movl %eax, (%edi)

    /* ------------------------------------------------------------
     * Enable PAE, PGE, NXE
     * ------------------------------------------------------------ */
    movl %cr4, %eax
    orl $(1 << 5) | (1 << 7), %eax
    movl %eax, %cr4

    /* Set NX bit in EFER */
    movl $0xC0000080, %ecx
    rdmsr
    orl $(1 << 11), %eax
    wrmsr

    /* Load CR3 (PML4) */
    movl page_table_base, %eax
    movl %eax, %cr3

    /* Enable Long Mode + Paging */
    movl $0xC0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr

    movl %cr0, %eax
    orl $(1 << 31), %eax
    movl %eax, %cr0

    /* ------------------------------------------------------------
     * Jump to 64-bit code
     * ------------------------------------------------------------ */
    lgdt gdt64_ptr
    ljmp $0x08, $.long_mode_start

.no_long_mode:
    hlt
    jmp .no_long_mode

.no_1gb_pages:
    hlt
    jmp .no_1gb_pages

.code64
.long_mode_start:
    /* ------------------------------------------------------------
     * In long mode!
     * Set up 64-bit segments
     * ------------------------------------------------------------ */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    /* Set up stack */
    movq _stack_top, %rsp

    /* Zero BSS (kernel_main does this too, but do early) */

    /* ------------------------------------------------------------
     * Parse Limine boot info from registers
     * Limine passes:
     *   RSI = pointer to boot info
     * ------------------------------------------------------------ */

    /* Save boot info pointer */
    movq %rsi, limine_boot_info

    /* ------------------------------------------------------------
     * Call kernel_main
     * ------------------------------------------------------------ */
    call kernel_main

    /* Should never return */
    cli
    hlt
    jmp .

.section .rodata
gdt64:
    .quad 0                                  /* Null descriptor */
    .quad 0x00af9b000000ffff                 /* Kernel code */
    .quad 0x00cf93000000ffff                 /* Kernel data */

gdt64_ptr:
    .word gdt64_ptr - gdt64 - 1
    .quad gdt64

.section .bss
.align 4096
page_table_base:  .skip 8
pdp_base:         .skip 8
pd_base:          .skip 8
pt_base:          .skip 8
limine_boot_info: .skip 8

/* Stack (16KB) */
stack_bottom:
    .skip 16384
stack_top: