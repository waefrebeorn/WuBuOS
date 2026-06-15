/*
 * kernel.ld — WuBuOS Bare-Metal Kernel Linker Script
 * 
 * Links kernel at 1MB (0x100000) physical / higher half virtual
 * Compatible with Limine/Stivale2 boot protocol
 */

ENTRY(_start)

SECTIONS
{
    /* Physical load address: 1MB (below this is BIOS/ACPI reserved) */
    . = 0x100000;
    
    /* Virtual address for higher half kernel */
    . = 0xffffffff80000000 + 0x100000;
    
    /* Kernel text (code + rodata) */
    .text : ALIGN(0x1000) {
        *(.multiboot_header)
        *(.limine_requests)
        *(.text*)
        *(.rodata*)
        *(.gcc_except_table)
        KEEP(*(.eh_frame*))
    }
    
    /* Kernel data (initialized) */
    .data : ALIGN(0x1000) {
        *(.data*)
    }
    
    /* BSS (zero-initialized) */
    .bss : ALIGN(0x1000) {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    }
    
    /* End of kernel */
    _kernel_end = .;
    
    /* Symbols for physical address calculation */
    _physical_offset = 0xffffffff80000000;
    
    /* Discard sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}