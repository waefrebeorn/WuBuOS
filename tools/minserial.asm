; minserial.asm -- minimal boot that prints "HELLO" to COM1 in a loop.
; Verify the agent's QEMU serial capture works at all.
        org 0x7C00
        bits 16
start:
        cli
        xor ax, ax
        mov ds, ax
        mov ss, ax
        mov sp, 0x7C00
        mov si, msg
.loop:
        lodsb
        test al, al
        jz .done
        mov dx, 0x3FD
.w:     in al, dx
        test al, 0x20
        jz .w
        mov dx, 0x3F8
        out dx, al
        jmp .loop
.done:
        cli
        hlt
        jmp .done
msg:    db "HELLO WUBU", 13, 10, 0
        times 510-($-$$) db 0
        dw 0xAA55
