/*
 * dosgui_wm_test_stub.c  --  Stubs for WM unit tests without full desktop
 * integration.
 *
 * Provides minimal implementations of start-menu / launch / shutdown hooks
 * that the WM and context-menu engine call.  Most are no-ops for unit tests.
 * The start-menu functions provide a MINIMAL testable implementation (not
 * the full programs-db + MIME registry) so that desktop integration tests
 * in wubu_desktop_shot.c can verify the Companion toggle end-to-end.
 *
 * IMPORTANT: dosgui_wm_handle_mouse is deliberately NOT defined here. The real
 * implementation in dosgui_wm_input.c must bind, so that input (and the wubu_ui
 * automation layer) drives the genuine window manager rather than a no-op.
 */

#include "dosgui_wm.h"
#include "dosgui_startmenu.h"
#include "wubu_theme.h"
#include "wubu_bonzi.h"
#include "../hosted/hosted.h"
#include "../kernel/vbe.h"
#include <stdbool.h>
#include <string.h>

/* --- Start menu: minimal testable implementation --- */

/* Menu geometry (must match dosgui_startmenu.c constants). */
#define TSM_MENU_X    4
#define TSM_MENU_W    200
#define TSM_SIDEBAR_W 48
#define TSM_ITEM_H    24

static int tsm_open = 0;
/* Minimal main-menu items for testing: the real app catalog is not linked. */
typedef struct { const char *label; int type; } tsm_item_t;
static tsm_item_t tsm_items[] = {
    { "Programs",  1 },  /* submenu */
    { "Documents", 0 },
    { "Settings",  0 },
    { "",          2 },  /* separator */
    { "Find",      0 },
    { "Help",      0 },
    { "Run...",    0 },
    { "",          2 },  /* separator */
    { "Shut Down", 3 },
    { "WuBu Buddy", 4 },  /* toggle */
};
#define TSM_N_ITEMS (int)(sizeof(tsm_items)/sizeof(tsm_items[0]))

void dosgui_startmenu_toggle(void) { tsm_open = !tsm_open; }
void dosgui_startmenu_open(void)   { tsm_open = 1; }
void dosgui_startmenu_close(void)  { tsm_open = 0; }
int  dosgui_startmenu_is_open(void) { return tsm_open; }

void dosgui_startmenu_handle_click(int x, int y) {
    if (!tsm_open) return;
    int task_h = dosgui_taskbar_height();
    int mh = TSM_ITEM_H;
    int mw = TSM_MENU_W;
    int menu_y = dosgui_wm_screen_h() - task_h - (TSM_N_ITEMS * mh + 4);
    /* Bounds check: click outside the menu closes it. */
    if (x < TSM_MENU_X || x >= TSM_MENU_X + mw ||
        y < menu_y || y >= menu_y + TSM_N_ITEMS * mh + 4) {
        tsm_open = 0;
        return;
    }
    int idx = (y - menu_y - 2) / mh;
    if (idx < 0 || idx >= TSM_N_ITEMS) return;
    if (tsm_items[idx].type == 4) {
        /* Companion toggle */
        extern void wubu_bonzi_set_enabled(bool on);
        extern bool wubu_bonzi_is_enabled(void);
        wubu_bonzi_set_enabled(!wubu_bonzi_is_enabled());
        tsm_open = 0;
    } else if (tsm_items[idx].type == 3) {
        /* Shutdown */
        extern void dosgui_shutdown(void);
        dosgui_shutdown();
        tsm_open = 0;
    }
    /* Other item types: close menu (apps are stubbed). */
    if (tsm_items[idx].type == 0 || tsm_items[idx].type == 1)
        tsm_open = 0;
}

void dosgui_startmenu_track_hover(int x, int y) { (void)x; (void)y; }

void dosgui_startmenu_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!tsm_open || !fb) return;
    int task_h = dosgui_taskbar_height();
    int mh = TSM_ITEM_H;
    int mw = TSM_MENU_W;
    int menu_y = fb_h - task_h - (TSM_N_ITEMS * mh + 4);
    int main_h = TSM_N_ITEMS * mh + 4;

    /* Menu background (Win98 silver). */
    uint32_t bg = 0x00C0C0C0;
    uint32_t border = 0x00808080;
    uint32_t text = 0x00000000;
    uint32_t sel_bg = 0x00000080;
    uint32_t sel_text = 0x00FFFFFF;

    /* Fill background */
    for (int yy = 0; yy < main_h; yy++)
        for (int xx = 0; xx < mw; xx++)
            fb[(menu_y + yy) * fb_w + (TSM_MENU_X + xx)] = bg;
    /* Border */
    for (int xx = 0; xx < mw; xx++) {
        fb[menu_y * fb_w + TSM_MENU_X + xx] = border;
        fb[(menu_y + main_h - 1) * fb_w + TSM_MENU_X + xx] = border;
    }
    for (int yy = 0; yy < main_h; yy++) {
        fb[(menu_y + yy) * fb_w + TSM_MENU_X] = border;
        fb[(menu_y + yy) * fb_w + TSM_MENU_X + mw - 1] = border;
    }

    /* Draw items */
    int content_x = TSM_MENU_X + TSM_SIDEBAR_W + 4;
    int content_w = mw - TSM_SIDEBAR_W - 8;
    for (int i = 0; i < TSM_N_ITEMS; i++) {
        int iy = menu_y + 2 + i * mh;
        if (tsm_items[i].type == 2) {
            /* Separator: horizontal line */
            int sep_y = iy + mh / 2;
            for (int xx = TSM_MENU_X + 4; xx < TSM_MENU_X + mw - 4; xx++)
                fb[sep_y * fb_w + xx] = border;
            continue;
        }
        /* Item background */
        uint32_t ibg = bg, ifg = text;
        /* Simulate: no hover in test render */
        for (int yy = 0; yy < mh - 1; yy++)
            for (int xx = 0; xx < content_w; xx++)
                fb[(iy + yy) * fb_w + content_x + xx] = ibg;
        /* Draw label (8x8 font) */
        vbe_draw_text(content_x + 4, iy + (mh - 8) / 2,
                      tsm_items[i].label, ifg, 1);
    }
}

/* Stub for launch app */
void dosgui_launch_app(const char *name) { (void)name; }

/* Stub for shutdown */
void dosgui_shutdown(void) { }
void dosgui_platform_shutdown(void) { }

/* Hosted-state getter (Play action): no hosted binary in the test harness,
 * so return NULL -- ctx_action_play treats NULL as "no launch". */
hosted_state_t *dosgui_wm_get_hosted_state(void) {
    return NULL;
}