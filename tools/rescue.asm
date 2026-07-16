; rescue.asm -- tiny 16-bit COM that prints "RESCUE OK\r\n" to COM1 (0x3F8)
; and then halts. Used by AUTOEXEC.BAT on the FreeDOS rescue floppy so the
; WuBuOS boot test can confirm the 16-bit layer actually executed.
;
; Assemble: nasm -f bin -o rescue.com rescue.asm
        org 0x100

        mov cx, 11          ; 11 chars including CR/LF
        mov si, msg
pchar:
        ; wait for THR empty (LSR bit 5 @ 0x3FD)
        mov dx, 0x3FD
w:
        in al, dx
        test al, 0x20
        jz w
        mov dx, 0x3F8
        mov al, [si]
        out dx, al
        inc si
        loop pchar

        cli
h:      hlt
        jmp h

msg:    db "RESCUE OK", 13, 10
