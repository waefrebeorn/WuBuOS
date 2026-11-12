/*
 * run_dialog.c -- the Win+R run dialog (Windows 11 parity).
 *
 * Self-contained: state, key handling and the render.  The launch itself goes
 * through the shell's app registry (weak, so headless WM tests still link).
 */

#include "run_dialog.h"
#include "../wubu_theme.h"
#include "../../kernel/vbe.h"
#include <stdio.h>
#include <string.h>

#define RUN_MAX 128

__attribute__((weak)) void launch_app(const char *name);

static bool g_open;
static char g_query[RUN_MAX];
static int  g_len;

void run_dialog_open(void)
{
    g_open = true;
    g_query[0] = '\0';
    g_len = 0;
}

void run_dialog_close(void) { g_open = false; }
bool run_dialog_is_open(void) { return g_open; }

void run_dialog_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h)
{
    int bw = 420, bh = 96;
    if (bw > fb_w - 24) bw = fb_w - 24;
    if (x) *x = (fb_w - bw) / 2;
    if (y) *y = fb_h / 3;
    if (w) *w = bw;
    if (h) *h = bh;
}

void run_dialog_handle_key(uint32_t key, uint32_t mods)
{
    (void)mods;
    if (!g_open) return;

    if (key == 0x1B) { run_dialog_close(); return; }

    if (key == 8 || key == 127) {
        if (g_len > 0) g_query[--g_len] = '\0';
        return;
    }

    if (key == 13 || key == 10) {
        if (g_len > 0 && launch_app) {
            g_query[g_len] = '\0';
            launch_app(g_query);
        }
        run_dialog_close();
        return;
    }

    if (key >= 32 && key < 127 && g_len < RUN_MAX - 1) {
        g_query[g_len++] = (char)key;
        g_query[g_len] = '\0';
    }
}

bool run_dialog_handle_click(int x, int y)
{
    if (!g_open) return false;
    int bx, by, bw, bh;
    run_dialog_rect(1920, 1080, &bx, &by, &bw, &bh);
    /* The rect depends on the real output size; the WM passes it in via
     * run_dialog_rect at render time, so recompute against the WM's size. */
    extern int wm_screen_w(void);
    extern int wm_screen_h(void);
    run_dialog_rect(wm_screen_w(), wm_screen_h(), &bx, &by, &bw, &bh);
    if (x < bx || x >= bx + bw || y < by || y >= by + bh) {
        run_dialog_close();
        return true;    /* consumed: the press dismissed it */
    }
    return true;        /* inside the box: swallow */
}

void run_dialog_render(uint32_t *fb, int fb_w, int fb_h)
{
    (void)fb;
    if (!g_open) return;

    const WubuThemeColors *c = wubu_theme_colors();
    int bx, by, bw, bh;
    run_dialog_rect(fb_w, fb_h, &bx, &by, &bw, &bh);

    vbe_fill_rect_rounded(bx, by, bw, bh, 8, c->startmenu_bg);
    vbe_rect_rounded(bx, by, bw, bh, 8, c->border_light);

    vbe_draw_text(bx + 16, by + 10,
                  "Type a program, folder or document name, and WuBuOS will",
                  c->startmenu_text, 1);
    vbe_draw_text(bx + 16, by + 22, "open it.", c->startmenu_text, 1);

    int ix = bx + 16, iy = by + 48, iw = bw - 32, ih = 28;
    vbe_fill_rect_rounded(ix, iy, iw, ih, 6, c->btn_face);
    vbe_rect_rounded(ix, iy, iw, ih, 6, c->border_light);

    char disp[RUN_MAX + 2];
    snprintf(disp, sizeof(disp), "%s|", g_query);
    vbe_draw_text(ix + 10, iy + 10, disp, c->startmenu_text, 1);
}
