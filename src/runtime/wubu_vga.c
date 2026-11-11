/*
 * wubu_vga.c -- raw VGA text mode (the HolyC console surface).
 *
 * See wubu_vga.h.
 */
#include "wubu_vga.h"

#include <string.h>

uint8_t wb_vga_fg(uint8_t attr) { return attr & 0x0F; }
uint8_t wb_vga_bg(uint8_t attr) { return (uint8_t)(attr >> 4); }

uint8_t wb_vga_make_attr(uint8_t fg, uint8_t bg)
{
    /* The two nibbles are a PACKED PAIR: foreground in the low nibble,
     * background in the high.  Swapping them makes white-on-black render as
     * black-on-white with no error. */
    return (uint8_t)((fg & 0x0F) | ((bg & 0x0F) << 4));
}

void wb_vga_console_init(wb_vga_console_t *c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->attr = wb_vga_make_attr(WB_VGA_LGRAY, WB_VGA_BLACK);
    /* A real VGA buffer powers up as spaces with the default attribute. */
    for (uint32_t y = 0; y < WB_VGA_ROWS; y++)
        for (uint32_t x = 0; x < WB_VGA_COLS; x++) {
            c->cells[y][x].ch = ' ';
            c->cells[y][x].attr = c->attr;
        }
}

static void scroll(wb_vga_console_t *c)
{
    /* Move every row up by one and CLEAR the last row.  The top row is lost --
     * that is what scrolling means -- but nothing else is. */
    for (uint32_t y = 1; y < WB_VGA_ROWS; y++)
        memcpy(c->cells[y - 1], c->cells[y], sizeof(c->cells[0]));
    for (uint32_t x = 0; x < WB_VGA_COLS; x++) {
        c->cells[WB_VGA_ROWS - 1][x].ch = ' ';
        c->cells[WB_VGA_ROWS - 1][x].attr = c->attr;
    }
    c->cursor_y = WB_VGA_ROWS - 1;
    c->scroll_count++;
}

static void advance(wb_vga_console_t *c)
{
    c->cursor_x++;
    /* WRAP at the right margin, and SCROLL at the bottom. */
    if (c->cursor_x >= WB_VGA_COLS) {
        c->cursor_x = 0;
        c->cursor_y++;
        if (c->cursor_y >= WB_VGA_ROWS) scroll(c);
    }
}

void wb_vga_console_putc(wb_vga_console_t *c, char ch)
{
    if (!c) return;

    switch (ch) {
    case '\n':
        /* COLUMN 0 and ADVANCE the row -- distinct from '\r'. */
        c->cursor_x = 0;
        c->cursor_y++;
        if (c->cursor_y >= WB_VGA_ROWS) scroll(c);
        return;
    case '\r':
        /* COLUMN 0 WITHOUT advancing. */
        c->cursor_x = 0;
        return;
    case '\b':
        /* BACK ONE CELL, and do NOT erase. */
        if (c->cursor_x > 0) c->cursor_x--;
        return;
    case '\t':
        /* The next hardware tab stop: a multiple of 8, not four spaces. */
        c->cursor_x = (c->cursor_x / WB_VGA_TAB + 1) * WB_VGA_TAB;
        if (c->cursor_x >= WB_VGA_COLS) {
            c->cursor_x = 0;
            c->cursor_y++;
            if (c->cursor_y >= WB_VGA_ROWS) scroll(c);
        }
        return;
    default:
        break;
    }

    /* Any OTHER control character is IGNORED, not written: a NUL or a bell in
     * a string is data, not a cell. */
    if ((unsigned char)ch < 0x20) return;

    c->cells[c->cursor_y][c->cursor_x].ch = (uint8_t)ch;
    c->cells[c->cursor_y][c->cursor_x].attr = c->attr;
    advance(c);
}

void wb_vga_console_puts(wb_vga_console_t *c, const char *s)
{
    if (!c || !s) return;
    for (const char *p = s; *p; p++) wb_vga_console_putc(c, *p);
}

void wb_vga_console_clear(wb_vga_console_t *c)
{
    if (!c) return;
    uint8_t attr = c->attr;
    uint32_t sx = c->cursor_x, sy = c->cursor_y;
    uint32_t sc = c->scroll_count;
    wb_vga_console_init(c);
    /* a clear keeps the current attribute and the scroll history */
    c->attr = attr;
    c->scroll_count = sc;
    (void)sx; (void)sy;
}

wb_vga_cell_t wb_vga_console_cell(const wb_vga_console_t *c, uint32_t x,
                                  uint32_t y)
{
    wb_vga_cell_t empty;
    empty.ch = ' ';
    empty.attr = 0;
    if (!c || x >= WB_VGA_COLS || y >= WB_VGA_ROWS) return empty;
    return c->cells[y][x];
}
