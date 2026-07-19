/*
 * boot.S -- WuBuOS minimal BIOS bootloader (real -> protected mode)
 *
 * Loaded by SeaBIOS at 0x7C00. Reads the kernel ELF (raw, concatenated at
 * LBA 1) into conventional RAM, parses its 64-bit program headers, copies
 * each PT_LOAD segment to its physical LMA (0x100000+), switches to 32-bit
 * protected mode with a flat 4GB GDT, and jumps to the kernel entry
 * (_start, which finishes the long-mode transition in crt0.S).
 *
 * This is the self-contained QEMU validation harness for the bare-metal
 * kernel: it proves crt0.S paging + long-mode + kernel_main actually run.
 *
 * Self-contained: no includes, no libc. Diagnostics are written to the COM1
 * serial port (0x3F8) so `qemu -serial stdio` captures the boot trace.
 *
 * Layout note: SeaBIOS loads a raw 512-byte bootsector at LINEAR 0x0000
 * (CS=0, IP=0), NOT at 0x7C00. Therefore we anchor the section at `.org 0`
 * so every symbol's VMA equals its file offset equals its linear load
 * address.  _start16 is the first symbol (at 0), and the boot signature
 * (computed as `510 - (. - _start16)`) lands exactly at offset 510.
 * Helper/string/GDT/DAP data are defined AFTER _start16; they are reached
 * only via `call`/absolute-load so ordering is irrelevant to execution.
 * The early stack is placed at 0x7C00 (safely above the 512-byte sector).
 */

.org 0

.code16
.section .text
.global _start16

_start16:
    cli
    cld                         /* lodsb/stosb must increment, not decrement */
    /* Boot sector physically sits at linear 0x7C00.  Point the data segments
     * there so near references line up with reality (boot contract). */
    movw $0x07C0, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7C00, %sp          /* stack grows down from just below us */

    /* Immediate entry marker via the real serial_putc path. */
    movb $'B', %al
    call serial_putc

    movw $str_boot, %si
    call serial_puts

    /* Load the kernel ELF (LBA 1+) into 0x10000 (conventional RAM). */
    movw $disk_dap, %si
    movb $0x42, %ah            /* BIOS: extended read sectors (LBA) */
    int  $0x13
    jc   disk_err

    movw $str_loaded, %si
    call serial_puts

    /* Switch to 32-bit protected mode with a flat 4GB GDT. */
    lgdt (gdt32_ptr)
    movl %cr0, %eax
    orl  $1, %eax
    movl %eax, %cr0
    jmp  $0x08, $1f

.code32
1:
    cld                         /* lodsb/stosl increment in 32-bit too */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movl $0x90000, %esp

    /* Parse the ELF64 header at 0x10000. */
    movl $0x10000, %ebx
    cmpb $0x7F, (%ebx)         /* ELF magic */
    jne elf_err
    cmpb $'E', 1(%ebx)
    jne elf_err
    cmpb $'L', 2(%ebx)
    jne elf_err
    cmpb $'F', 3(%ebx)
    jne elf_err

    /* e_entry @0x18 (8B). The kernel is linked at VMA 0xffffffff80000000+
     * but LOADED at LMA 0x100000. So the physical entry is simply the low
     * 32 bits of e_entry minus the higher-half base (0x80000000); we must
     * NOT also add 0x100000 (that would double-count the LMA). */
    movl 0x18(%ebx), %eax      /* entry low 32 bits (higher-half VA) */
    subl $0x80000000, %eax     /* -> physical load address of _start */
    movl %eax, %edi            /* save physical entry for later */

    movl $str_elfok, %esi
    /* In pmode DS base = 0 and the string is below 64K (org 0), so the
     * low-16-bit near pointer in SI addresses it correctly. */
    movl %esi, %eax
    movw %ax, %si
    call serial_puts

    /* Walk program headers, copying each PT_LOAD segment to its LMA. */
    xorl %ecx, %ecx
    movw 0x38(%ebx), %cx       /* e_phnum */
    movl 0x20(%ebx), %esi       /* e_phoff */
    addl %ebx, %esi             /* pointer to first program header */

