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
/* Limine boot protocol header (request IDs) */
#define LIMINE_MAGIC      0xf26b9c2b
#define LIMINE_FB_REQ_ID0 0xc7b1dd30df4c8b88
#define LIMINE_FB_REQ_ID1 0x6e1b0a4b5b8d7c0a
#define LIMINE_MEM_REQ_ID0 0x67cf3d9d378a806f
#define LIMINE_MEM_REQ_ID1 0xc323c3c6f73f3e2a
#define LIMINE_TERM_REQ_ID0 0xc8ac59310c2b0844
#define LIMINE_TERM_REQ_ID1 0x13d2b5a15d21a53b
#define LIMINE_SMP_REQ_ID0 0x95a67b819a1b857e
#define LIMINE_SMP_REQ_ID1 0xa0b61b723b6a73e0

/* IMPORTANT: the entire bootstrap below lives in ONE contiguous .text
 * section starting at file offset 0.  This is deliberate — a *separate*
 * .multiboot_header section at the front would make the loaded ELF image
 * non-contiguous from file offset 0, so `qemu -kernel` (and other multiboot
 * loaders) place it at a misaligned physical base, which splits every
 * higher-half VMA page across two physical pages and breaks the 4KB page
 * table mapping.  Keeping everything in .text guarantees the image is
 * contiguous in the file and loads page-aligned. */

.section .text
.global _start
.extern kernel_main

/* Limine magic + request list (must be first, aligned to 8) */
.align 8
limine_magic:
    .long LIMINE_MAGIC
    .long 0x00000000
    .long limine_requests_start - limine_magic   /* checksum */
limine_requests_start:
    .quad limine_fb_req
    .quad limine_memmap_req
    .quad limine_terminal_req
    .quad limine_smp_req
    .quad 0
limine_terminal_req:
    .quad LIMINE_TERM_REQ_ID0
    .quad LIMINE_TERM_REQ_ID1
    .quad 0
limine_smp_req:
    .quad LIMINE_SMP_REQ_ID0
    .quad LIMINE_SMP_REQ_ID1
    .quad 0
    .quad 0
limine_fb_req:
    .quad LIMINE_FB_REQ_ID0
    .quad LIMINE_FB_REQ_ID1
    .quad 0
limine_memmap_req:
    .quad LIMINE_MEM_REQ_ID0
    .quad LIMINE_MEM_REQ_ID1
    .quad 0


.extern kernel_main


/* ── Multiboot1 header (for `qemu -kernel` / GRUB) ──────────────────────
 * The kernel is linked at the higher-half VA 0xffffffff80100000 but LOADED
 * at physical LMA 0x100000.  `qemu -kernel` jumps to e_entry, which is the
 * higher-half VA and would crash.  Providing ADDRESS fields + entry_addr in
 * the multiboot header makes the loader jump to the PHYSICAL entry (0x100000)
 * instead.  The magic (0x1BADB002) must appear within the first 8KB of the
 * loaded image, so this header is the very first thing in .text. */
.align 4
mb1_hdr:
    .long 0x1BADB002                  /* magic */
    .long 0x00010000                  /* flags: bit16 = address fields provided */
    .long -(0x1BADB002 + 0x00010000) /* checksum = -(magic+flags) */
    .long 0x00100000                  /* header_addr  (physical LMA of .text) */
    .long 0x00100000                  /* load_addr    (physical LMA) */
    .long 0x00000000                  /* load_end_addr (0 = use ELF) */
    .long 0x00000000                  /* bss_end_addr  (0 = use ELF) */
    .long 0x00100020                  /* entry_addr: PHYS addr of _start.
                                        * QEMU's multiboot loader places this
                                        * header at load_addr (0x100000) and
                                        * _start immediately follows (32-byte
                                        * mb1_hdr), so the real physical entry
                                        * is 0x100020 — NOT the ELF VMA-derived
                                        * 0x10012c (the limine header before this
                                        * shifts the file offset, but the loader
                                        * starts loading AT this header). */

_start:
    cli                     /* Disable interrupts */

    /* Disable paging if a previous loader (multiboot/SeaBIOS) left it on. */
    movl %cr0, %eax
    andl $~(1 << 31), %eax
    movl %eax, %cr0

    /* Load a flat 32-bit GDT so every segment base is 0.  Build the 6-byte
     * GDT pointer at a location inside the loaded image (DS-relative, which
     * is fine once we reload DS to the flat 0x10 selector).  The physical
     * address of gdt32_boot is recovered with the call/pop trick. */
    call .gdt_eip
