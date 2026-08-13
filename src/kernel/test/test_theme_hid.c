/* test_theme_hid.c -- host tests for the /theme namespace + unified HID.
 * Builds the two freestanding kernel modules with a tiny host shim. */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

/* host shim: the kernel tick accessor */
static uint64_t fake_tick = 0;
uint64_t task_tick_count(void) { return fake_tick; }

/* shims the modules reference */
int snprintf(char *s, size_t n, const char *fmt, ...);

#define WUBU_THEME_STANDALONE
#include "wubu_theme.h"
#include "wubu_theme.c"

#include "wubu_hid.h"
#include "wubu_hid.c"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

int main(void)
{
    /* ---- /theme namespace ---- */
    wubu_theme_init();
    CHECK(wubu_theme_write_count() >= 18);          /* preset seeded nodes */

    const WubuKTheme *t = wubu_theme_get();
    CHECK(t->desktop_bg == 0x00808080u);            /* WIN98 preset */
    CHECK(t->rounded_buttons == false);

    /* write a node + apply -> the draw struct changes (self-modifying) */
    CHECK(wubu_theme_node_set("/theme/win/title_active", 0x123456u) == 0);
    CHECK(wubu_theme_node_set("/theme/nope", 1) == -1);   /* unknown path */
    uint32_t v = 0;
    CHECK(wubu_theme_node_get("/theme/win/title_active", &v) == 0 && v == 0x123456u);
    wubu_theme_apply();
    t = wubu_theme_get();
    CHECK(t->win_title_active == 0x123456u);

    /* preset switching re-seeds */
    CHECK(wubu_theme_preset(1) == 0);               /* LUNA */
    t = wubu_theme_get();
    CHECK(t->desktop_bg == 0x003A6EA5u);
    CHECK(t->rounded_buttons == true);
    CHECK(t->gradient_title == true);

    /* node list renders + terminates */
    char buf[1024];
    int n = wubu_theme_node_list(buf, (int)sizeof(buf));
    CHECK(n >= 18);
    CHECK(strstr(buf, "/theme/win/title_active") != NULL);

    /* ---- unified HID ---- */
    wubu_hid_init();
    fake_tick = 100;
    wubu_hid_feed_key('A', true, 0);
    wubu_hid_feed_mouse(10, 20, 1, 2, 1, 0);
    int16_t axes[2] = { 0, -32767 };
    wubu_hid_feed_gamepad(0x1u, axes, 2);

    WubuInputEvent ev;
    CHECK(wubu_hid_poll(&ev) == 1 && ev.device == WUBU_DEV_KEYBOARD);
    CHECK(ev.u.key.keycode == 'A' && ev.u.key.down == true);
    CHECK(ev.ts_ms == 1000u);                       /* common time base */
    CHECK(wubu_hid_poll(&ev) == 1 && ev.device == WUBU_DEV_MOUSE);
    CHECK(wubu_hid_poll(&ev) == 1 && ev.device == WUBU_DEV_GAMEPAD);
    CHECK(ev.u.pad.buttons == 0x1u && ev.u.pad.axes[1] == -32767);
    CHECK(wubu_hid_poll(&ev) == 0);                 /* drained */

    /* per-device master switch: disable the mouse entirely */
    wubu_hid_disable(WUBU_DEV_MOUSE, true);
    wubu_hid_feed_mouse(0, 0, 0, 0, 0, 0);
    wubu_hid_feed_key('B', true, 0);
    CHECK(wubu_hid_poll(&ev) == 1 && ev.device == WUBU_DEV_KEYBOARD);
    CHECK(wubu_hid_poll(&ev) == 0);                 /* mouse disabled */

    CHECK(wubu_hid_stats(WUBU_DEV_KEYBOARD) == 2);  /* A + B */
    CHECK(wubu_hid_stats(WUBU_DEV_MOUSE) == 2);
    CHECK(wubu_hid_stats(WUBU_DEV_GAMEPAD) == 1);
    CHECK(wubu_hid_overflow() == 0);

    if (failures == 0) printf("test_theme_hid: ALL PASS\n");
    else printf("test_theme_hid: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
