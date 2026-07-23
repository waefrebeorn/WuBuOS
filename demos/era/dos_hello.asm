; dos_hello.asm -- MS-DOS 2.0 era demo (1981)
; Real 16-bit .COM: prints a banner via INT 21h/AH=09 (DOS print $-string)
; and writes a marker file via INT 21h/AH=3C/40/3E (create/write/close).
; Runs INSIDE WuBuOS through the in-process 8086 interpreter
; (wubu_dos_emu -> wubu_dos_proc -> wubu_exec_dos), exercising the DOS
; INT 21h personality. No second OS is booted.
;
; Assemble: nasm -f bin dos_hello.asm -o dos_hello.com

        org 0x100

start:
        ; --- print banner via DOS INT 21h/09 ---
        mov dx, banner
        mov ah, 0x09
        int 0x21

        ; --- create marker file C:\WUBU_ERA_DOS.OK (INT 21h/3C) ---
        mov dx, fname
        mov cx, 0x00           ; normal attribute
        mov ah, 0x3C
        int 0x21
        jc  done               ; carry set on error -> skip
        mov [handle], ax       ; save handle

        ; --- write marker bytes (INT 21h/40) ---
        mov bx, [handle]
        mov dx, fdata
        mov cx, fdata_len
        mov ah, 0x40
        int 0x21

        ; --- close (INT 21h/3E) ---
        mov bx, [handle]
        mov ah, 0x3E
        int 0x21

done:
        ; --- exit (INT 21h/4C) ---
        mov ax, 0x4C00
        int 0x21

section .data
banner  db "WuBuOS DOS era: hello from the 8086 compatible window!$", 13, 10
fname   db "WUBU_ERA_DOS.OK", 0
fdata   db "DOS INT 21h personality exercised by WuBuOS", 13, 10
fdata_len equ $ - fdata

section .bss
handle  resw 1