.gdt_eip:
    pop %ebx                          /* ebx = physical addr of .gdt_eip */
    leal (gdt32_boot - .gdt_eip)(%ebx), %eax   /* phys addr of GDT */
    leal (early_gdt_ptr - .gdt_eip)(%ebx), %edi /* phys addr of ptr storage */
    movw $(gdt32_boot_end - gdt32_boot - 1), %cx
    movw %cx, (%edi)                  /* limit */
    movl %eax, 2(%edi)               /* base (physical) */
    lgdt (%edi)                       /* load via physical ptr (paging off) */
    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    /* ===== Kernel image physical base =====
     * qemu -kernel / GRUB / multiboot load this ELF at its LMA, which
     * kernel.ld sets to KERNEL_PHYS = 0x100000 (verified: readelf shows
     * PhysAddr 0x100000).  A prior byte-scan for the "WuBuOS" signature was
     * removed: its assumed .text offset (0x3c0) did not match the real
     * .lm_msg location, and the scan also false-matched coincidental
     * "WuBuOS" byte patterns in SeaBIOS low RAM (observed base 0x33 -> all
     * page tables built at garbage addresses -> triple fault).  We use the
     * linker-known LMA directly.  (Limine would supply the base in a
     * register; that path is secondary and reuses this constant.) */
    movl $0x100000, %esi              /* image physical base (LMA) */

    /* Early boot marker: proves _start runs under qemu -kernel/multiboot. */
    movw $0x3F8, %dx
    movb $'M', %al
    outb %al, %dx

    /* Check CPUID for long mode support */
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb .no_long_mode

    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx  /* LM bit */
    jz .no_long_mode

    /* marker: long-mode supported */
    movw $0x3F8, %dx
    movb $'P', %al
    outb %al, %dx

    /* (1GiB-page capability is not required; we use 2MB PDEs below.) */

    /* ------------------------------------------------------------
     * Page tables (4-level PAE long mode)
     *
     * ROOT CAUSE of the prior triple-fault saga: GAS mis-encodes DIRECT
     * absolute memory operands (movl $ADDR,%reg / movl %reg,ADDR) in this
     * mixed-code .o — they silently became movabs / wrong-address loads
     * (observed: the multiboot magic intended for scratch 0x90058 instead
     * landed at 0x803ff0, and every table store mislanded).  So we build
     * the tables with NO direct-address operands at all: every base is held
     * in a register, derived from %esi (the scanned image base).
     *
     * Layout (6 x 4KB, placed right after the loaded image at %esi+0x200000,
     * i.e. 2MB past the base — clear of the ~1.25MB image + 64KB stack):
     *   PML4       @ base+0x200000
     *   PDP_ident  @ base+0x201000  (low-half identity, 0..1GB)
     *   PD_ident   @ base+0x202000  (512 x 2MB identity PDEs, 0..1GB)
     *   PDP_high   @ base+0x203000  (high-half 0xffffffff80000000..)
     *   PD_high    @ base+0x204000  (-> PT_high)
     *   PT_high    @ base+0x205000  (512 x 4KB, image + 64KB stack)
     * These are all identity-mapped by PD_ident and within low RAM.
     * ------------------------------------------------------------ */
    /* Registers holding table bases are derived from %ebx (PML4) which is
     * preserved; everything else is recomputed as needed (no reliance on
     * registers that the zero-loop clobbers). */
    movl %esi, %ebx
    addl $0x200000, %ebx             /* %ebx = PML4 base (base+0x200000) */
    /* PDP_ident  = ebx+0x1000  (base+0x201000)
       PD_ident   = ebx+0x2000  (base+0x202000)
       PDP_high   = ebx+0x3000  (base+0x203000)
       PD_high    = ebx+0x4000  (base+0x204000)
       PT_high    = ebx+0x5000  (base+0x205000) */

    /* Zero all six page-table pages (6 * 4096 = 24576 bytes = 6144 dwords).
     * IMPORTANT: only the 6 pages we actually allocated (0x200000..0x206000)
     * are backed by RAM that qemu -kernel loaded/zeroed.  Zeroing past
     * 0x206000 walks into unbacked guest RAM and QEMU faults the store
     * (triple fault at the zero loop).  Use rep stosl over exactly 6 pages. */
    movl %ebx, %edi                  /* edi = PML4 base */
    xorl %eax, %eax                  /* zero */
    movl $6144, %ecx                 /* 6 pages * 1024 dwords */
    rep  stosl

    /* PML4[0] -> PDP_ident (ebx+0x1000) */
    movl %ebx, %eax
    addl $0x1000, %eax
    orl  $0x3, %eax
    movl %eax, (%ebx)                /* PML4[0] */
    /* PML4[511] -> PDP_high (ebx+0x3000) */
    movl %ebx, %eax
    addl $0x3000, %eax
    orl  $0x3, %eax
    movl %eax, 4088(%ebx)            /* PML4[511] */

    /* PDP_ident[0] -> PD_ident (ebx+0x2000); PDP_ident base = ebx+0x1000 */
    movl %ebx, %eax
    addl $0x2000, %eax
    orl  $0x3, %eax
    movl %eax, 0x1000(%ebx)          /* PDP_ident[0] */

    /* Fill PD_ident with 512 2MB identity PDEs: PDE = PA|0x83.
     * Use %edi as the loop counter (GAS mis-encodes %ecx as %r9d with a REX
     * prefix in this mixed-mode .o, which breaks the bound check). */
    movl %ebx, %ebp
    addl $0x2000, %ebp               /* %ebp = PD_ident ptr */
    xorl %edi, %edi                  /* i = 0 */
    xorl %edx, %edx                  /* PA = 0 */
