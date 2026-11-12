/*
 * power_menu.c -- the Win+X power-user menu (Windows 11 parity).
 *
 * Self-contained: state, geometry, key/pointer handling and the render.  The
 * actions go through the shell's own launchers (weak, so headless WM tests
 * still link).
 */

#include "power_menu.h"
#include "../wubu_theme.h"
#include "../../kernel/vbe.h"
#include <string.h>

#define POWER_ITEMS 7
#define POWER_ROW_H 26

__attribute__((weak)) void launch_app(const char *name);
__attribute__((weak)) void startmenu_open(void);
__attribute__((weak)) void run_dialog_open(void);
__attribute__((weak)) void wubu_shutdown(void);

enum { POWER_ACT_APP, POWER_ACT_SEARCH, POWER_ACT_RUN, POWER_ACT_SHUTDOWN };

typedef struct {
    const char *label;
    const char *app;        /* for POWER_ACT_APP */
    int         action;
} PowerItem;

static const PowerItem g_items[POWER_ITEMS] = {
    { "Windows Terminal", "Terminal",   POWER_ACT_APP },
    { "Task Manager",     "Task Manager", POWER_ACT_APP },
    { "Settings",         "Settings",   POWER_ACT_APP },
    { "File Explorer",    "File Manager", POWER_ACT_APP },
    { "Search",           NULL,         POWER_ACT_SEARCH },
    { "Run...",           NULL,         POWER_ACT_RUN },
    { "Shut down",        NULL,         POWER_ACT_SHUTDOWN },
};

static bool g_open;
static int  g_hover;

void power_menu_open(void) { g_open = true; g_hover = 0; }
void power_menu_close(void) { g_open = false; }
bool power_menu_is_open(void) { return g_open; }

void power_menu_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h)
{
    int bw = 210, bh = POWER_ITEMS * POWER_ROW_H + 12;
    if (bw > fb_w - 16) bw = fb_w - 16;
    if (x) *x = 8;                       /* anchored over the Start button */
    if (y) *y = fb_h - bh - 52;          /* just above the floating bar */
    if (w) *w = bw;
    if (h) *h = bh;
}

static void activate(int i)
{
    const PowerItem *it = &g_items[i];
    switch (it->action) {
    case POWER_ACT_APP:
        if (launch_app && it->app) launch_app(it->app);
        break;
    case POWER_ACT_SEARCH:
        if (startmenu_open) startmenu_open();
        break;
    case POWER_ACT_RUN:
        if (run_dialog_open) run_dialog_open();
        break;
    case POWER_ACT_SHUTDOWN:
        if (wubu_shutdown) wubu_shutdown();
        break;
    }
    power_menu_close();
}

void power_menu_handle_key(uint32_t key, uint32_t mods)
{
    (void)mods;
    if (!g_open) return;
    switch (key) {
    case 0x1B: power_menu_close(); return;         /* Escape */
    case 0xE048: if (g_hover > 0) g_hover--; return;   /* Up */
    case 0xE050: if (g_hover < POWER_ITEMS - 1) g_hover++; return;  /* Down */
    case 13: case 10: activate(g_hover); return;   /* Enter */
    default: return;
    }
}

bool power_menu_handle_click(int x, int y)
{
    if (!g_open) return false;
    int bx, by, bw, bh;
    power_menu_rect(0, 0, &bx, &by, &bw, &bh);
    /* The rect needs the real output size; the WM passes it via the same
     * function at render time, so recompute against the WM's size. */
    extern int wm_screen_w(void);
    extern int wm_screen_h(void);
    power_menu_rect(wm_screen_w(), wm_screen_h(), &bx, &by, &bw, &bh);

    if (x < bx || x >= bx + bw || y < by || y >= by + bh) {
        power_menu_close();
        return true;                  /* the press dismissed it */
    }
    int i = (y - by - 6) / POWER_ROW_H;
    if (i >= 0 && i < POWER_ITEMS) activate(i);
    return true;
}

void power_menu_render(uint32_t *fb, int fb_w, int fb_h)
{
    (void)fb;
    if (!g_open) return;

    const WubuThemeColors *c = wubu_theme_colors();
    int bx, by, bw, bh;
    power_menu_rect(fb_w, fb_h, &bx, &by, &bw, &bh);

    vbe_fill_rect_rounded(bx, by, bw, bh, 8, c->startmenu_bg);
    vbe_rect_rounded(bx, by, bw, bh, 8, c->border_light);

    for (int i = 0; i < POWER_ITEMS; i++) {
        int iy = by + 6 + i * POWER_ROW_H;
        bool hot = (i == g_hover);
        if (hot) {
            vbe_fill_rect_rounded(bx + 4, iy, bw - 8, POWER_ROW_H - 2, 4,
                                  c->select_bg);
        }
        vbe_draw_text(bx + 14, iy + 8, g_items[i].label,
                      hot ? c->select_text : c->startmenu_text, 1);
    }
}
