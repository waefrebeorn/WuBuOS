/*
 * wubu_vga.h -- raw VGA text mode (the HolyC console surface).
 *
 * The VGA text buffer is 80x25 cells of (character, attribute), where the
 * attribute byte packs the FOREGROUND in the low nibble and the BACKGROUND in
 * the high nibble.  Those two nibbles are the classic swap bug, and everything
 * else here is about not losing or inventing a cell:
 *
 *   * The cursor WRAPS at the right margin, and SCROLLS at the bottom.
 *     Scrolling moves every row up and CLEARS the last row -- it does not
 *     rotate, and it does not lose the top row silently.
 *   * '\n' moves to COLUMN 0 and advances the row.  '\r' moves to column 0
 *     WITHOUT advancing.  Confusing the two makes every line overwrite the
 *     last.
 *   * '\b' moves the cursor back ONE CELL and does not erase.  Erasing on
 *     backspace is a surprise for a terminal emulator.
 *   * '\t' advances to the next multiple of 8 -- a hardware tab stop, not a
 *     fixed four spaces.
 *   * A control character other than \n, \r, \b and \t is IGNORED, not
 *     written: a NUL or a bell in a string is data, not a cell.
 *   * Writing past the last cell SCROLLS rather than dropping the character.
 *   * The attribute nibbles are fg = attr & 0x0F, bg = attr >> 4.  A swapped
 *     pair makes white-on-black render as black-on-white with no error.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VGA_H
#define WUBU_VGA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WB_VGA_COLS  80
#define WB_VGA_ROWS  25
#define WB_VGA_CELLS (WB_VGA_COLS * WB_VGA_ROWS)
#define WB_VGA_TAB   8

#define WB_VGA_BLACK   0u
#define WB_VGA_BLUE    1u
#define WB_VGA_GREEN   2u
#define WB_VGA_CYAN    3u
#define WB_VGA_RED     4u
#define WB_VGA_MAGENTA 5u
#define WB_VGA_BROWN   6u
#define WB_VGA_LGRAY   7u
#define WB_VGA_DGRAY   8u
#define WB_VGA_LBLUE   9u
#define WB_VGA_LGREEN  10u
#define WB_VGA_LCYAN   11u
#define WB_VGA_LRED    12u
#define WB_VGA_LMAGENTA 13u
#define WB_VGA_YELLOW  14u
#define WB_VGA_WHITE   15u

typedef struct {
    uint8_t ch;
    uint8_t attr;
} wb_vga_cell_t;

typedef struct {
    wb_vga_cell_t cells[WB_VGA_ROWS][WB_VGA_COLS];
    uint32_t      cursor_x;
    uint32_t      cursor_y;
    uint8_t       attr;
    uint32_t      scroll_count;
} wb_vga_console_t;

void wb_vga_console_init(wb_vga_console_t *c);

/* The attribute nibbles. */
uint8_t wb_vga_fg(uint8_t attr);
uint8_t wb_vga_bg(uint8_t attr);
uint8_t wb_vga_make_attr(uint8_t fg, uint8_t bg);

/* Write one byte.  A control character other than the four handled ones is
 * IGNORED. */
void wb_vga_console_putc(wb_vga_console_t *c, char ch);
void wb_vga_console_puts(wb_vga_console_t *c, const char *s);
void wb_vga_console_clear(wb_vga_console_t *c);

/* The cell at a position, for a capture. */
wb_vga_cell_t wb_vga_console_cell(const wb_vga_console_t *c, uint32_t x,
                                  uint32_t y);

#endif /* WUBU_VGA_H */
