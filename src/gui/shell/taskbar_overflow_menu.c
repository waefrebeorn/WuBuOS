/*
 * taskbar_overflow_menu.c -- the list behind the taskbar's "+N" chip.
 *
 * One geometry owner (taskbar_overflow_row_rect) so drawing and hit-testing cannot drift, the
 * same rule the tab strip, the projection flyout and snap assist follow.
 */

#include "taskbar_overflow_menu.h"

#include "taskbar_tasks.h"        /* WubuTask: a hidden pin is listed and launched like any pin */
#include "wubu_gui_metrics.h"
#include "../wm_internal.h"
#include "../kernel/vbe.h"
#include "../wubu_theme.h"

#define OVM_MAX      48
#define OVM_ROW_H    WUBU_TASKBAR_BTN_MIN_W   /* a row is a taskbar button, restated once */
#define OVM_W        280
#define OVM_PAD      WUBU_SPACE_SM

static bool g_open;
static int  g_ids[OVM_MAX];       /* window id, or taskbar task index for a pin */
static int  g_is_pin[OVM_MAX];
static int  g_n;

bool taskbar_overflow_is_open(void) { return g_open; }
int  taskbar_overflow_rows(void)    { return g_n; }
int  taskbar_overflow_window_at(int i) { return (i >= 0 && i < g_n) ? g_ids[i] : -1; }

void taskbar_overflow_open(void) {
    WmTaskLayout L;
    taskbar_modern_layout(g_dwm.screen_w, g_dwm.screen_h, &L);
    /* EVERYTHING hidden is listed, pins included -- the user's question is "what am I not
     * seeing", and a pinned app they cannot see is part of the answer. Pins are appended
     * after window tasks, so they are often exactly what overflows. */
    g_n = 0;
    for (int i = 0; i < L.hidden_n && g_n < OVM_MAX; i++) {
        g_ids[g_n] = L.hidden_id[i];
        g_is_pin[g_n] = L.hidden_is_pin[i];
        g_n++;
    }
    g_open = (g_n > 0);
}

void taskbar_overflow_close(void) { g_open = false; }

bool taskbar_overflow_row_rect(int i, int fb_w, int fb_h, int *x, int *y, int *w, int *h) {
    if (i < 0 || i >= g_n) return false;
    WmTaskLayout L;
    taskbar_modern_layout(fb_w, fb_h, &L);
    int card_h = g_n * OVM_ROW_H + 2 * OVM_PAD;
    int cx = L.overflow_x + L.overflow_w - OVM_W;         /* right-aligned to the chip */
    if (cx < WUBU_SPACE_SM) cx = WUBU_SPACE_SM;
    if (cx + OVM_W > fb_w - WUBU_SPACE_SM) cx = fb_w - WUBU_SPACE_SM - OVM_W;
    int cy = L.bar_y - WUBU_SPACE_SM - card_h;            /* sits ABOVE the bar */
    if (cy < WUBU_SPACE_SM) cy = WUBU_SPACE_SM;
    if (x) *x = cx + OVM_PAD;
    if (y) *y = cy + OVM_PAD + i * OVM_ROW_H;
    if (w) *w = OVM_W - 2 * OVM_PAD;
    if (h) *h = OVM_ROW_H;
    return true;
}

bool taskbar_overflow_click(int x, int y) {
    if (!g_open) return false;
    for (int i = 0; i < g_n; i++) {
        int rx, ry, rw, rh;
        if (!taskbar_overflow_row_rect(i, g_dwm.screen_w, g_dwm.screen_h, &rx, &ry, &rw, &rh))
            continue;
        if (x >= rx && x < rx + rw && y >= ry && y < ry + rh) {
            if (g_is_pin[i]) {
                /* A hidden pin launches, exactly as a visible pin button does. */
                extern __attribute__((weak)) void desktop_launch_exec(const char *);
                const WubuTask *t = wubu_taskbar_get(g_ids[i]);
                if (t && desktop_launch_exec)
                    desktop_launch_exec(t->exec[0] ? t->exec : t->title);
            } else if (g_ids[i] >= 0 && g_ids[i] < DOSGUI_MAX_WINDOWS) {
                DosGuiWindow *w = &g_dwm.windows[g_ids[i]];
                if (w->alive) {
                    if (w->flags & DOSGUI_WIN_MINIMIZED) w->flags &= ~DOSGUI_WIN_MINIMIZED;
                    wm_set_focus(w);
                }
            }
            g_open = false;
            return true;
        }
    }
    g_open = false;              /* a click elsewhere dismisses, like every other flyout */
    return true;
}

bool taskbar_overflow_key(uint32_t key) {
    if (!g_open) return false;
    if (key == 0x01) { g_open = false; return true; }   /* Esc */
    return false;
}

void taskbar_overflow_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_open || g_n == 0) return;

    int rx, ry, rw, rh;
    taskbar_overflow_row_rect(0, fb_w, fb_h, &rx, &ry, &rw, &rh);
    int card_x = rx - OVM_PAD;
    int card_y = ry - OVM_PAD;
    int card_w = OVM_W;
    int card_h = g_n * OVM_ROW_H + 2 * OVM_PAD;

    vbe_fill_rect_rounded(card_x, card_y, card_w, card_h, WUBU_METRIC_RADIUS_WINDOW,
                          tc()->win_face);   /* the flyout card palette, as projection_flyout uses */
    vbe_rect_rounded(card_x, card_y, card_w, card_h, WUBU_METRIC_RADIUS_CONTROL,
                     tc()->border_dark);

    for (int i = 0; i < g_n; i++) {
        int r_x, r_y, r_w, r_h;
        if (!taskbar_overflow_row_rect(i, fb_w, fb_h, &r_x, &r_y, &r_w, &r_h)) continue;
        const char *title;
        if (g_is_pin[i]) {
            const WubuTask *t = wubu_taskbar_get(g_ids[i]);
            title = t ? t->title : "";
        } else {
            title = wm_window_title_at(g_ids[i]);
        }
        vbe_draw_text(r_x + WUBU_SPACE_XS, r_y + (r_h - 14) / 2,
                      title ? title : "", tc()->win_title_text, 1);
    }
}
