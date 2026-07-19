/*
 * isr_stubs.S  --  x86_64 Assembly Interrupt Service Routine Stubs
 *
 * Generates 256 ISR entry points (vectors 0-255).
 * Each stub:
 *   1. Pushes error code (0 for vectors without hardware error code)
 *   2. Pushes vector number
 *   3. Jumps to common_handler
 *
 * All ISR entry points are exported as global symbols (isr0..isr255)
 * for C code to reference when populating the IDT.
 *
 * Note: We always push two qwords (error_code, vector) for uniform stack frame.
 * Vectors with hardware error codes: 8, 10-14, 17, 30
 * For these, the CPU pushes the error code, so we push an additional 0 for alignment.
 * For consistency, we push 0 for all vectors.
 */

.code64
.intel_syntax noprefix
.section .text

/* ------------------------------------------------------------------
 * External C symbols
 * ------------------------------------------------------------------ */
.extern isr_dispatch
.extern task_switch_asm
.extern g_current_task

/* ------------------------------------------------------------------
 * Macros: ISR_STUB(vector)
 * Generates entry point for interrupt vector.
 *
 * x86-64 ABI: the CPU pushes a hardware error code for vectors
 *   8, 10, 11, 12, 13, 14, 17, 30.  For those we must NOT push a
 *   dummy, or the stack frame is misaligned by 8 bytes and the
 *   trailing iretq pops the wrong RIP/CS/RFLAGS -> immediate #GP ->
 *   double fault -> triple fault.  Everything else gets a dummy 0 so
 *   common_isr_handler sees a uniform frame.
 * ------------------------------------------------------------------ */
.macro ISR_STUB_NOERR vector
.global isr\vector
.hidden isr\vector
.align 16
isr\vector:
    push \vector        /* Push vector number (CPU already pushed error code) */
    jmp common_isr_handler
.endm

.macro ISR_STUB vector
.global isr\vector
.hidden isr\vector
.align 16
isr\vector:
    push 0              /* Dummy error code (uniform frame) */
    push \vector        /* Push vector number */
    jmp common_isr_handler
.endm

/* ------------------------------------------------------------------
 * ISR Stubs for vectors 0-255
 * ------------------------------------------------------------------ */

/* Exceptions (0-31) */
ISR_STUB 0    /* #DE Divide Error */
ISR_STUB 1    /* #DB Debug */
ISR_STUB 2    /* NMI */
ISR_STUB 3    /* #BP Breakpoint */
ISR_STUB 4    /* #OF Overflow */
ISR_STUB 5    /* #BR Bound Range Exceeded */
ISR_STUB 6    /* #UD Invalid Opcode */
ISR_STUB 7    /* #NM Device Not Available */
ISR_STUB_NOERR 8    /* #DF Double Fault (CPU pushes error code) */
ISR_STUB 9    /* Coprocessor Segment Overrun */
ISR_STUB_NOERR 10   /* #TS Invalid TSS */
ISR_STUB_NOERR 11   /* #NP Segment Not Present */
ISR_STUB_NOERR 12   /* #SS Stack Segment Fault */
ISR_STUB_NOERR 13   /* #GP General Protection Fault */
ISR_STUB_NOERR 14   /* #PF Page Fault */
ISR_STUB 15   /* Reserved */
ISR_STUB 16   /* #MF x87 FPU Floating-Point Error */
ISR_STUB_NOERR 17   /* #AC Alignment Check */
ISR_STUB 18   /* #MC Machine Check */
ISR_STUB 19   /* #XM SIMD Floating-Point Exception */
ISR_STUB 20   /* #VE Virtualization Exception */
ISR_STUB 21   /* #CP Control Protection Exception */
ISR_STUB 22   /* Reserved */
ISR_STUB 23   /* Reserved */
ISR_STUB 24   /* Reserved */
ISR_STUB 25   /* Reserved */
ISR_STUB 26   /* Reserved */
ISR_STUB 27   /* Reserved */
ISR_STUB 28   /* Reserved */
ISR_STUB 29   /* Reserved */
ISR_STUB_NOERR 30   /* #SX Security Exception (CPU pushes error code) */
ISR_STUB 31   /* Reserved */

/* Hardware IRQs (32-47) remapped to 0x20-0x2F by PIC */
ISR_STUB 32   /* IRQ0 - Timer */
ISR_STUB 33   /* IRQ1 - Keyboard */
ISR_STUB 34   /* IRQ2 - Cascade */
ISR_STUB 35   /* IRQ3 - Serial 2 */
ISR_STUB 36   /* IRQ4 - Serial 1 */
ISR_STUB 37   /* IRQ5 - Parallel 2/Sound */
ISR_STUB 38   /* IRQ6 - Floppy */
ISR_STUB 39   /* IRQ7 - Parallel 1 */
ISR_STUB 40   /* IRQ8 - RTC */
ISR_STUB 41   /* IRQ9 - ACPI */
ISR_STUB 42   /* IRQ10 - Network */
ISR_STUB 43   /* IRQ11 - USB */
ISR_STUB 44   /* IRQ12 - Mouse */
ISR_STUB 45   /* IRQ13 - FPU */
ISR_STUB 46   /* IRQ14 - Primary ATA */
ISR_STUB 47   /* IRQ15 - Secondary ATA */

