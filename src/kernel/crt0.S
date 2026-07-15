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
    .long 0x00100000                  /* entry_addr    (physical _start) */

_start:
.extern _kernel_start
.extern _kernel_end
.extern _kernel_phys_start
.extern _kernel_phys_end
.extern _kernel_phys_base
_start:
.extern _kernel_start
.extern _kernel_end
.extern _kernel_phys_start
.extern _kernel_phys_end
.extern _kernel_phys_base
.extern _physical_offset
.extern long_mode_jmp
.extern _bss_start
.extern _bss_end
.extern _stack_top

    cli                     /* Disable interrupts */

    /* Disable paging if a previous loader (multiboot/SeaBIOS) left it on.
     * We build our own page tables from scratch; writing to low-physical
     * scratch (0x90000) would #PF under an active mapping and triple-fault. */
    movl %cr0, %eax
    andl $~(1 << 31), %eax
    movl %eax, %cr0

    /* Load a flat 32-bit GDT so every segment base is 0.  Build the 6-byte
     * GDT pointer at a physical scratch location referenced via register
     * (not DS-based), since DS may still be the loader's non-flat selector.
     * The running CS is already a flat 32-bit segment, so no far jump is
     * needed here — only DS/ES/SS get reloaded to the flat data selector. */
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

    /* ===== Recover the REAL physical load base of the kernel image =====
     * The actual loader (qemu -kernel / multiboot / Limine) may place the ELF
     * at a physical base that differs from the linker's KERNEL_PHYS (ELF
     * header / section-alignment gaps), so no hardcoded VMA->LMA formula is
     * reliable.  Instead we SCAN physical RAM for the kernel's unique signature
     * string "WuBuOS" (crt0's .lm_msg, present exactly once at the lowest
     * address of all such strings).  Its physical address X recovers the LMA
     * base:
     *   LMA_base = X - (.lm_msg_VMA - 0xffffffff80100000) = X - 0x3c0
     *   (physical of VMA 0xffffffff80100000, i.e. the image base)
     * Paging is OFF here, so (%esi) reads physical RAM directly.  The result
     * lives in %esi (survives the cpuid below, which clobbers only
     * %eax/%ebx/%ecx/%edx) and the page-table builder reads it later.  This
     * is fully loader-agnostic.  Fallback to 0x100000 if the scan fails. */
    movl $0xE0000, %esi              /* scan start (physical) */
    movl $0x300000, %edi             /* scan end */
.scan_loop:
    cmpl %edi, %esi
    jae .scan_fail
    movl (%esi), %eax                /* first 4 bytes: "WuBu" little-endian
                                         *   mem = 57 75 42 75  ->  eax = 0x75427557 */
    cmpl $0x75427557, %eax
    jne .scan_next
    movzwl 4(%esi), %eax             /* next 2 bytes: "OS" little-endian */
    cmpw $0x534f, %ax
    jne .scan_next
    subl $0x3c0, %esi                 /* %esi = real LMA base */
    jmp .scan_done
.scan_next:
    addl $1, %esi
    jmp .scan_loop
.scan_fail:
    movl $0x100000, %esi              /* fallback (ideal higher-half base) */
