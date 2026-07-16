; rescue_shim.asm -- 512-byte real-mode shim that enables the A20 gate,
; then loads the genuine FreeDOS BOOT.BIN (stored at LBA 1 on the rescue
; image) into 0x0000:0x7C00 and jumps to it.
;
; Why: FreeDOS's 2026 kernel hangs early unless A20 is enabled before it
; runs. SeaBIOS leaves A20 in an undefined state; this shim guarantees it.
;
; Layout of the WuBuDOS rescue FAT image:
;   LBA 0 = this shim (booted by WuBuOS boot.S when in emergency mode)
;   LBA 1 = FreeDOS BOOT.BIN (the real FreeDOS boot sector)
;   LBA 2+ = FAT12 filesystem (KERNEL.SYS at cluster 2, etc.)
;
; Assemble: nasm -f bin -o rescue_shim.bin rescue_shim.asm

        org 0x7C00
        bits 16

start:
        cli
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7C00

        ; Print marker to COM1 so the serial test can confirm the shim ran.
        mov si, msg_shim
        call puts

        ; --- Enable A20 ---
        ; Method 1: BIOS (clean, works under SeaBIOS/QEMU).
        mov ax, 0x2401
        int 0x15
        jc  .kbca20
        call a20_check
        jz  .a20ok

.kbca20:
        ; Method 2: keyboard controller (8042).
        call a20_wait_in
        mov al, 0xAD        ; disable keyboard
        out 0x64, al
        call a20_wait_in
        mov al, 0xD0        ; read output port
        out 0x64, al
        call a20_wait_out
        in  al, 0x60
        or  al, 0x02        ; set A20 bit
        push ax
        call a20_wait_in
        mov al, 0xD1        ; write output port
        out 0x64, al
        call a20_wait_in
        pop ax
        out 0x60, al
        call a20_wait_in
        mov al, 0xAE        ; re-enable keyboard
        out 0x64, al
        call a20_wait_in
        call a20_check
        jz  .a20ok

        ; A20 still off -- proceed anyway (some envs don't need it).
.a20ok:
        mov si, msg_a20
        call puts

        ; --- Load FreeDOS BOOT.BIN (LBA 1) into 0x7C00 ---
        mov si, dap
        mov ah, 0x42
        int 0x13
        jc  .diskerr

        ; Jump to the FreeDOS boot sector (real mode, far jmp to 0:0x7C00).
        jmp 0x0000:0x7C00

.diskerr:
        mov si, msg_err
        call puts
.hlt:   cli
        hlt
        jmp .hlt

; ---- A20 helpers ----
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
a20_check:
        ; Returns ZF=1 if A20 enabled (wrap test).
        pushf
        push es
        xor ax, ax
        mov es, ax
        mov ax, 0xFFFF
        mov gs, ax
        mov bx, 0x0510      ; linear 0x00000510 (below 1MB)
        mov ax, [es:bx]
        mov cx, ax
        not  ax
        mov [gs:bx+0x10], ax   ; linear 0x00100520 (wraps to 0x00000520 if A20 off)
        mov ax, [es:bx]
        cmp ax, cx
        mov [es:bx], cx         ; restore
        pop es
        popf
        ret

; ---- Serial helper (COM1 @ 0x3F8) ----
puts:
        lodsb
        test al, al
        jz  .ret
        push si
.w:     mov dx, 0x3FD
        in  al, dx
        test al, 0x20
        jz  .w
        pop si
        mov dx, 0x3F8
        out dx, al
        jmp puts
.ret:   ret

msg_shim: db "WuBuDOS A20 shim -> FreeDOS", 13, 10, 0
msg_a20:  db "A20 ok", 13, 10, 0
msg_err:  db "RESCUE DISK ERR", 13, 10, 0

; ---- DAP for int 0x13 AH=0x42 (load LBA 1) ----
dap:
        db 0x10
        db 0
        dw 1               ; 1 sector
        dw 0x7C00          ; dest offset
        dw 0x0000          ; dest segment (0:0x7C00)
        dq 1               ; LBA 1 (the FreeDOS BOOT.BIN)

        times 510-($-$$) db 0
        dw 0xAA55
