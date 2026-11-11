/*
 * wubu_vga_test.c -- the raw VGA text console.
 *
 * The load-bearing rules: '\n' moves to column 0 AND advances while '\r' moves
 * to column 0 WITHOUT advancing (confusing them makes every line overwrite the
 * last), '\b' moves back WITHOUT erasing, '\t' goes to the next multiple of 8,
 * and the attribute nibbles are fg in the LOW nibble and bg in the HIGH.
 */
#include "wubu_vga.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_wubu_vga ===\n");
    wb_vga_console_t c;

    /* -- the attribute nibbles -- */
    CHECK(wb_vga_fg(0x07) == 7, "the foreground is the LOW nibble");
    CHECK(wb_vga_bg(0x07) == 0, "the background is the HIGH nibble");
    CHECK(wb_vga_fg(0x1F) == 15 && wb_vga_bg(0x1F) == 1,
          "white-on-blue is 0x1F, not 0xF1 (the classic swap)");
    CHECK(wb_vga_make_attr(WB_VGA_WHITE, WB_VGA_BLUE) == 0x1F,
          "building the attribute packs fg low and bg high");
    CHECK(wb_vga_make_attr(0xFF, 0xFF) == 0xFF, "and masks both nibbles");

    /* -- init -- */
    wb_vga_console_init(&c);
    CHECK(c.cursor_x == 0 && c.cursor_y == 0, "the cursor starts at 0,0");
    CHECK(c.scroll_count == 0, "nothing has scrolled");
    {
        wb_vga_cell_t cell = wb_vga_console_cell(&c, 0, 0);
        CHECK(cell.ch == ' ', "the buffer powers up as spaces");
        CHECK(wb_vga_fg(cell.attr) == WB_VGA_LGRAY &&
              wb_vga_bg(cell.attr) == WB_VGA_BLACK,
              "with the default light-gray-on-black attribute");
    }

    /* -- writing -- */
    wb_vga_console_puts(&c, "Hi");
    CHECK(wb_vga_console_cell(&c, 0, 0).ch == 'H' &&
          wb_vga_console_cell(&c, 1, 0).ch == 'i', "the text is written");
    CHECK(c.cursor_x == 2, "and the cursor advanced");

    /* -- '\r' vs '\n' -- */
    wb_vga_console_putc(&c, '\r');
    CHECK(c.cursor_x == 0 && c.cursor_y == 0,
          "'\\r' moves to column 0 WITHOUT advancing");
    wb_vga_console_puts(&c, "AB");
    wb_vga_console_putc(&c, '\n');
    CHECK(c.cursor_x == 0 && c.cursor_y == 1,
          "'\\n' moves to column 0 AND advances");
    CHECK(wb_vga_console_cell(&c, 0, 0).ch == 'A' &&
          wb_vga_console_cell(&c, 1, 0).ch == 'B',
          "the previous line is intact");

    /* -- '\b' moves back WITHOUT erasing -- */
    wb_vga_console_puts(&c, "XY");
    wb_vga_console_putc(&c, '\b');
    CHECK(c.cursor_x == 1, "'\\b' moves back one cell");
    CHECK(wb_vga_console_cell(&c, 1, 1).ch == 'Y',
          "and does NOT erase");

    /* -- '\t' goes to the next multiple of 8 -- */
    wb_vga_console_init(&c);
    c.cursor_x = 1;
    wb_vga_console_putc(&c, '\t');
    CHECK(c.cursor_x == 8, "'\\t' advances to the next multiple of 8");
    c.cursor_x = 8;
    wb_vga_console_putc(&c, '\t');
    CHECK(c.cursor_x == 16, "and again");
    c.cursor_x = 78;
    wb_vga_console_putc(&c, '\t');
    CHECK(c.cursor_x == 0 && c.cursor_y == 1,
          "a tab past the margin wraps to the next line");

    /* -- wrapping at the right margin -- */
    wb_vga_console_init(&c);
    for (int i = 0; i < WB_VGA_COLS; i++) wb_vga_console_putc(&c, 'x');
    CHECK(c.cursor_x == 0 && c.cursor_y == 1,
          "after exactly 80 columns the cursor wraps to the next line");
    for (int i = 0; i < WB_VGA_COLS; i++) wb_vga_console_putc(&c, 'y');
    CHECK(wb_vga_console_cell(&c, 0, 0).ch == 'x' &&
          wb_vga_console_cell(&c, 79, 1).ch == 'y', "both rows hold their text");

    /* -- scrolling at the bottom -- */
    wb_vga_console_init(&c);
    for (int i = 0; i < WB_VGA_ROWS * WB_VGA_COLS; i++)
        wb_vga_console_putc(&c, 'a');
    CHECK(c.scroll_count == 1, "writing one screen past the end scrolls once");
    CHECK(c.cursor_y == WB_VGA_ROWS - 1, "and the cursor is on the last row");
    /* the top row now holds the SECOND screenful */
    CHECK(wb_vga_console_cell(&c, 0, 0).ch == 'a', "the rows moved up");

    /* -- a control character is ignored, not written -- */
    wb_vga_console_init(&c);
    wb_vga_console_putc(&c, '\0');
    wb_vga_console_putc(&c, '\a');
    wb_vga_console_putc(&c, '\x1B');
    CHECK(c.cursor_x == 0 && c.cursor_y == 0,
          "a NUL, a bell and an ESC are IGNORED, not written");
    wb_vga_console_puts(&c, "Z");
    CHECK(wb_vga_console_cell(&c, 0, 0).ch == 'Z', "and the real text lands");

    /* -- clear -- */
    wb_vga_console_init(&c);
    wb_vga_console_puts(&c, "hello");
    wb_vga_console_clear(&c);
    CHECK(c.cursor_x == 0 && c.cursor_y == 0, "clear resets the cursor");
    CHECK(wb_vga_console_cell(&c, 0, 0).ch == ' ', "and blanks the buffer");
    CHECK(wb_vga_console_cell(&c, 4, 0).ch == ' ', "everywhere");

    /* -- out-of-range reads are safe -- */
    {
        wb_vga_cell_t cell = wb_vga_console_cell(&c, 999, 999);
        CHECK(cell.ch == ' ', "an out-of-range read returns a blank cell");
        cell = wb_vga_console_cell(NULL, 0, 0);
        CHECK(cell.ch == ' ', "and a null console too");
        wb_vga_console_init(NULL);
        wb_vga_console_putc(NULL, 'x');
        wb_vga_console_puts(NULL, "x");
        wb_vga_console_clear(NULL);
    }

    if (failures == 0) printf("ALL WUBU_VGA TESTS PASSED\n");
    else printf("%d WUBU_VGA FAILURES\n", failures);
    return failures ? 1 : 0;
}