.scan_done:

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

    /* Check for 1GB pages (PDPE). We don't strictly require them: the PD
     * is built with 2MB pages below, which cover the first 1GB identity
     * and the higher-half kernel (374KB fits in one 2MB page). Skip the
     * check rather than halt on CPUs without PDPE1GB. */
    testl $(1 << 26), %edx
    /* (intentionally not branching to .no_1gb_pages) */

    /* ------------------------------------------------------------
     * Set up page tables (4-level: PML4 -> PDP -> PD -> PT)
     * Identity map first 4GB, higher half kernel at 0xffffffff80000000
     * ------------------------------------------------------------ */

    /* Allocate page tables just past the PHYSICAL end of the loaded kernel.
     * Layout (6 contiguous pages): PML4, PDP_ident, PD_ident, PDP_high,
     * PD_high, PT_high.  Fixed physical base 0x123000 (kernel ends ~0x12203c
     * per `nm`, so 0x123000 is clear).
     *
     * IMPORTANT (GAS footgun): these early-boot scratch vars (page_table_base,
     * pdp_base, ...) are ABSOLUTE link-time constants (<4GB) defined in
     * kernel.ld.  A bare `movl sym, %reg` or `movl %reg, sym` against such an
     * absolute symbol is AMBIGUOUS in mixed 16/32/64 code and GAS mis-encodes
     * it (it emits a garbage 64-bit `movabs` instead of a 32-bit abs load),
     * silently corrupting every page-table store.  The correct, unambiguous
     * pattern is to load the symbol's ADDRESS with `movl $sym, %reg` then
     * dereference `(%reg)`.  We use that everywhere below. */
    movl $0x123000, %edi             /* fixed base, past kernel image */
    addl $0xfff, %edi
    andl $~0xfff, %edi
    movl $page_table_base, %edx
    movl %edi, (%edx)               /* scratch: PML4 base */
    addl $0x1000, %edi
    movl $pdp_base, %edx
    movl %edi, (%edx)               /* scratch: PDP_ident base */
    addl $0x1000, %edi
    movl $pd_base, %edx
    movl %edi, (%edx)               /* scratch: PD_ident base */
    addl $0x1000, %edi
    movl $pdp_high_base, %edx
    movl %edi, (%edx)               /* scratch: PDP_high base */
    addl $0x1000, %edi
    movl $pd_high_base, %edx
    movl %edi, (%edx)               /* scratch: PD_high base */

    /* Zero all 6 page-table pages: PML4, PDP_ident, PD_ident, PDP_high,
     * PD_high, PT_high (6 * 1024 dwords). */
    movl $6144, %ecx
    xorl %eax, %eax
    movl $page_table_base, %edx
    movl (%edx), %edi
    rep stosl

    /* PML4[0] -> PDP_ident (identity map, low 4GB). */
    movl $pdp_base, %edx
    movl (%edx), %eax
    orl $0x3, %eax
    movl $page_table_base, %edx
    movl (%edx), %edi
    movl %eax, (%edi)

    /* PML4[511] -> PDP_high (higher-half kernel). */
    movl $pdp_high_base, %edx
    movl (%edx), %eax
    orl $0x3, %eax
    movl $page_table_base, %edx
    movl (%edx), %edi
    leal 4088(%edi), %edx
    movl %eax, (%edx)

    /* PDP_ident[0] -> PD_ident (first 1GB identity). */
    movl $pd_base, %edx
    movl (%edx), %eax
    orl $0x3, %eax
    movl $pdp_base, %edx
    movl (%edx), %edi
    movl %eax, (%edi)

    /* PDP_high[510] -> PD_high.  This is the CRITICAL higher-half wiring.
     * For VAs in 0xffffffff80000000..0xffffffffBFFFFFFF, the 4-level index
     * is: PML4[511] (bits 47:39) -> PDP index (bits 38:30).  Bits 38:30 of
     * 0xffffffff80000000 are 0b111111110 = 510 (NOT 0!).  So the higher-half
     * PDP entry MUST be at PDP_high[510]; writing PDP_high[0] leaves every
     * VMA access unmapped and triple-faults on the first C data reference. */
    movl $pd_high_base, %edx
    movl (%edx), %eax
    orl $0x3, %eax
    movl $pdp_high_base, %edx
    movl (%edx), %edi
    leal 4080(%edi), %edx          /* index 510 * 8 = 4080 */
    movl %eax, (%edx)              /* PDP_high[510] -> PD_high */

    /* PD_ident[0..511] -> 2MB identity pages (covers 0..1GB, including the
     * physical kernel + early code that still runs at its load address). */
    movl $pd_base, %edx
    movl (%edx), %edi
    movl $0x83, %eax
    movl $512, %ecx
