;
; crt0.S  --  WuBuOS Bare-Metal Startup (Limine Protocol)
;
; Entry point for Limine bootloader. Sets up:
;   - Long mode (if booted in 32-bit)
;   - Initial page tables (identity + higher half)
;   - Stack
;   - Passes Limine boot info to kernel_main
;
; Build: nasm -f elf64 crt0.S -o crt0.o
; Link:  ld -T kernel.ld -o kernel.elf crt0.o ...
;

bits 32
section .multiboot_header

; Limine boot protocol header
; Request ID: Framebuffer (0xc7b1dd30df4c8b88, 0x6e1b0a4b5b8d7c0a)
; Request ID: Memmap    (0x67cf3d9d378a806f, 0xc323c3c6f73f3e2a)

ALIGN 8
limine_magic:
    dd 0xf26b9c2b  ; Limine magic
    dd 0x00000000  ; Revision
    dd limine_requests_start - limine_magic  ; Checksum (size of header)

limine_requests_start:
    ; Framebuffer request
    dq limine_fb_req
    ; Memmap request
    dq limine_memmap_req
    ; Terminal request (for early logging)
    dq limine_terminal_req
    ; SMP request (for multi-core)
    dq limine_smp_req
    ; End marker
    dq 0

limine_terminal_req:
    dq 0xc8ac59310c2b0844, 0x13d2b5a15d21a53b  ; Terminal UUID
    dq 0  ; revision

limine_smp_req:
    dq 0x95a67b819a1b857e, 0xa0b61b723b6a73e0  ; SMP UUID
    dq 0  ; revision
    dq 0  ; flags (0 = no X2APIC)

limine_fb_req:
    dq 0xc7b1dd30df4c8b88, 0x6e1b0a4b5b8d7c0a  ; Framebuffer UUID
    dq 0  ; revision
    ; Response pointer filled by Limine

limine_memmap_req:
    dq 0x67cf3d9d378a806f, 0xc323c3c6f73f3e2a  ; Memmap UUID
    dq 0  ; revision
    ; Response pointer filled by Limine

section .text
global _start
extern kernel_main
extern _kernel_start
extern _kernel_end
extern _bss_start
extern _bss_end
extern _stack_top

_start:
    ; ------------------------------------------------------------
    ; We're in 32-bit protected mode (Limine puts us here)
    ; Switch to long mode
    ; ------------------------------------------------------------
    
    ; Disable interrupts
    cli
    
    ; Check CPUID for long mode support
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29  ; LM bit
    jz .no_long_mode
    
    ; Check for 1GB pages (PDPE)
    test edx, 1 << 26
    jz .no_1gb_pages
    
    ; ------------------------------------------------------------
    ; Set up page tables (4-level: PML4 -> PDP -> PD -> PT)
    ; Identity map first 4GB, higher half kernel at 0xffffffff80000000
    ; ------------------------------------------------------------
    
    ; Allocate page tables at _kernel_end (aligned to 4KB)
    mov edi, _kernel_end
    add edi, 0xfff
    and edi, ~0xfff
    mov [page_table_base], edi
    
    ; PML4 at page_table_base
    ; PDP  at page_table_base + 4KB
    ; PD   at page_table_base + 8KB
    ; PT   at page_table_base + 12KB (for first 2MB identity)
    
    mov eax, edi                    ; PML4
    mov [page_table_base], eax
    add eax, 0x1000                 ; PDP
    mov [pdp_base], eax
    add eax, 0x1000                 ; PD
    mov [pd_base], eax
    add eax, 0x1000                 ; PT
    mov [pt_base], eax
    
    ; Zero page tables (4 pages = 16KB)
    mov ecx, 4096
    xor eax, eax
    mov edi, [page_table_base]
    rep stosd
    
    ; PML4[0] -> PDP (identity map, first 4GB)
    mov eax, [pdp_base]
    or eax, 0x3  ; Present | Writable
    mov edi, [page_table_base]
    mov [edi], eax
    
    ; PML4[511] -> PDP (higher half kernel)
    mov eax, [pdp_base]
    or eax, 0x3
    mov [edi + 511*8], eax
    
    ; PDP[0] -> PD (first 1GB)
    mov eax, [pd_base]
    or eax, 0x3
    mov edi, [pdp_base]
    mov [edi], eax
    
    ; PDP[511] -> PD (last 1GB for higher half)
    mov eax, [pd_base]
    or eax, 0x3
    mov [edi + 511*8], eax
    
    ; PD[0..511] -> 2MB pages (identity map 1GB)
    mov edi, [pd_base]
    mov eax, 0x83  ; Present | Writable | Page Size (2MB)
    mov ecx, 512
