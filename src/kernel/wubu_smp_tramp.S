/* wubu_smp_tramp.S -- the AP trampoline (gap I2).
 *
 * Copied to 0x8000 by wubu_smp_start_aps, entered by the SIPI (vector
 * 8) in 16-bit real mode. The shared control block sits at 0x7000:
 *   +0  magic, +4 cpu_count, +8.. alive[8], +40 gdtr (6 bytes),
 *   +48 cr3_value (8 bytes)
 *
 * The trampoline: lgdt from the control block, enable PAE + LME + PG,
 * far-jump to 64-bit, park bumping alive[lapic_id].
 *
 * The 64-bit entry is PINNED at offset 0x100 in the trampoline so the
 * 16-bit far-jump can use a fixed address (0x8100) -- a relocated
 * symbol would carry the kernel's higher-half address and truncate.
 */
    .intel_syntax noprefix
    .code16
    .global wubu_smp_tramp_start
wubu_smp_tramp_start:
    cli
    /* lgdt from the control block (+40): 6 bytes at 0x7028 */
    lgdt [0x7028]
    /* PAE */
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax
    /* EFER.LME */
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr
    /* CR3 = the BSP's page tables */
    mov eax, [0x7030]
    mov cr3, eax
    /* PG */
    mov eax, cr0
    or eax, 0x80000001
    mov cr0, eax
    /* long mode via the kernel's flat CS (GDT index 1) */
    jmp 0x08:0x8100

    .code64
    .org 0x100
    .global wubu_smp_ap64
wubu_smp_ap64:
    /* alive[lapic_id]: lapic id from the LAPIC ID register */
    mov rax, 0xFEE00020
    mov eax, [rax]
    shr eax, 24
    mov ebx, eax                 /* cpu index */
    mov rax, 0x7000              /* control block */
    lea rax, [rax + rbx*4 + 8]   /* alive[cpu] */
ap_loop:
    lock inc dword ptr [rax]
    mov ecx, 1000000
1:  dec ecx
    jnz 1b
    jmp ap_loop

    .global wubu_smp_tramp_end
wubu_smp_tramp_end:
