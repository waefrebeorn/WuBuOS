; cpm_stat.asm -- CP/M 2.2 era demo (1974)  [EMULATOR GAP -- not runnable yet]
;
; This is the CP/M 2.2 "STAT" style demo WuBuOS *would* run through the
; 0xC0 BDOS personality (vsl_cpm_syscall_dispatch). It prints a banner via
; BDOS function 9 (console string output) and would exercise the CP/M
; personality's real host-file effects (select disk / create / write / read
; sequential, already implemented in vsl_syscall_cpm.c).
;
; WHY IT IS A GAP (honest):
;   * WuBuOS has the CP/M BDOS SYSCALL PERSONALITY (real host effects), but
;     NO 8080 CPU EMULATOR. A .COM is 8080 machine code; without an 8080
;     interpreter to fetch/decode/execute it, the binary cannot run.
;   * No 8080 assembler is installed on this host to even emit the .COM.
;   * Building this requires: (1) an 8080 interpreter in wubu_dos_emu-class
;     module, (2) a CP/M .COM loader, (3) an 8080 assembler (or hand-encoded
;     opcodes). See docs/ERA_APPS_AND_RESOLVE_GAPS.md "CP/M personality".
;
; The 8080 opcodes for the workload below (for when the emu lands):
;   BDOS call = execute at 0x0005 vector (JP into BDOS).
;   Print $-string:  load DE=addr, MVI C,9, CALL 5.
;   Exit:           MVI C,0, CALL 5.
;
; Placeholder encoding (BDOS print of "WuBuOS CP/M era\n$"):
;   0x21 <lo> <hi>   ; LXI H, msg
;   0x11 <lo> <hi>   ; LXI D, msg
;   0x0E 0x09        ; MVI C, 9
;   0xCD 0x05 0x00   ; CALL 5   (BDOS)
;   0x0E 0x00        ; MVI C, 0
;   0xCD 0x05 0x00   ; CALL 5   (BDOS terminate)
;   msg: "WuBuOS CP/M era\r\n$"
;
; The WuBuOS VSL router maps class 0xC0 + BDOS fn (bits 15..8) to
; vsl_cpm_syscall_dispatch(); once an 8080 interpreter exists, this binary
; will run end-to-end through that router.

        org 0x0100

start:
        lxi  h, msg
        lxi  d, msg
        mvi  c, 9
        call 5            ; BDOS 9: print $-terminated string
        mvi  c, 0
        call 5            ; BDOS 0: terminate

msg:    db   "WuBuOS CP/M era", 13, 10, "$"
