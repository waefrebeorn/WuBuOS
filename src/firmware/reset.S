/*
 * reset.S  --  WuBuFW reset vector -> long mode.
 *
 * QEMU maps the -bios image so that it ends at the 4GB boundary. With a
 * 256KB image the flash base is 0xFFFC0000 and the CPU starts executing in
 * 16-bit real mode at linear 0xFFFFFFF0 (CS=F000 with hidden base
 * 0xFFFF0000). Only the last 64KB of the image is reachable through that
 * segment base, so the whole 16-bit stub lives at 0xFFFF0000.
 *
 * Sequence:
 *   16-bit real -> flat 32-bit protected (GDT in flash)
 *   -> copy firmware body from flash LMA to RAM VMA (0x200000)
 *   -> build 4-level page tables, identity map 4GB with 2MB pages
 *   -> enable PAE + LME + PG, far jump to 64-bit, call fw_main()
 *
 * GAS/AT&T. No relocations that the ROM cannot satisfy: every symbol used
 * before long mode is an absolute link-time constant below 4GB.
 */

    .set FLASH_BASE,  0xFFFC0000
    .set STUB_BASE,   0xFFFF0000
    .set RAM_BASE,    0x00200000
    .set PT_BASE,     0x00070000
    .set STACK_TOP,   0x0009F000

/* PML4 @ PT_BASE, PDPT @ +0x1000, PD0..PD3 @ +0x2000..+0x5000 */
    .set PML4,        PT_BASE
    .set PDPT,        PT_BASE + 0x1000
    .set PD0,         PT_BASE + 0x2000

/* ---------------------------------------------------------------- */
    .section .reset16, "ax"
    .code16
    .global _reset_stub
_reset_stub:
    cli
    cld
    /* Load a flat GDT while still in real mode. */
    lgdtl   %cs:(gdt32_desc - STUB_BASE)
    movl    %cr0, %eax
    orl     $1, %eax
    movl    %eax, %cr0
    /* Far jump into 32-bit flat protected mode. */
    ljmpl   $0x08, $prot32

    .code32
prot32:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    %ax, %fs
    movw    %ax, %gs
    movl    $STACK_TOP, %esp

    /* --- copy firmware body: flash LMA -> RAM VMA ---------------- */
    movl    $_body_lma, %esi
    movl    $_body_vma, %edi
    movl    $_body_size, %ecx
    addl    $3, %ecx
    shrl    $2, %ecx
    rep movsl

    /* --- zero BSS ------------------------------------------------ */
    movl    $_bss_start, %edi
    movl    $_bss_end, %ecx
    subl    %edi, %ecx
    addl    $3, %ecx
    shrl    $2, %ecx
    xorl    %eax, %eax
    rep stosl

    /* --- zero page-table area (6 pages) -------------------------- */
    movl    $PT_BASE, %edi
    movl    $(6 * 4096 / 4), %ecx
    xorl    %eax, %eax
    rep stosl

    /* PML4[0] -> PDPT */
    movl    $PDPT, %eax
    orl     $0x03, %eax            /* present | rw */
    movl    %eax, PML4

    /* PDPT[0..3] -> PD0..PD3 */
    movl    $PD0, %eax
    orl     $0x03, %eax
    movl    $PDPT, %edi
    movl    $4, %ecx
1:  movl    %eax, (%edi)
    movl    $0, 4(%edi)
    addl    $0x1000, %eax
    addl    $8, %edi
    loop    1b

    /* PD entries: 2048 * 2MB = 4GB identity, PS=1 */
    movl    $PD0, %edi
    xorl    %eax, %eax             /* phys low  */
    xorl    %edx, %edx             /* phys high */
    movl    $2048, %ecx
2:  movl    %eax, %ebx
    orl     $0x83, %ebx            /* present | rw | PS */
    movl    %ebx, (%edi)
    movl    %edx, 4(%edi)
    addl    $0x200000, %eax
    adcl    $0, %edx
    addl    $8, %edi
    loop    2b

    /* --- enable PAE ---------------------------------------------- */
    movl    %cr4, %eax
    orl     $(1 << 5), %eax
    movl    %eax, %cr4

    movl    $PT_BASE, %eax
    movl    %eax, %cr3

    /* --- EFER.LME + NXE ------------------------------------------ */
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $((1 << 8) | (1 << 11)), %eax
    wrmsr

    /* --- paging on ------------------------------------------------ */
    movl    %cr0, %eax
    orl     $0x80010000, %eax      /* PG | WP */
    movl    %eax, %cr0

    ljmpl   $0x18, $long64

    .code64
long64:
    movw    $0x20, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    %ax, %fs
    movw    %ax, %gs
    movq    $STACK_TOP, %rsp
    xorq    %rbp, %rbp

    /* Enable SSE: firmware C code is compiled with -mno-sse, but the PE
     * images we launch (and memcpy intrinsics) expect a sane FPU state. */
    movq    %cr0, %rax
    andq    $~(1 << 2), %rax       /* clear EM */
    orq     $(1 << 1), %rax        /* set MP   */
    movq    %rax, %cr0
    movq    %cr4, %rax
    orq     $((1 << 9) | (1 << 10)), %rax  /* OSFXSR | OSXMMEXCPT */
    movq    %rax, %cr4
    fninit

    /* fw_main lives at 0x200000, far outside a rel32 call from flash. */
    movabsq $fw_main, %rax
    call    *%rax
3:  cli
    hlt
    jmp     3b

/* ---------------------------------------------------------------- */
    .align 16
gdt32:
    .quad   0x0000000000000000     /* 0x00 null            */
    .quad   0x00CF9A000000FFFF     /* 0x08 code32          */
    .quad   0x00CF92000000FFFF     /* 0x10 data32          */
    .quad   0x00AF9A000000FFFF     /* 0x18 code64 (L=1)    */
    .quad   0x00CF92000000FFFF     /* 0x20 data64          */
gdt32_end:

    .align 4
gdt32_desc:
    .word   gdt32_end - gdt32 - 1
    .long   gdt32

/* ---------------------------------------------------------------- */
/* CPU reset vector: the last 16 bytes of the flash image.           */
    .section .resetvec, "ax"
    .code16
    .global _reset_vector
_reset_vector:
    jmp     _reset_stub
    .byte   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    .byte   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
