#include "wubu_dos_emu.h"
#include <stdio.h>
#include <string.h>

/* Build a tiny .COM by hand:
 *   mov ah, 0x09      ; DOS print '$'-terminated string
 *   mov dx, msg       ; offset of msg (computed after we know code size)
 *   int 0x21
 *   mov ax, 0x4C00    ; exit 0
 *   int 0x21
 * msg: db "HELLO DOS!$"
 * The code sits at CS:0x100; the string follows immediately.
 */
static uint8_t *build_com(size_t *out_size) {
    static uint8_t buf[256];
    int p = 0;
    buf[p++] = 0xB4; buf[p++] = 0x09;            /* mov ah,9 */
    buf[p++] = 0xBA;                             /* mov dx,imm16 */
    int immpos = p; buf[p++] = 0; buf[p++] = 0; /* placeholder */
    buf[p++] = 0xCD; buf[p++] = 0x21;           /* int 21 */
    buf[p++] = 0xB8; buf[p++] = 0x00; buf[p++] = 0x4C; /* mov ax,4C00 */
    buf[p++] = 0xCD; buf[p++] = 0x21;           /* int 21 */
    int codesize = p;
    int msg_off = 0x100 + codesize;
    buf[immpos] = (uint8_t)(msg_off & 0xFF);
    buf[immpos + 1] = (uint8_t)(msg_off >> 8);
    const char *msg = "HELLO DOS!$";
    for (int i = 0; msg[i]; i++) buf[p++] = (uint8_t)msg[i];
    buf[p++] = '$';
    *out_size = p;
    return buf;
}

int main(void) {
    size_t sz = 0;
    uint8_t *com = build_com(&sz);
    WubuDosEmu *e = wubu_dos_emu_create();
    if (wubu_dos_emu_load_com(e, com, sz) != 0) { printf("load failed\n"); return 1; }
    WubuDosEmuState st = wubu_dos_emu_run(e, 1000000);
    printf("state=%d exit=%d\n", st, wubu_dos_emu_exit_code(e));
    char text[4096];
    size_t n = wubu_dos_emu_text(e, text, sizeof(text));
    printf("text(%zu):\n%s\n", n, text);
    int found = (strstr(text, "HELLO DOS!") != NULL);
    int ok = (st == WUBU_DOS_TERMINATED) && (wubu_dos_emu_exit_code(e) == 0) && found;
    printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    wubu_dos_emu_destroy(e);
    return ok ? 0 : 1;
}