.pd_loop:
    movl %eax, (%edi)
    addl $0x200000, %eax
    addl $8, %edi
    loop .pd_loop

    /* PD_high[0] -> PT_high (FINE-GRAINED 4KB page table, NOT a 2MB page).
     * The linker maps LMA = VMA - 0xffffffff80000000 (VMA base
     * 0xffffffff80100000, LMA base 0x100000), so PT_high[i] maps
     *   VA (0xffffffff80000000 + i*4096)  ->  PA (i*4096)
     * i.e. the page's VMA-minus-VMA_base equals its physical load address.
     * So e.g. VA 0xffffffff80100000 (i=256) -> PA 0x100000 = real .text LMA.
     * VMA accesses in C (string literals, globals, _stack_top) resolve to the
     * real loaded image.  512 entries cover VA 0xffffffff80000000..0x801FFFFF
     * -> PA 0x0..0x200000, which includes the whole kernel image (LMA ends
     * ~0x12203c) and the early kernel stack (VMA offset ~0x3203c). */
    movl $0x128000, %eax           /* PT_high physical base */
    orl $0x3, %eax
    movl $pd_high_base, %edx
    movl (%edx), %edi
    movl %eax, (%edi)              /* PD_high[0] -> PT_high */

    /* Build PT_high by walking the loaded ELF SECTION HEADERS.
     * The loader placed the image contiguously in RAM from elf_base, but the
     * VMA->physical mapping is NON-LINEAR (ELF header + per-section alignment
     * gaps).  The only correct, loader-agnostic way to map a higher-half VMA
     * v is, for the section that contains v:
     *     physical(v) = elf_base + sh_offset + (v - sh_addr)
     * which is exactly what a real bootloader does.  We scan for the ELF magic
     * to locate elf_base, then iterate the section headers.  ELF-64 field
     * offsets: e_shoff @0x28, e_shentsize @0x3a, e_shnum @0x3c; per section:
     * sh_type @0x00, sh_addr @0x10, sh_offset @0x18, sh_size @0x20 (all 8-byte
     * fields; we use their low 32 bits since the kernel lives in 0xffffffff8...). */

    /* scratch slots (low, identity-mapped physical) for the walk */
    .equ ELF_BASE_SCR, 0x90040
    .equ SHTAB_PA_SCR, 0x90044
    .equ SHENTSZ_SCR,  0x90048
    .equ SHNUM_SCR,    0x9004c
    .equ VL_LO_SCR,    0x90050
    .equ PAGE_I_SCR,   0x90054

    /* 1) locate elf_base: scan physical RAM for ELF magic 0x464c457f. */
    movl $0xE0000, %ebp
    movl $0x140000, %edx
.elf_scan:
    cmpl %edx, %ebp
    jae .elf_scan_fail
    movl (%ebp), %eax
    cmpl $0x464c457f, %eax
    je .elf_found
    addl $4, %ebp
    jmp .elf_scan
.elf_found:
    movl 0x28(%ebp), %ebx           /* e_shoff (file offset of shtab) */
    addl %ebp, %ebx                 /* %ebx = shtab physical address */
    movzwl 0x3a(%ebp), %edx         /* e_shentsize */
    movzwl 0x3c(%ebp), %ecx         /* e_shnum */
    movl %ebp, ELF_BASE_SCR
    movl %ebx, SHTAB_PA_SCR
    movl %edx, SHENTSZ_SCR
    movl %ecx, SHNUM_SCR
    movl $256, %eax                 /* first higher-half kernel page index */
.pt_outer:
    cmpl $384, %eax
    jae .pt_done
    movl %eax, PAGE_I_SCR
    /* v_lo = 0x80000000 + i*4096  (low 32 bits of this page's VMA) */
    movl $0x80000000, %ecx
    movl %eax, %edx
    shll $12, %edx
    addl %edx, %ecx
    movl %ecx, VL_LO_SCR
    xorl %ecx, %ecx                 /* section index j = 0 */