/* User-defined / Software interrupts (48-255) */
ISR_STUB 48
ISR_STUB 49
ISR_STUB 50
ISR_STUB 51
ISR_STUB 52
ISR_STUB 53
ISR_STUB 54
ISR_STUB 55
ISR_STUB 56
ISR_STUB 57
ISR_STUB 58
ISR_STUB 59
ISR_STUB 60
ISR_STUB 61
ISR_STUB 62
ISR_STUB 63
ISR_STUB 64
ISR_STUB 65
ISR_STUB 66
ISR_STUB 67
ISR_STUB 68
ISR_STUB 69
ISR_STUB 70
ISR_STUB 71
ISR_STUB 72
ISR_STUB 73
ISR_STUB 74
ISR_STUB 75
ISR_STUB 76
ISR_STUB 77
ISR_STUB 78
ISR_STUB 79
ISR_STUB 80
ISR_STUB 81
ISR_STUB 82
ISR_STUB 83
ISR_STUB 84
ISR_STUB 85
ISR_STUB 86
ISR_STUB 87
ISR_STUB 88
ISR_STUB 89
ISR_STUB 90
ISR_STUB 91
ISR_STUB 92
ISR_STUB 93
ISR_STUB 94
ISR_STUB 95
ISR_STUB 96
ISR_STUB 97
ISR_STUB 98
ISR_STUB 99
ISR_STUB 100
ISR_STUB 101
ISR_STUB 102
ISR_STUB 103
ISR_STUB 104
ISR_STUB 105
ISR_STUB 106
ISR_STUB 107
ISR_STUB 108
ISR_STUB 109
ISR_STUB 110
ISR_STUB 111
ISR_STUB 112
ISR_STUB 113
ISR_STUB 114
ISR_STUB 115
ISR_STUB 116
ISR_STUB 117
ISR_STUB 118
ISR_STUB 119
ISR_STUB 120
ISR_STUB 121
ISR_STUB 122
ISR_STUB 123
ISR_STUB 124
ISR_STUB 125
ISR_STUB 126
ISR_STUB 127
ISR_STUB 128
ISR_STUB 129
ISR_STUB 130
ISR_STUB 131
ISR_STUB 132
ISR_STUB 133
ISR_STUB 134
ISR_STUB 135
ISR_STUB 136
ISR_STUB 137
ISR_STUB 138
ISR_STUB 139
ISR_STUB 140
ISR_STUB 141
ISR_STUB 142
ISR_STUB 143
ISR_STUB 144
ISR_STUB 145
ISR_STUB 146
ISR_STUB 147
ISR_STUB 148
ISR_STUB 149
ISR_STUB 150
ISR_STUB 151
ISR_STUB 152
ISR_STUB 153
ISR_STUB 154
ISR_STUB 155
ISR_STUB 156
ISR_STUB 157
ISR_STUB 158
ISR_STUB 159
ISR_STUB 160
ISR_STUB 161
ISR_STUB 162
ISR_STUB 163
ISR_STUB 164
ISR_STUB 165
ISR_STUB 166
ISR_STUB 167
ISR_STUB 168
ISR_STUB 169
ISR_STUB 170
ISR_STUB 171
ISR_STUB 172
ISR_STUB 173
ISR_STUB 174
ISR_STUB 175
ISR_STUB 176
ISR_STUB 177
ISR_STUB 178
ISR_STUB 179
ISR_STUB 180
ISR_STUB 181
ISR_STUB 182
ISR_STUB 183
ISR_STUB 184
ISR_STUB 185
ISR_STUB 186
ISR_STUB 187
ISR_STUB 188
ISR_STUB 189
ISR_STUB 190
ISR_STUB 191
ISR_STUB 192
ISR_STUB 193
ISR_STUB 194
ISR_STUB 195
ISR_STUB 196
ISR_STUB 197
ISR_STUB 198
ISR_STUB 199
ISR_STUB 200
ISR_STUB 201
ISR_STUB 202
ISR_STUB 203
ISR_STUB 204
ISR_STUB 205
ISR_STUB 206
ISR_STUB 207
ISR_STUB 208
ISR_STUB 209
ISR_STUB 210
ISR_STUB 211
ISR_STUB 212
ISR_STUB 213
ISR_STUB 214
ISR_STUB 215
ISR_STUB 216
ISR_STUB 217
ISR_STUB 218
ISR_STUB 219
ISR_STUB 220
ISR_STUB 221
ISR_STUB 222
ISR_STUB 223
ISR_STUB 224
ISR_STUB 225
ISR_STUB 226
ISR_STUB 227
ISR_STUB 228
ISR_STUB 229
ISR_STUB 230
ISR_STUB 231
ISR_STUB 232
ISR_STUB 233
ISR_STUB 234
ISR_STUB 235
ISR_STUB 236
ISR_STUB 237
ISR_STUB 238
ISR_STUB 239
ISR_STUB 240
ISR_STUB 241
ISR_STUB 242
ISR_STUB 243
ISR_STUB 244
ISR_STUB 245
ISR_STUB 246
ISR_STUB 247
ISR_STUB 248
ISR_STUB 249
ISR_STUB 250
ISR_STUB 251
ISR_STUB 252
ISR_STUB 253
ISR_STUB 254
ISR_STUB 255