ph_loop:
    cmpl $0, %ecx
    je   ph_done
    decl %ecx

    movl (%esi), %edx          /* p_type */
    cmpl $1, %edx              /* PT_LOAD? */
    jne  ph_next

    movl 0x08(%esi), %edx      /* p_offset */
    movl 0x18(%esi), %eax      /* p_paddr (physical LMA) */
    movl 0x20(%esi), %ebp      /* p_filesz */
    movl 0x28(%esi), %ecx      /* p_memsz */

    /* Copy p_filesz bytes from (0x10000 + p_offset) -> p_paddr. */
    pushl %esi
    pushl %ecx
    movl $0x10000, %esi
    addl %edx, %esi            /* src = file base + p_offset */
    movl %eax, %edi            /* dst = p_paddr */
    movl %ebp, %ecx
    shrl $2, %ecx              /* dwords */
    cld
    rep movsl

    /* Zero (p_memsz - p_filesz) trailing bytes (BSS). */
    movl %eax, %edi
    addl %ebp, %edi            /* start of zero region */
    popl %ecx                  /* restore p_memsz into ecx */
    subl %ebp, %ecx            /* trailing bytes */
    xorl %eax, %eax
    movl %ecx, %edx
    shrl $2, %edx
    rep stosl
    popl %esi

ph_next:
    addl $56, %esi             /* sizeof(Elf64_Phdr) */
    jmp ph_loop

ph_done:
    /* Recompute physical entry (edi may have been clobbered above). */
    movl 0x18(%ebx), %eax
    subl $0x80000000, %eax
    jmp  *%eax                 /* enter kernel _start (32-bit entry) */

/* ---- 16-bit serial helpers (COM1 @ 0x3F8) ------------------------------- */

serial_putc:                     /* al = byte */
    pushw %ax
    pushw %dx
1:
    movw $0x3FD, %dx            /* LSR */
    inb  %dx, %al
    testb $0x20, %al           /* THR empty? */
    jz   1b
    popw %dx                   /* restore dx (was port placeholder) */
    popw %ax                   /* restore byte into al */
    movw $0x3F8, %dx           /* THR port */
    outb %al, %dx
    ret

serial_puts:                    /* si = NUL-terminated string */
    pushw %si
    pushw %ax
1:
    lodsb
    testb %al, %al
    jz   2f
    call serial_putc
    jmp  1b
2:
    popw %ax
    popw %si
    ret

/* ---- Diagnostics strings ------------------------------------------------- */

str_boot:    .asciz "WuBuOS BOOT\r\n"
str_loaded:  .asciz "LOADED\r\n"
str_elfok:   .asciz "ELF OK\r\n"
str_diskerr: .asciz "DISKERR\r\n"
str_elferr:  .asciz "ELFERR\r\n"

disk_err:
    movw $str_diskerr, %si
    call serial_puts
    jmp .

elf_err:
    movw $str_elferr, %si
    call serial_puts
    jmp .

/* --- GDT for flat 32-bit protected mode --- */
.align 4
gdt32:
    .quad 0
    .quad 0x00cf9a000000ffff   /* 32-bit code, base 0, limit 4G */
    .quad 0x00cf92000000ffff   /* 32-bit data, base 0, limit 4G */
gdt32_ptr:
    .word gdt32_ptr - gdt32 - 1
    .long gdt32

/* --- Disk Address Packet for int 0x13 AH=0x42 --- */
.align 4
disk_dap:
    .byte 0x10                  /* packet size */
    .byte 0                     /* reserved */
    .word 900                   /* sectors to read (covers kernel.elf) */
    .word 0x0000                /* dest offset */
    .word 0x1000                /* dest segment (0x1000:0 = 0x10000) */
    .quad 1                     /* start LBA = 1 (sector 0 = this bootsector) */

/* Boot signature must sit at byte offset 510 of the sector regardless of
 * how much code precedes it. Use .org to pin it absolutely rather than a
 * location-relative .space (which goes negative and drops the sig once the
 * bootsector code grows past 510 bytes). */
    .org 510
    .word 0xAA55                /* boot signature */
