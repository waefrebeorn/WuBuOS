/*
 * payload.ld -- flat link for the WuBuOS test EFI payload.
 * Fixed base so mkpe can produce a relocation-free PE32+.
 */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x04000000;
    _image_start = .;

    .text : {
        *(.text.startup)
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data .data.*)
        . = ALIGN(16);
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        _bss_end = .;
    }

    /DISCARD/ : {
        *(.comment) *(.note*) *(.eh_frame*) *(.rela*) *(.dynamic)
    }
}