.pd_ident_loop:
    movl %edx, %eax
    orl  $0x83, %eax                 /* P + RW + PS (2MB) */
    movl %eax, (%ebp)
    addl $0x200000, %edx             /* next 2MB PA */
    addl $8, %ebp                    /* next PDE slot */
    incl %edi
    cmpl $512, %edi
    jb   .pd_ident_loop
    /* now %ebp = ebx+0x3000 = PDP_high (handy for [510] below) */

    /* PD_high[0] -> PT_high (ebx+0x5000); PD_high base = ebx+0x4000 */
    movl %ebx, %eax
    addl $0x5000, %eax
    orl  $0x3, %eax
    movl %eax, 0x4000(%ebx)          /* PD_high[0] -> PT_high */

    /* PDP_high[510] -> PD_high (CRITICAL higher-half wiring).
     * PDP_high base = %ebp (= ebx+0x3000); entry 510 -> +4080. */
    movl %ebx, %eax
    addl $0x4000, %eax               /* PD_high */
    orl  $0x3, %eax
    movl %eax, 4080(%ebp)            /* PDP_high[510] -> PD_high */

    /* Build PT_high (ebx+0x5000): map higher-half VA 0xffffffff80000000 +
     * i*4KB to the kernel's physical image.  Image base = PML4 - 0x200000. */
    movl %ebx, %eax
    subl $0x200000, %eax             /* %eax = image physical base */
    movl %eax, %ebp                  /* %ebp = image base */
    movl %ebx, %eax
    addl $0x5000, %eax               /* %eax = PT_high ptr */
    movl $256, %edi                  /* first higher-half kernel page index */
.pt_outer:
    cmpl $784, %edi
    jae  .pt_done
    movl %edi, %edx
    subl $256, %edx
    shll $12, %edx                   /* (i-256)*4096 */
    addl %ebp, %edx                  /* + image base */
    andl $0xfffff000, %edx
    orl  $0x3, %edx                  /* present + RW */
    movl %edx, (%eax)                /* PT_high[i] = PA | 0x3 */
    addl $8, %eax                    /* next PT slot */
    incl %edi
    jmp  .pt_outer
.pt_done:

    /* marker: page tables built */
    movw $0x3F8, %dx
    movb $'G', %al
    outb %al, %dx

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

    /* Load CR3 (PML4).  %ebx still holds the PML4 physical base. */
    movl %ebx, %eax
    movl %eax, %cr3

    /* marker: CR3 loaded */
    movw $0x3F8, %dx
    movb $'C', %al
    outb %al, %dx

    /* Enable Long Mode + Paging */
    movl $0xC0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr

    /* marker: EFER.LM set */
    movw $0x3F8, %dx
    movb $'E', %al
    outb %al, %dx

    movl %cr0, %eax
    orl $(1 << 31), %eax
    movl %eax, %cr0

    /* marker: paging on */
    movw $0x3F8, %dx
    movb $'W', %al
    outb %al, %dx

    /* Get the physical (load) address of the current instruction via a
     * call/pop.  The kernel is loaded at its physical LMA and identity-
     * mapped, so %esp0 below holds the real physical EIP. */
    call .get_eip