.sh_loop:
    movl SHNUM_SCR, %eax
    cmpl %eax, %ecx
    jae .sh_nomatch
    /* shdr_PA = SHTAB_PA + j*shentsize */
    movl SHTAB_PA_SCR, %eax
    movl %ecx, %edx
    imul SHENTSZ_SCR, %edx
    addl %edx, %eax                 /* %eax = shdr_PA */
    movl (%eax), %edx               /* sh_type */
    movl 0x10(%eax), %ebx           /* sh_addr low 32 */
    movl 0x20(%eax), %esi           /* sh_size low 32 */
    movl 0x18(%eax), %ebp           /* sh_offset low 32 */
    cmpl $8, %edx                   /* SHT_NOBITS (bss) -> zeroed page */
    je .sh_nomatch
    movl VL_LO_SCR, %eax
    subl %ebx, %eax                 /* eax = v_lo - sh_addr_lo */
    cmpl %esi, %eax                 /* v_lo >= sh_addr+sh_size -> next section */
    jae .sh_next
    /* in this PROGBITS section: physical = elf_base + sh_offset + (v_lo - sh_addr) */
    movl ELF_BASE_SCR, %edx
    addl %ebp, %edx                 /* + sh_offset */
    addl %eax, %edx                 /* + (v_lo - sh_addr_lo) */
    andl $0xfffff000, %edx          /* page-align */
    orl  $0x3, %edx                 /* present + RW */
    movl PAGE_I_SCR, %eax
    shll $3, %eax                   /* i*8 */
    movl $0x128000, %edi
    addl %eax, %edi
    movl %edx, (%edi)
    jmp .pt_nextpage
.sh_next:
    incl %ecx
    jmp .sh_loop
.sh_nomatch:
    /* bss / gap / stack: map to a zeroed, identity-mapped 4KB page. */
    movl PAGE_I_SCR, %eax
    shll $3, %eax
    movl $0x128000, %edi
    addl %eax, %edi
    movl $0x93003, (%edi)           /* 0x93000 | present+RW (QEMU RAM is zeroed) */
.pt_nextpage:
    movl PAGE_I_SCR, %eax
    incl %eax
    jmp .pt_outer
.pt_done:
    jmp .pt_finish
.elf_scan_fail:
    /* fallback: identity-map the higher-half region 1:1 (best effort) */
    movl $0x128000, %edi
    xorl %eax, %eax
    movl $512, %ecx
.fb_loop:
    movl %eax, %edx
    orl  $0x3, %edx
    movl %edx, (%edi)
    addl $0x1000, %eax
    addl $8, %edi
    loop .fb_loop
.pt_finish:

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

    /* Load CR3 (PML4) -- use the address-load pattern (see GAS footgun note). */
    movl $page_table_base, %edx
    movl (%edx), %eax
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

    /* ------------------------------------------------------------
     * Jump to 64-bit code (position-independent)
     * ------------------------------------------------------------ */
    /* Get the physical (load) address of the current instruction via a
     * call/pop. Because the kernel is loaded at its physical LMA (1MB) and
     * identity-mapped, %ebx below holds the real physical EIP. */
    call .get_eip
