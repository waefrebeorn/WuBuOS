/*
 * fw_con.c  --  WuBuFW EFI_SIMPLE_TEXT_{INPUT,OUTPUT}_PROTOCOL thunks.
 *
 * These are the ms_abi wrappers over the raw serial/VGA console in fw_lib.c.
 */

#include "fw.h"

void fw_vga_clear(void);
void fw_vga_set_attr(uint8_t a);
void fw_vga_set_pos(int row, int col);
void fw_vga_get_pos(int *row, int *col);

static SIMPLE_TEXT_OUTPUT_MODE g_outmode = { 1, 0, 0x0F, 0, 0, TRUE };

static EFI_STATUS EFIAPI out_reset(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, BOOLEAN ext) {
    (void)This; (void)ext;
    fw_vga_clear();
    g_outmode.CursorColumn = g_outmode.CursorRow = 0;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI out_string(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *s) {
    (void)This;
    if (!s) return EFI_INVALID_PARAMETER;
    for (; *s; s++) fw_putc(*s < 0x80 ? (char)*s : '?');
    fw_vga_get_pos(&g_outmode.CursorRow, &g_outmode.CursorColumn);
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI out_test(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *s) {
    (void)This; (void)s;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI out_query(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN mode,
                                   UINTN *cols, UINTN *rows) {
    (void)This;
    if (mode != 0) return EFI_UNSUPPORTED;
    if (cols) *cols = 80;
    if (rows) *rows = 25;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI out_setmode(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN mode) {
    (void)This;
    return mode == 0 ? EFI_SUCCESS : EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI out_attr(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN a) {
    (void)This;
    g_outmode.Attribute = (INT32)a;
    fw_vga_set_attr((uint8_t)(a & 0x7F));
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI out_clear(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This) {
    (void)This;
    fw_vga_clear();
    g_outmode.CursorColumn = g_outmode.CursorRow = 0;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI out_setpos(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN c, UINTN r) {
    (void)This;
    if (c >= 80 || r >= 25) return EFI_UNSUPPORTED;
    fw_vga_set_pos((int)r, (int)c);
    g_outmode.CursorColumn = (INT32)c;
    g_outmode.CursorRow = (INT32)r;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI out_cursor(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, BOOLEAN v) {
    (void)This;
    g_outmode.CursorVisible = v;
    return EFI_SUCCESS;
}

EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL g_conout = {
    out_reset, out_string, out_test, out_query, out_setmode,
    out_attr, out_clear, out_setpos, out_cursor, &g_outmode
};

/* -- input ---------------------------------------------------------- */

static EFI_STATUS EFIAPI in_reset(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, BOOLEAN ext) {
    (void)This; (void)ext;
    while (fw_getc_nb() >= 0) { }
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI in_read(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *key) {
    (void)This;
    if (!key) return EFI_INVALID_PARAMETER;
    int c = fw_getc_nb();
    if (c < 0) return EFI_NOT_READY;
    key->ScanCode = 0;
    if (c & 0x100) {                       /* extended scancode */
        switch (c & 0xFF) {
        case 0x48: key->ScanCode = 0x01; break;   /* up    */
        case 0x50: key->ScanCode = 0x02; break;   /* down  */
        case 0x4B: key->ScanCode = 0x04; break;   /* left  */
        case 0x4D: key->ScanCode = 0x03; break;   /* right */
        default:   return EFI_NOT_READY;
        }
        key->UnicodeChar = 0;
        return EFI_SUCCESS;
    }
    if (c == 27) { key->ScanCode = 0x17; key->UnicodeChar = 0; return EFI_SUCCESS; }
    if (c == '\r') c = '\n';
    key->UnicodeChar = (CHAR16)c;
    return EFI_SUCCESS;
}

/* WaitForKey is a poll-backed event; CheckEvent/WaitForEvent in the boot
 * services layer special-case this handle and poll the hardware. */
EFI_EVENT g_wait_for_key = (EFI_EVENT)(uintptr_t)0xE0FE0001ULL;

EFI_SIMPLE_TEXT_INPUT_PROTOCOL g_conin = { in_reset, in_read, NULL };