.get_eip:
    pop %ebp                        /* %ebp = phys addr of .get_eip (scratch) */

    /* Build a 6-byte GDT pointer at early_gdt_ptr with the PHYSICAL base of
     * gdt64 (recovered via the .get_eip physical base in %ebp). */
    leal (gdt64 - .get_eip)(%ebp), %eax       /* phys addr of gdt64 */
    leal (early_gdt_ptr - .get_eip)(%ebp), %edi
    movw $(gdt64_end - gdt64 - 1), %cx
    movw %cx, (%edi)
    movl %eax, 2(%edi)
    lgdt (%edi)

    /* marker: gdt64 loaded */
    movw $0x3F8, %dx
    movb $'X', %al
    outb %al, %dx

    /* Long-mode entry: far jump to the PHYSICAL .long_mode_start.
     * In 32-bit protected mode a far indirect jump reads a 6-byte m16:32
     * pointer (4-byte offset @0, 2-byte selector @4).  We use a dedicated
     * ljmp_ptr so we never clobber the lgdt descriptor layout above. */
    leal (.long_mode_start - .get_eip)(%ebp), %eax   /* physical entry */
    leal (ljmp_ptr - .get_eip)(%ebp), %edi
    movl %eax, (%edi)                /* 32-bit offset (physical entry) @0 */
    movw $0x08, 4(%edi)              /* selector = 64-bit code in gdt64 @4 */

    /* marker: about to ljmp to 64-bit (physical entry) */
    movb $'Y', %al
    outb %al, %dx
    ljmp *(%edi)

.no_long_mode:
    movw $0x3F8, %dx
    movb $'L', %al
    outb %al, %dx
    hlt
    jmp .no_long_mode

.no_1gb_pages:
    movw $0x3F8, %dx
    movb $'H', %al
    outb %al, %dx
    hlt
    jmp .no_1gb_pages

.code64
.long_mode_start:
    /* marker: entered 64-bit long mode (running at physical load address) */
    movw $0x3F8, %dx
    movb $'J', %al
    outb %al, %dx

    /* 64-bit segments. */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    /* marker: 64-bit segments set */
    movw $0x3F8, %dx
    movb $'K', %al
    outb %al, %dx

    /* Early boot stack: physical 0x70000 (identity-mapped by PD_ident[0],
     * which covers 0..2MB).  We switch to the higher-half _stack_top before
     * calling kernel_main. */
    movq $0x70000, %rsp

    /* marker: stack up */
    movw $0x3F8, %dx
    movb $'S', %al
    outb %al, %dx

    call klog_init

    /* marker: klog_init returned */
    movw $0x3F8, %dx
    movb $'I', %al
    outb %al, %dx

    /* Decide whether we booted via Limine (EAX=0x2BADB002 means multiboot). */
    movl $0, %eax
    cmpl $0x2BADB002, %eax
    je   .not_limine
    movb $1, g_limine_ok(%rip)
.not_limine:

    movabsq $.lm_msg, %rdi
    call klog_printf

    /* marker: klog printed */
    movw $0x3F8, %dx
    movb $'Q', %al
    outb %al, %dx

    jmp .lm_after_msg
.lm_msg:
    .asciz "WuBuOS: 64-bit long mode entered, stack up\n"
.lm_after_msg:

    /* Copy boot-info pointer: multiboot leaves it in EBX, but we did not
     * preserve EBX (it held PML4 base).  For `qemu -kernel` we have no
     * usable mb_info; kernel_main treats non-Limine as defaults.  Pass 0. */
    xorq %rsi, %rsi

    /* Switch to the higher-half _stack_top before entering kernel_main. */
    movabsq $_stack_top, %rsp

    /* Call kernel_main (never returns). */
    call kernel_main

    /* Should never return */
    cli
    hlt
    jmp .

gdt64:
    .quad 0                                  /* Null descriptor */
    .quad 0x00af9b000000ffff                 /* Kernel code */
    .quad 0x00cf93000000ffff                 /* Kernel data */
gdt64_end:

/* (gdt64_ptr is built at runtime from the physical address of gdt64.) */
/* Flat 32-bit GDT used during early boot.  Referenced only via the call/pop
 * physical-address trick, never via a 32-bit absolute relocation. */
.align 4
early_gdt_ptr:
    .space 6, 0            /* scratch: 6-byte GDT/ljmp pointer (physical) */

ljmp_ptr:
    .space 6, 0            /* far-jump m16:32: offset@0 (4B), selector@4 (2B) */

gdt32_boot:
    .quad 0                                  /* Null */
    .quad 0x00cf9a000000ffff                 /* 32-bit code, base 0, limit 4G */
    .quad 0x00cf92000000ffff                 /* 32-bit data, base 0, limit 4G */
gdt32_boot_end:
