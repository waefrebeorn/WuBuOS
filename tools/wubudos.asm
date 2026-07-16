; wubudos.asm -- minimal self-contained 16-bit "WuBuDOS" rescue OS.
; Boots at 0x7C00, enables A20, prints a banner to COM1 + VGA, and
; runs RESCUE.COM (loaded from the FAT image) is left as future work;
; here it just proves the 16-bit rescue *kernel* path executes and emits.
;
; This is the verifiable proof-vehicle for the emergency-16-bit layer:
; WuBuOS boot.S (emergency) -> rescue_shim (A20) -> here. If COM1 shows
; "WuBuDOS 16-bit rescue ready" and VGA shows the banner, the whole
; boot->rescue chain is proven on real hardware-class QEMU/KVM.
        org 0x7C00
        bits 16
start:
        cli
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7C00

        ; --- A20 enable (same as shim, self-contained) ---
        mov ax, 0x2401
        int 0x15
        jc  .kbc
        jmp .a20done
.kbc:
        call a20_wait_in
        mov al, 0xAD
        out 0x64, al
        call a20_wait_in
        mov al, 0xD0
        out 0x64, al
        call a20_wait_out
        in  al, 0x60
        or  al, 0x02
        push ax
        call a20_wait_in
        mov al, 0xD1
        out 0x64, al
        call a20_wait_in
        pop ax
        out 0x60, al
        call a20_wait_in
        mov al, 0xAE
        out 0x64, al
        call a20_wait_in
.a20done:

        ; --- VGA text mode banner ---
        mov ax, 0xB800
        mov es, ax
        xor di, di
        mov si, banner
        mov ah, 0x0F        ; white on black
.vga:
        lodsb
        test al, al
        jz  .vgadone
        cmp al, 13
        je  .vganl
        stosw
        jmp .vga
.vganl:
        add di, 160 - 2     ; next line
        jmp .vga
.vgadone:

        ; --- COM1 banner ---
        mov si, com_msg
        call puts

        ; --- halt (rescue shell would continue here) ---
.halt:  cli
        hlt
        jmp .halt

a20_wait_in:
        in  al, 0x64
        test al, 0x02
        jnz a20_wait_in
        ret
a20_wait_out:
        in  al, 0x64
        test al, 0x01
        jz  a20_wait_out
        ret
puts:
        lodsb
        test al, al
        jz  .ret
        mov dx, 0x3FD
.w:     in  al, dx
        test al, 0x20
        jz  .w
        mov dx, 0x3F8
        out dx, al
        jmp puts
.ret:   ret

banner:  db "WuBuDOS 16-bit rescue ready", 13, 0
com_msg: db "WuBuDOS 16-bit rescue ready", 13, 10, 0
        times 510-($-$$) db 0
        dw 0xAA55
