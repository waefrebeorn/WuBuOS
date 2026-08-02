/*
 * loader.ld -- flat link for the WuBuOS WuBuFW chainloader.
 * Fixed base (0x04000000) so mkpe can produce a relocation-free PE32+,
 * exactly like the firmware's test payload.
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