/* ------------------------------------------------------------------
 * Common ISR Handler
 * ------------------------------------------------------------------ */
.global common_isr_handler
.hidden common_isr_handler
.align 16
common_isr_handler:
    /* Save all registers */
    push rdi
    push rsi
    push rdx
    push rcx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    /* RSP now points to our saved registers.
     * Stack layout (from RSP):
     *   r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8, rcx, rdx, rsi, rdi  (14 * 8 = 112)
     *   error_code (8), vector (8)                                               (16)
     *   RIP, CS, RFLAGS, RSP, SS (pushed by CPU)                                 (40)
     * Total offset to vector = 112 + 8 = 120
     * Total offset to error_code = 112
     */

    mov rdi, rsp          /* RDI = frame pointer (InterruptFrame*) */
    mov rsi, [rsp + 120]  /* RSI = vector number */

    /* DIAGNOSTIC: emit the faulting vector as a hex digit so the serial
     * trace tells us WHICH vector triple-faults the moment the IDT is live.
     * Vectors 0-15 -> '0'-'9','A'-'F'; 16-255 -> raw modulo mapped to 'p'.. */
    mov rcx, rsi
    cmp rcx, 15
    ja  .vec_hi
    add cl, '0'
    cmp cl, '9'
    jbe .vec_emit
    add cl, ('A' - '9' - 1)
    jmp .vec_emit
.vec_hi:
    /* 16..255 -> 'p' + (vec>>4) to stay printable */
    shr rcx, 4
    add cl, 'p'
.vec_emit:
    mov dx, 0x3F8
    mov al, cl
    out dx, al

    call isr_dispatch

    /* Restore registers (reverse order) */
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rcx
    pop rdx
    pop rsi
    pop rdi

    /* Pop error_code and vector */
    add rsp, 16

    iretq

/* ------------------------------------------------------------------
 * SYSCALL/SYSRET Entry Point (Fast Path)
 * ------------------------------------------------------------------ */
.global syscall_entry
.hidden syscall_entry
.align 16
syscall_entry:
    /* SYSCALL calling convention (x86-64):
     *   RCX = return RIP
     *   R11 = return RFLAGS
     *   RDI, RSI, RDX, R10, R8, R9 = args 1-6 (same as System V ABI)
     *   RAX = syscall number
     *
     * We need to save user context and switch to kernel stack.
     * IST is not used for syscall (trap gate), so we use the kernel stack.
     */

    /* Swap to kernel GS base (if using FS/GS for per-CPU data) */
    swapgs

    /* Save user RSP and SS so we can return with SYSRET */
    mov [rsp - 8], rsp      /* Save user RSP below current stack */
    mov [rsp - 16], ss      /* Save user SS */
    sub rsp, 16

    /* Save user registers that SYSCALL clobbers (RCX, R11) */
    push rcx                /* User RIP */
    push r11                /* User RFLAGS */

    /* Save rest of registers (callee-saved + caller-saved we care about) */
    push rdi
    push rsi
    push rdx
    push rcx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    /* RDI = frame pointer for handler */
    mov rdi, rsp
    mov rsi, rax            /* Syscall number in RSI */

    call syscall_handler

    /* Restore registers */
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rcx
    pop rdx
    pop rsi
    pop rdi

    /* Restore user RFLAGS and RIP */
    pop r11
    pop rcx

    /* Restore user RSP and SS */
    add rsp, 16
    /* Note: SYSRET requires RCX=RIP, R11=RFLAGS, RSP=user_rsp, SS=user_ss */

    swapgs
    sysretq

/* ------------------------------------------------------------------
 * IDT Pointer (for lidt)
 * ------------------------------------------------------------------ */
.section .rodata
.global idt_ptr
.hidden idt_ptr
.align 8
idt_ptr:
    .word 2047          /* Limit: 256 * 16 - 1 */
    .quad idt_table     /* Base: address of IDT table */

/* ------------------------------------------------------------------
 * IDT Table (256 entries, 16 bytes each)
 * ------------------------------------------------------------------ */
.section .bss
.global idt_table
.hidden idt_table
.align 16
idt_table:
    .fill 256, 16, 0

/* ------------------------------------------------------------------
 * Global current task pointer
 * ------------------------------------------------------------------ */
.section .bss
.global g_current_task
.hidden g_current_task
.align 8
g_current_task:
    .quad 0

.section .note.GNU-stack,"",@progbits