.pd_loop:
    mov [edi], eax
    add eax, 0x200000  ; 2MB
    add edi, 8
    loop .pd_loop
    
    ; PD[511] entries for higher half (map kernel at 0xffffffff80000000)
    ; Kernel physical address is at _kernel_start (typically 0x100000 = 1MB)
    mov eax, _kernel_start
    
    ; Clear low bits, set flags
    and eax, ~0x1fffff
    or eax, 0x83
    
    lea edi, [edi + 511*8 - 512*8]  ; Back to start of PD
    add edi, 511*8
    mov [edi], eax
    
    ; ------------------------------------------------------------
    ; Enable PAE, PGE, NXE
    ; ------------------------------------------------------------
    mov eax, cr4
    or eax, (1 << 5) | (1 << 7)  ; PAE | PGE
    mov cr4, eax
    
    ; Set NX bit in EFER
    mov ecx, 0xC0000080  ; EFER MSR
    rdmsr
    or eax, 1 << 11  ; NXE
    wrmsr
    
    ; Load CR3 (PML4)
    mov eax, [page_table_base]
    mov cr3, eax
    
    ; Enable Long Mode + Paging
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8  ; LME
    wrmsr
    
    mov eax, cr0
    or eax, 1 << 31  ; PG (paging)
    mov cr0, eax
    
    ; ------------------------------------------------------------
    ; Jump to 64-bit code
    ; ------------------------------------------------------------
    lgdt [gdt64_ptr]
    jmp 0x08:.long_mode_start  ; 0x08 = kernel code segment
    
.no_long_mode:
    hlt
    jmp .no_long_mode
    
.no_1gb_pages:
    hlt
    jmp .no_1gb_pages

bits 64
.long_mode_start:
    ; ------------------------------------------------------------
    ; In long mode!
    ; Set up 64-bit segments
    ; ------------------------------------------------------------
    mov ax, 0x10  ; Data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov rsp, _stack_top
    
    ; Zero BSS (kernel_main does this too, but do early)
    ; Actually kernel_main will do it
    
    ; ------------------------------------------------------------
    ; Parse Limine boot info from registers
    ; Limine passes:
    ;   RSI = pointer to boot info
    ; ------------------------------------------------------------
    
    ; Save boot info pointer
    mov [limine_boot_info], rsi
    
    ; ------------------------------------------------------------
    ; Call kernel_main
    ; ------------------------------------------------------------
    call kernel_main
    
    ; Should never return
    cli
    hlt
    jmp $

section .rodata
gdt64:
    dq 0  ; Null descriptor
    dq (1<<47)|(1<<44)|(1<<41)|(1<<43)|(1<<53)  ; Kernel code: P=1, DPL=0, S=1, Type=0xA (code, execute/read), L=1, D=0
    dq (1<<47)|(1<<44)|(1<<41)|(1<<53)           ; Kernel data: P=1, DPL=0, S=1, Type=0x2 (data, read/write), L=1

gdt64_ptr:
    dw gdt64_ptr - gdt64 - 1
    dq gdt64

section .bss
align 4096
page_table_base:  resq 1
pdp_base:         resq 1
pd_base:          resq 1
pt_base:          resq 1
limine_boot_info: resq 1

; Stack (16KB)
stack_bottom:
    resb 16384
stack_top: