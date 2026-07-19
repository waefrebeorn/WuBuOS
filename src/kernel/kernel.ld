/*
 * kernel.ld — WuBuOS Bare-Metal Kernel Linker Script
 *
 * Links the kernel to run at a higher-half virtual address
 * (0xffffffff80000000 + 1MB) but to be LOADED at a low physical address
 * (1MB). The VMA/LMA split (via a single PT_LOAD + AT()) is what lets the
 * 32-bit boot stub (crt0.S) reference page-table and kernel bases with plain
 * 32-bit immediates: it uses the *_phys symbols (low physical addresses),
 * while the running kernel uses the higher-half VMA symbols.
 *
 * CRITICAL LAYOUT INVARIANT (do not regress):
 *   The ENTIRE kernel (text + data + bss) lives in ONE PT_LOAD segment with a
 *   single AT(0x100000).  GNU ld then places every section at
 *       LMA = VMA - 0xffffffff80000000
 *   exactly, so the file offset of a section equals (LMA - 0x100000) and the
 *   section-header walk in crt0.S (physical(v) = elf_base + sh_offset +
 *   (v - sh_addr)) resolves every VMA to its true load address.  Using
 *   separate per-section AT() values (the previous bug) broke this: .data's
 *   LMA (0x112720) diverged from its file offset (0x14000), so crt0 mapped
 *   .data's VMA onto the wrong physical page and the first read of a .data
 *   global triple-faulted.  Keep it ONE segment, ONE AT().
 *
 * Compatible with the Limine/Stivale2 boot protocol.
 */

ENTRY(_start)

/* Physical load base (what the bootloader places in RAM). */
KERNEL_PHYS = 0x100000;

PHDRS {
    /* Single load segment: VMA higher-half, LMA = VMA - 0x80000000. */
    kernel PT_LOAD FLAGS(7) ;   /* R+W+X (permissive; fine for early boot) */
}

SECTIONS
{
    /* Virtual (higher-half) base. */
    . = 0xffffffff80000000 + 0x100000;

    /* Kernel text (code + rodata).
     * AT(0x100000) means file offset 0 of the segment is loaded at physical
     * 1MB; every later section's LMA = VMA - 0xffffffff80000000. */
    .text : AT(KERNEL_PHYS) {
        *(.multiboot_header)
        *(.limine_requests)
        *(.text*)
        *(.rodata*)
        *(.gcc_except_table)
        KEEP(*(.eh_frame*))
    } :kernel

    /* Kernel data (initialized) — same segment, LMA follows .text. */
    .data : {
        *(.data*)
    } :kernel

    /* BSS (zero-initialized) — same segment. */
    .bss : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } :kernel

    /* End of kernel (higher-half VMA). */
    _kernel_end = .;

    /* Symbols referenced by crt0.S for early boot wiring.
     *   _kernel_start -- higher-half virtual base of the loaded image;
     *                    crt0 uses it to map the PD[511] higher-half entry.
     *   _kernel_phys_start -- physical load address (1MB); used by the
     *                   32-bit stub for page-table allocation and mapping.
     *   _stack_top    -- top of the early boot stack (just after the BSS). */
    _kernel_start = .;
    _kernel_phys_start = KERNEL_PHYS;
    . = . + 0x10000;            /* 64 KiB early boot stack */
    _stack_top = .;

    /* Symbols for physical address calculation (crt0 32-bit math). */
    _physical_offset = 0xffffffff80000000;

    /* Absolute LOW-PHYSICAL counterparts of the higher-half VMA symbols.
     * The 32-bit boot stub (crt0.S) may only reference these (values <4GB),
     * because 32-bit immediate/absolute loads against a higher-half symbol
     * are rejected by -mcmodel=kernel. _physical_offset subtracted at link
     * time yields the real load address. */
    _kernel_phys_base    = ABSOLUTE(_kernel_start) - _physical_offset;
    _kernel_phys_end     = ABSOLUTE(_kernel_end)   - _physical_offset;

    /* Early-boot scratch variables. Defined as ABSOLUTE low-physical addresses
     * (<4GB) so the 32-bit boot stub (crt0.S) can reference them with plain
     * 32-bit loads, while the identity map also makes them reachable from
     * higher-half 64-bit code. These are NOT in .bss (which is higher-half). */
    page_table_base  = 0x90000;
    pdp_base         = 0x90008;   /* PDP for identity map (low 4GB) */
    pd_base          = 0x90010;   /* PD  for identity map */
    pt_base          = 0x90018;
    pdp_high_base    = 0x90020;   /* PDP for higher-half kernel */
    pd_high_base     = 0x90028;   /* PD  for higher-half kernel */
    limine_boot_info = 0x90030;
    long_mode_jmp    = 0x90038;   /* 6-byte far-jump target for ljmp */
    boot_magic       = 0x90058;   /* scratch: boot protocol magic captured at _start */

    /* Discard sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}
