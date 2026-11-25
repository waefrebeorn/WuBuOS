/* wubu_x11_windows_test.c -- ids, bounds, and pixels that survive the trip out. */
#include "wubu_x11_windows.h"

#include <stdio.h>
#include <string.h>

static WubuX11Windows table;
static int failures;
#define CHECK(c, m) do { if (c) printf("  [PASS] %s\n", m); else { printf("  [FAIL] %s\n", m); failures++; } } while (0)

int main(void) {
    printf("x11 windows\n");
    wubu_x11_windows_init(&table);
    CHECK(table.count == 0, "an empty table has no windows");

    CHECK(wubu_x11_window_create(&table, 0x400001, 10, 20, 64, 48) == 0, "root window is created");
    CHECK(table.count == 1, "one window in the table");

    /* a client that asks twice for the same id is refused, not silently duplicated */
    CHECK(wubu_x11_window_create(&table, 0x400001, 0, 0, 8, 8) < 0, "duplicate id refused");
    /* bounded pool: a window larger than the whole pool is refused */
    CHECK(wubu_x11_window_create(&table, 0x400002, 0, 0, 4096, 4096) < 0, "oversized window refused");
    CHECK(wubu_x11_window_create(&table, 0x400003, 0, 0, 0, 8) < 0, "zero-size window refused");

    uint8_t img[4 * 4];
    /* A little-endian CARD32 0x00RRGGBB arrives as bytes BB GG RR 00 -- so red is
     * 00 00 ff 00, not ff 00 00 00. Writing the bytes in the order the VALUE reads
     * sends blue and then fails a test that expected red. */
    for (int i = 0; i < 4; i++) { img[i*4+0]=0x00; img[i*4+1]=0x00; img[i*4+2]=0xff; img[i*4+3]=0x00; }
    CHECK(wubu_x11_window_put_image(&table, 0x400001, 2, 3, 2, 2, img, sizeof img, 1, 24) == 0,
          "PutImage into the named window");
    CHECK(wubu_x11_window_put_image(&table, 0xDEAD, 0, 0, 1, 1, img, 4, 1, 24) < 0,
          "PutImage into an unknown window is refused");

    /* present: the pixels must arrive where the compositor expects them */
    uint8_t out[64 * 48 * 4];
    CHECK(wubu_x11_window_present(&table, 0x400001, out, sizeof out, 64, 48) == 0, "present succeeds");
    size_t off = ((size_t)3 * 64 + 2) * 4;
    CHECK(out[off] == 0xff && out[off + 1] == 0x00 && out[off + 2] == 0x00,
          "presented pixel is the red the client sent");
    CHECK(!wubu_x11_surface_dirty(&wubu_x11_window_find(&table, 0x400001)->surface),
          "presentation consumed the damage");

    /* a buffer too small must be refused, never handed a truncated frame */
    uint8_t small[16];
    CHECK(wubu_x11_window_present(&table, 0x400001, small, sizeof small, 64, 48) < 0,
          "present refuses a buffer it cannot fill");

    CHECK(wubu_x11_window_map(&table, 0x400001, 1) == 0, "window maps");
    CHECK(wubu_x11_window_find(&table, 0x400001)->mapped == 1, "map state is recorded");
    CHECK(wubu_x11_window_destroy(&table, 0x400001) == 0, "window destroys");
    CHECK(table.count == 0 && !wubu_x11_window_find(&table, 0x400001), "table is empty again");

    printf("x11 windows: %s (%d failure(s))\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