.get_eip:
    pop %ebx

    /* Build a 6-byte GDT pointer at the physical scratch (early_gdt_ptr)
     * with the PHYSICAL base of gdt64.  Storing the higher-half VMA of
     * gdt64 (as the static gdt64_ptr does) breaks once paging is on, since
     * the GDT must be referenced by its physical address. */
    leal (gdt64 - .get_eip)(%ebx), %eax       /* phys addr of gdt64 */
    leal (early_gdt_ptr - .get_eip)(%ebx), %edi
    movw $(gdt64_end - gdt64 - 1), %cx        /* limit = sizeof(gdt64)-1 */
    movw %cx, (%edi)
    movl %eax, 2(%edi)                        /* base = phys addr of gdt64 */
    lgdt (%edi)

    /* marker: gdt64 loaded */
    movw $0x3F8, %dx
    movb $'X', %al
    outb %al, %dx

    /* Long-mode entry: jump to the PHYSICAL .long_mode_start first (compat
     * ljmp, proven to enter 64-bit).  Once in 64-bit we switch to the
     * higher-half VMA via a separate `jmp rax` (reference pattern: alamot/
     * osdev).  The 32-bit compat ljmp offset is only 32-bit and sign-extends,
     * which makes a direct higher-half target fragile; doing it in 64-bit with
     * a full 64-bit address is reliable.  %ebx still holds the physical
     * .get_eip base (set after the call/pop, before any cpuid clobber), so we
     * can compute the higher-half VMA of a label precisely. */
    leal (.long_mode_start - .get_eip)(%ebx), %eax   /* physical entry */
    movl %eax, (%edi)          /* offset (physical entry) */
    movw $0x08, 4(%edi)       /* selector = 64-bit code in gdt64 */

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
    /* marker: entered 64-bit long mode (running at physical load address;
     * higher-half VMA accesses are served by the page tables at 0x123000). */
    movw $0x3F8, %dx
    movb $'J', %al
    outb %al, %dx
    /* ------------------------------------------------------------
     * In long mode, running at the higher-half VMA!
     * Set up 64-bit segments.
     * ------------------------------------------------------------ */
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
     * which covers 0..2MB).  We must NOT add the higher-half offset here --
     * that would place rsp at an unmapped VA (PML4[510], outside the
     * higher-half 2MB window we mapped), and the very next `call` would #PF
     * pushing its return address.  The kernel C code runs fine with a
     * physical stack because rel/near calls use offsets that cancel the
     * VMA-LMA difference; only absolute-VMA data references need the
     * higher-half PT (handled by PT_high above).  We switch to the real
     * higher-half _stack_top just before calling kernel_main. */
    movq $0x70000, %rsp

    /* marker: stack up */
    movw $0x3F8, %dx
    movb $'S', %al
    outb %al, %dx

    /* Boot milestone: we are in 64-bit long mode at the higher-half VMA with
     * a valid (higher-half) stack.  Initialize the serial log and print a
     * marker so QEMU (-serial) captures proof that paging + long-mode
     * transition succeeded.  All higher-half data refs now resolve. */
    call klog_init

    /* marker: klog_init returned */
    movw $0x3F8, %dx
    movb $'I', %al
    outb %al, %dx

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

    /* Zero BSS (kernel_main does this too, but do early) */

    /* ------------------------------------------------------------
     * Pass boot info to kernel_main.
     *   - Limine leaves it in RSI.
     *   - Multiboot (qemu -kernel / GRUB) leaves it in EBX.
     * Copy EBX->RSI so both paths populate the same slot.
     * ------------------------------------------------------------ */
    movq %rbx, %rsi

    /* Switch to the higher-half _stack_top before entering kernel_main.
     * The early stack (0x70000 + offset) is small; use the real kernel stack
     * which is already at the higher-half VMA and mapped by PD_high[0]. */
    movabsq $_stack_top, %rsp

    /* ------------------------------------------------------------
     * Call kernel_main
     * ------------------------------------------------------------ */
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
/* Flat 32-bit GDT used during early boot (before long mode). Bases are 0,
 * limits 4GB, so any segment-relative access maps to a clean linear addr.
 * Referenced only via the call/pop physical-address trick above, never via
 * a 32-bit absolute relocation against .text. */
.align 4
early_gdt_ptr:
    .space 6, 0            /* scratch: 6-byte GDT/ljmp pointer (physical) */

gdt32_boot:
    .quad 0                                  /* Null */
    .quad 0x00cf9a000000ffff                 /* 32-bit code, base 0, limit 4G */
    .quad 0x00cf92000000ffff                 /* 32-bit data, base 0, limit 4G */
gdt32_boot_end:
