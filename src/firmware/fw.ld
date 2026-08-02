/*
 * fw.ld  --  WuBuFW flash image layout.
 *
 * 256KB ROM ending at the 4GB boundary (QEMU -bios placement):
 *   0xFFFC0000  firmware body (LMA)  -> copied to 0x00200000 (VMA) by reset.S
 *   0xFFFF0000  16/32-bit reset stub + GDT (runs in place from flash)
 *   0xFFFFFFF0  CPU reset vector
 *
 * The body is linked for its RAM VMA but stored in flash, so it uses an
 * explicit AT> region rather than an AT() expression (which would resolve
 * against the wrong dot for every section after the first).
 */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_reset_vector)

MEMORY
{
    RAM   (rwx) : ORIGIN = 0x00200000, LENGTH = 0x00200000
    FLASH (rx)  : ORIGIN = 0xFFFC0000, LENGTH = 0x00030000
    STUB  (rx)  : ORIGIN = 0xFFFF0000, LENGTH = 0x0000FFF0
    RVEC  (rx)  : ORIGIN = 0xFFFFFFF0, LENGTH = 0x00000010
}

SECTIONS
{
    _body_vma = 0x00200000;
    _body_lma = 0xFFFC0000;

    /* One output section: AT> advances its own LMA counter and would drift
     * from the VMA at every ALIGN, so the body must be contiguous for the
     * flat flash->RAM copy in reset.S to be correct. */
    .body : ALIGN(16) {
        *(.text .text.*)
        . = ALIGN(32);
        *(.rodata .rodata.*)
        . = ALIGN(32);
        *(.data .data.*)
        *(.got .got.plt)
        . = ALIGN(16);
    } > RAM AT> FLASH

    _body_end  = .;
    _body_size = _body_end - _body_vma;

    .bss (NOLOAD) : ALIGN(16) {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        _bss_end = .;
    } > RAM

    ASSERT(_body_size <= 0x30000, "WuBuFW body exceeds the 192KB flash window")

    .reset16 : ALIGN(16) {
        *(.reset16)
    } > STUB

    .resetvec : {
        *(.resetvec)
    } > RVEC

    /DISCARD/ : {
        *(.comment) *(.note*) *(.eh_frame*) *(.dynamic) *(.dynsym) *(.dynstr)
        *(.hash) *(.gnu.hash) *(.interp) *(.rela*) *(.debug_pubnames)
    }
}
