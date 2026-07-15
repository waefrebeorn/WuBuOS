/*
 * kernel.ld — WuBuOS Bare-Metal Kernel Linker Script
 *
 * Links the kernel to run at a higher-half virtual address
 * (0xffffffff80000000 + 1MB) but to be LOADED at a low physical address
 * (1MB). The VMA/LMA split (via PHDRS + AT()) is what lets the 32-bit
 * boot stub (crt0.S) reference page-table and kernel bases with plain
 * 32-bit immediates: it uses the *_phys symbols (low physical addresses),
 * while the running kernel uses the higher-half VMA symbols.
 *
 * Compatible with the Limine/Stivale2 boot protocol.
 */

ENTRY(_start)

/* Physical load base (what the bootloader places in RAM). */
KERNEL_PHYS = 0x100000;

PHDRS {
    /* Load segment: VMA is higher-half, LMA is physical 1MB. */
    text PT_LOAD FLAGS(5) ;   /* R+X */
    data PT_LOAD FLAGS(6) ;   /* R+W */
    bss  PT_LOAD FLAGS(6) ;   /* R+W */
}

SECTIONS
{
    /* Virtual (higher-half) base. */
    . = 0xffffffff80000000 + 0x100000;

    /* Kernel text (code + rodata).
     * NOTE: AT(0) + NO file-alignment places the .text segment's FILE offset
     * at 0, so the ELF image is contiguous from the very start of the file.
     * This is required so `qemu -kernel` / multiboot loaders map the whole
     * image 1:1 to a page-aligned physical base; otherwise a higher-half VMA
     * page can straddle two physical pages and the 4KB page-table mapping
     * cannot represent it.  The 32-bit boot stub computes the real physical
     * base at runtime (scans for the "WuBuOS" signature), so the VMA/LMA split
     * here is only used by the linker for the *_phys symbols. */
    .text : AT(0) {
        *(.multiboot_header)
        *(.limine_requests)
        *(.text*)
        *(.rodata*)
        *(.gcc_except_table)
        KEEP(*(.eh_frame*))
    } :text

    /* Kernel data (initialized) */
    .data : AT(KERNEL_PHYS + SIZEOF(.text)) ALIGN(0x1000) {
        *(.data*)
    } :data

    /* BSS (zero-initialized) */
    .bss : AT(KERNEL_PHYS + SIZEOF(.text) + SIZEOF(.data)) ALIGN(0x1000) {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } :bss

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

    /* Discard sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}
