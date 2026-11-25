/* wm_taskbar.c -- Taskbar render + geometry.
 *
 * Self-contained: renders the taskbar (Start button, window buttons, clock,
 * systray) using the shared g_dwm state (extern in ../wm_internal.h) and
 * the public vbe_* and theme APIs. Clock strings come from taskbar_* (declared
 * in wm.h). Minimal includes.
 */

#include "wubu_gui_metrics.h"
#include "../wm_internal.h"
#include "../kernel/input.h"     /* MOD_SHIFT: shift-click means new instance */
#include "taskbar_tasks.h"
#include "startmenu.h"
#include <time.h>

/* The app registry lives in src/apps; a test or minimal build may not link
 * it.  Weak, exactly as the taskbar's pinned buttons do it. */
__attribute__((weak)) void launch_app(const char *name);
__attribute__((weak)) void desktop_launch_exec(const char *exec);
__attribute__((weak)) void taskbar_show_jump_list(int task_idx, int mx, int my);
/* Win+W widgets board lives in its own module; weak for the same reason. */
__attribute__((weak)) void win11_widgets_toggle(void);

/* -- Modern (Win11-class) floating bar ------------------------------ */

static void draw_start_flag(int x, int y);   /* defined below */

#define MODERN_GAP_V   WUBU_METRIC_GAP_SM /* was 6: off the grid */      /* floating margin below the screen */
#define MODERN_RADIUS  WUBU_METRIC_RADIUS_WINDOW
/* Bar padding and inter-task gaps come from the shared spacing scale
 * (WUBU_SPACE_SM / WUBU_SPACE_XS) so the panel reads as one grid. */

/*
 * THE geometry for the floating bar.  The renderer and the pointer path both
 * call this, so "drawn here, clicked there" is not reachable.
 */
void taskbar_modern_layout(int fb_w, int fb_h, WmTaskLayout *L) {
    memset(L, 0, sizeof *L);
    int th  = taskbar_height_dynamic();
    int bh  = th - WUBU_TASKBAR_TILE_INSET;
    int gap = WUBU_SPACE_XS;

    /* Measure the button row first so it can be centred. */
    int row_w = 0;
    /* STABLE ORDER: creation order, never z-order. Iterating zorder[] meant every focus
     * change re-sorted the row, so tiles physically jumped under the pointer -- "the taskbar
     * shuffles". Windows in Windows 11 never reorder on focus; a click changes the indicator,
     * not the position. */
    for (int idx = 0; idx < DOSGUI_MAX_WINDOWS && L->n_slots < 48; idx++) {
        DosGuiWindow *w = &g_dwm.windows[idx];
        if (!w->alive || w->desktop != g_dwm.current_desktop) continue;
        /* Uniform tile: Win11 shows ICONS at a fixed size, never titles. Measuring the title
         * made buttons variable-width, truncated ("Tem...") and text-heavy -- the single
         * biggest tell that this was not a Win11 taskbar. */
        int tw = WUBU_TASKBAR_BTN_MIN_W;
        L->slot[L->n_slots].w = tw;
        L->slot[L->n_slots].h = bh;
        L->slot[L->n_slots].is_pin = 0;
        L->slot[L->n_slots].index = idx;
        row_w += tw + gap;
        L->n_slots++;
    }
    for (int i = 0; i < wubu_taskbar_count() && L->n_slots < 48; i++) {
        const WubuTask *t = wubu_taskbar_get(i);
        if (!t || !t->pinned || t->win_id >= 0) continue;
        L->slot[L->n_slots].w = WUBU_TASKBAR_BTN_MIN_W;
        L->slot[L->n_slots].h = bh;
        L->slot[L->n_slots].is_pin = 1;
        L->slot[L->n_slots].index = i;
        row_w += WUBU_TASKBAR_BTN_MIN_W + gap;
        L->n_slots++;
    }
    if (row_w > 0) row_w -= gap;

    int start_w = th - WUBU_TASKBAR_TILE_INSET;
    int shell_w = WUBU_TASKBAR_SHELL_W;
    int shell_row_w = 3 * shell_w + 2 * gap;
    int clock_w = WUBU_TASKBAR_CLOCK_W;
    int tray_w  = g_dwm.systray_count * (DOSGUI_SYSTRAY_SIZE + WUBU_SPACE_XS);

    int need = WUBU_SPACE_SM + start_w + gap + shell_row_w + gap + row_w + gap +
               tray_w + gap + clock_w + WUBU_SPACE_SM;
    int bar_w = need;
    if (bar_w > fb_w - 2 * MODERN_GAP_V) bar_w = fb_w - 2 * MODERN_GAP_V;
    int bar_x = (fb_w - bar_w) / 2;
    int bar_y = taskbar_top() - MODERN_GAP_V;

    L->bar_x = bar_x; L->bar_y = bar_y; L->bar_w = bar_w; L->bar_h = th;
    L->start_x = bar_x + WUBU_SPACE_SM;
    L->start_y = bar_y + WUBU_SPACE_XS;
    L->start_w = start_w;
    L->start_h = bh;
    L->clock_x = bar_x + bar_w - WUBU_SPACE_SM - clock_w;
    L->clock_y = bar_y + WUBU_SPACE_XS;
    L->clock_w = clock_w;
    L->clock_h = bh;
    L->tray_x  = L->clock_x - gap - tray_w;
    L->tray_y  = bar_y + WUBU_SPACE_XS;
    L->tray_w  = tray_w;

    int shell_x0 = L->start_x + start_w + gap;
    L->shell_y = bar_y + WUBU_SPACE_XS;
    L->shell_w = shell_w;
    L->shell_h = bh;
    for (int i = 0; i < 3; i++)
        L->shell_x[i] = shell_x0 + i * (shell_w + gap);

    int row_x = shell_x0 + shell_row_w + gap;
    int avail = L->tray_x - gap - row_x;
    if (row_w < avail) row_x += (avail - row_w) / 2;

    /* Cap the row at the space that exists. Before this, extra slots were laid out past
     * tray_x -- drawn OVER the systray and the clock, with click geometry to match. Windows
     * collapses the excess behind an overflow button, so the last slots move behind one. */
    L->overflow_n = 0;
    L->hidden_n = 0;
    L->overflow_x = L->overflow_y = L->overflow_w = L->overflow_h = 0;
    int shown = L->n_slots;
    int use = row_w;
    if (use > avail) {
        int btn_w = WUBU_TASKBAR_BTN_MIN_W;
        while (shown > 0 && use + gap + btn_w > avail) {
            shown--;
            use -= L->slot[shown].w + gap;
            L->overflow_n++;
            L->hidden_id[L->hidden_n] = L->slot[shown].index;
            L->hidden_is_pin[L->hidden_n] = L->slot[shown].is_pin;
            L->hidden_n++;
        }
        /* The loop strips from the end, so reverse into task order. */
        for (int i = 0, j = L->hidden_n - 1; i < j; i++, j--) {
            int a = L->hidden_id[i]; L->hidden_id[i] = L->hidden_id[j]; L->hidden_id[j] = a;
            int b = L->hidden_is_pin[i]; L->hidden_is_pin[i] = L->hidden_is_pin[j];
            L->hidden_is_pin[j] = b;
        }
        if (L->overflow_n) {
            L->n_slots = shown;
            L->overflow_w = btn_w;
            L->overflow_h = bh;
            L->overflow_x = row_x + (shown ? use + gap : 0);
            L->overflow_y = bar_y + WUBU_SPACE_XS;
        }
    }

    for (int i = 0; i < L->n_slots; i++) {
        L->slot[i].x = row_x;
        L->slot[i].y = bar_y + WUBU_SPACE_XS;
        row_x += L->slot[i].w + gap;
    }
}

/* Small monochrome glyphs for the shell buttons.  Drawn rather than font
 * glyphs so they scale with the tile and never depend on font coverage.
 * Stroke offsets are glyph-art, not layout. */
static void draw_shell_glyph(int which, int cx, int cy, uint32_t color)
{
    switch (which) {
    case 0:   /* Search: a magnifier (ring + handle) */
        vbe_fill_rect(cx - 6, cy - 6, 8, 2, color); /* glyph-art */
        vbe_fill_rect(cx - 6, cy + 4, 8, 2, color); /* glyph-art */
        vbe_fill_rect(cx - 6, cy - 6, 2, 12, color); /* glyph-art */
        vbe_fill_rect(cx + 2, cy - 6, 2, 12, color); /* glyph-art */
        vbe_fill_rect(cx + 4, cy + 4, 2, 4, color); /* glyph-art */
        vbe_fill_rect(cx + 6, cy + 6, 2, 4, color); /* glyph-art */
        break;
    case 1:   /* Task view: two overlapping rectangles */
        vbe_rect(cx - 7, cy - 5, 14, 10, color); /* glyph-art */
        vbe_rect(cx - 2, cy - 2, 10, 8, color); /* glyph-art */
        break;
    default:  /* Widgets: a 2x2 grid of squares */
        vbe_fill_rect(cx - 7, cy - 7, 6, 6, color); /* glyph-art */
        vbe_fill_rect(cx + 1, cy - 7, 6, 6, color); /* glyph-art */
        vbe_fill_rect(cx - 7, cy + 1, 6, 6, color); /* glyph-art */
        vbe_fill_rect(cx + 1, cy + 1, 6, 6, color); /* glyph-art */
        break;
    }
}

static void render_modern_taskbar(uint32_t *fb, int fb_w, int fb_h) {
    WmTaskLayout L;
    taskbar_modern_layout(fb_w, fb_h, &L);

    vbe_fill_rect_rounded(L.bar_x, L.bar_y, L.bar_w, L.bar_h, MODERN_RADIUS,
                          tc()->taskbar_bg);
    vbe_rect_rounded(L.bar_x, L.bar_y, L.bar_w, L.bar_h, MODERN_RADIUS,
                     tc()->border_dark);

    /* Start: the 4-pane flag, centred in a rounded tile. */
    draw_start_flag(L.start_x + L.start_w / 2 - WUBU_TASKBAR_FLAG_SIZE / 2,
                    L.start_y + L.start_h / 2 - WUBU_TASKBAR_FLAG_SIZE / 2);

    /* Win11 shell buttons: flat subtle tiles — the era bevels are gone. */
    for (int i = 0; i < 3; i++) {
        int bx = L.shell_x[i];
        vbe_fill_rect_rounded(bx, L.shell_y, L.shell_w, L.shell_h,
                              WUBU_METRIC_RADIUS_CONTROL, tc()->btn_face);
        draw_shell_glyph(i, bx + L.shell_w / 2, L.shell_y + L.shell_h / 2,
                         tc()->btn_text);
    }

    for (int i = 0; i < L.n_slots; i++) {
        const WmTaskSlot *s = &L.slot[i];
        bool focused = false;
        bool running = !s->is_pin;
        const char *title;
        if (s->is_pin) {
            const WubuTask *t = wubu_taskbar_get(s->index);
            title = t ? t->title : "";
        } else {
            DosGuiWindow *w = &g_dwm.windows[s->index];
            title = w->title;
            focused = (s->index == g_dwm.focused_id) &&
                      !(w->flags & DOSGUI_WIN_MINIMIZED);
        }
        /* Flat Fluent tile: the focused app gets a subtle lift, the rest
         * sit flush on the bar. Running state reads from the indicator
         * pill below, not from a Win98 bevel. */
        vbe_fill_rect_rounded(s->x, s->y, s->w, s->h,
                              WUBU_METRIC_RADIUS_CONTROL,
                              focused ? tc()->btn_face : tc()->taskbar_bg);
        /* The app mark: a 24px rounded tile in the accent, carrying the initial. It is a
         * PLACEHOLDER for a real per-app icon, and it is honest about that -- what it is not
         * is a text label pretending to be a Windows 11 taskbar button. */
        {
            int isz = WUBU_TASKBAR_TRAY_ICON + 6;         /* 24px mark */
            int mx = s->x + (s->w - isz) / 2;
            int my = s->y + (s->h - isz) / 2 - 2;         /* room for the pill below */
            char mono[2] = { title && title[0] ? title[0] : '?', 0 };
            vbe_fill_rect_rounded(mx, my, isz, isz, WUBU_METRIC_RADIUS_CONTROL,
                                  focused ? tc()->select_bg : tc()->btn_face);
            vbe_draw_text(mx + (isz - vbe_text_width(mono, 1)) / 2,
                          my + (isz - WUBU_FONT_INK_H) / 2, mono,
                          focused ? tc()->select_text : tc()->btn_text, 1);
        }
        /* Progress badge: a sliver across the bottom of the tile. Drawn UNDER the running
         * indicator so a focused app still reads as focused. */
        if (!s->is_pin) {
            DosGuiWindow *pw = &g_dwm.windows[s->index];
            if (pw->alive && pw->task_progress > 0) {
                int th2 = 3;
                int ty2 = s->y + s->h - th2;
                int tw2 = (s->w * pw->task_progress) / 100;
                vbe_fill_rect(s->x, ty2, s->w, th2, tc()->border_dark);
                vbe_fill_rect(s->x, ty2, tw2, th2, tc()->select_bg);
            }
        }
        if (running) {
            int iw = focused ? WUBU_TASKBAR_INDICATOR_W_FOCUS
                             : WUBU_TASKBAR_INDICATOR_W_IDLE;
            int ix = s->x + (s->w - iw) / 2;
            int iy = s->y + s->h - WUBU_TASKBAR_INDICATOR_GAP -
                     WUBU_TASKBAR_INDICATOR_H;
            vbe_fill_rect_rounded(ix, iy, iw, WUBU_TASKBAR_INDICATOR_H,
                                  WUBU_TASKBAR_INDICATOR_H / 2,
                                  focused ? tc()->select_bg
                                          : WUBU_TASKBAR_INDICATOR_IDLE);
        }
    }

    /* Overflow chip: only when slots did not fit. It is reachable (opens task view), so it
     * is a control rather than a decoration -- and its count is the truth about what is
     * hidden, taken straight from the layout. */
    if (L.overflow_n > 0) {
        vbe_fill_rect_rounded(L.overflow_x, L.overflow_y, L.overflow_w, L.overflow_h,
                              WUBU_METRIC_RADIUS_CONTROL, tc()->btn_face);
        char chip[8];
        snprintf(chip, sizeof chip, "+%d", L.overflow_n);
        int tw = vbe_text_width(chip, 1);
        vbe_draw_text(L.overflow_x + (L.overflow_w - tw) / 2,
                      L.overflow_y + L.overflow_h / 2 - 7, chip, tc()->btn_text, 1);
    }

    int tx = L.tray_x;
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (!g_dwm.systray_icons[i].visible) continue;
        vbe_fill_rect_rounded(tx, L.tray_y, DOSGUI_SYSTRAY_SIZE,
                              DOSGUI_SYSTRAY_SIZE, WUBU_METRIC_RADIUS_CONTROL,
                              tc()->btn_face);
        vbe_fill_rect(tx + WUBU_SPACE_XS, L.tray_y + WUBU_SPACE_XS,
                      WUBU_TASKBAR_TRAY_ICON, WUBU_TASKBAR_TRAY_ICON,
                      g_dwm.systray_icons[i].icon_color);
        /* Notification badge — same art as the classic era (one owner:
         * WUBU_TASKBAR_BADGE*). The modern bar used to drop it silently. */
        if (g_dwm.systray_icons[i].notification_count > 0) {
            char badge[8];
            int cnt = g_dwm.systray_icons[i].notification_count;
            if (cnt < 0) cnt = 0;
            snprintf(badge, sizeof badge, "%d", cnt > 9 ? 9 : cnt);
            int bx = tx + DOSGUI_SYSTRAY_SIZE - WUBU_TASKBAR_BADGE;
            vbe_fill_rect_rounded(bx, L.tray_y, WUBU_TASKBAR_BADGE,
                                  WUBU_TASKBAR_BADGE, WUBU_TASKBAR_BADGE / 2,
                                  WUBU_TASKBAR_BADGE_BG);
            vbe_draw_text(bx + WUBU_SPACE_2XS, L.tray_y + 1, badge,
                          WUBU_TASKBAR_BADGE_FG, 1);
        }
        tx += DOSGUI_SYSTRAY_SIZE + WUBU_SPACE_XS;
    }

    /* Publish the tray well: the input path hit-tests THIS layout. */
    g_dwm.tray_well_x = L.tray_x;
    g_dwm.tray_well_y = L.tray_y;
    g_dwm.tray_well_w = L.tray_w;
    g_dwm.tray_well_h = DOSGUI_SYSTRAY_SIZE;

    taskbar_update_clock(time(NULL));
    char *clk = taskbar_get_clock_str();
    g_dwm.clock_well_x = L.clock_x;
    g_dwm.clock_well_y = L.clock_y;
    g_dwm.clock_well_w = L.clock_w;
    g_dwm.clock_well_h = L.clock_h;
    vbe_draw_text(L.clock_x + WUBU_SPACE_XS,
                  L.clock_y + (L.clock_h - WUBU_FONT_INK_H) / 2, clk,
                  tc()->icon_text, 1);
}

/* Which window a slot stands for, or -1. */
static int slot_window(const WmTaskSlot *s) {
    if (!s || s->is_pin) return -1;
    if (s->index < 0 || s->index >= DOSGUI_MAX_WINDOWS) return -1;
    return g_dwm.windows[s->index].alive ? s->index : -1;
}

/* Launch-or-focus the n-th task slot: Win+1..9 lands here. Windows numbers the BUTTONS the
 * user sees, so n counts every slot in layout order, pins and windows alike. */
int taskbar_slot_launch(int n) {
    WmTaskLayout L;
    taskbar_modern_layout(g_dwm.screen_w, g_dwm.screen_h, &L);
    if (n < 0 || n >= L.n_slots) return 0;
    const WmTaskSlot *s = &L.slot[n];
    int wi = slot_window(s);
    if (wi >= 0) {
        DosGuiWindow *w = &g_dwm.windows[wi];
        /* Exclusive branches: clearing the flag and then re-setting it in the next
         * statement meant "restore a minimized window" restored it and re-minimized it in
         * the same call -- the test caught it, the eye did not. */
        if (w->flags & DOSGUI_WIN_MINIMIZED) {
            w->flags &= ~DOSGUI_WIN_MINIMIZED;
            wm_set_focus(w);
        } else if (g_dwm.focused_id == wi) {
            w->flags |= DOSGUI_WIN_MINIMIZED;      /* Windows toggles on the focused one */
        } else {
            wm_set_focus(w);
        }
        return 1;
    }
    if (s->is_pin) {
        const WubuTask *t = wubu_taskbar_get(s->index);
        if (t) {
            const char *x = t->exec[0] ? t->exec : t->title;
            if (desktop_launch_exec) desktop_launch_exec(x);
            else if (launch_app)     launch_app(x);
        }
        return 1;
    }
    return 0;
}

int taskbar_shift_new_instance(int slot_index) {
    if (!(wm_last_mods() & MOD_SHIFT)) return 0;
    return taskbar_new_instance(slot_index);
}

/* Middle click = a NEW instance (Windows). Only a pin knows its command; a window task does
 * not record how it was started, so a window slot returns 0 and the caller falls through to
 * the normal focus behaviour. */
int taskbar_new_instance(int slot_index) {
    WmTaskLayout L;
    taskbar_modern_layout(g_dwm.screen_w, g_dwm.screen_h, &L);
    if (slot_index < 0 || slot_index >= L.n_slots) return 0;
    const WmTaskSlot *s = &L.slot[slot_index];
    if (!s->is_pin) return 0;
    const WubuTask *t = wubu_taskbar_get(s->index);
    if (!t) return 0;
    const char *x = t->exec[0] ? t->exec : t->title;
    if (desktop_launch_exec) desktop_launch_exec(x);
    else if (launch_app)     launch_app(x);
    return 1;
}


/* -- Hover previews (Win11 taskbar thumbnails) --------------------------
 *
 * The bar is composited AFTER the windows into the same frame buffer, so the frame the bar is
 * handed already contains every window. A thumbnail is therefore a nearest-neighbour sample
 * of that frame, taken at the window's rect -- no capture plumbing, no allocation, no second
 * copy of the renderer.
 *
 * Two facts are kept: which slot the pointer is over, and since when. A preview appears after
 * WM_HOVER_MS so brushing across the bar does not flash cards at the user. */

#define WM_THUMB_W 160
#define WM_THUMB_H 90
#define WM_HOVER_MS 300

static int      g_hover_slot = -1;
static long     g_hover_since_ms;
static int      g_hover_delay_ms = WM_HOVER_MS;

/* Dwell time before a preview appears. 0 in a test so a pixel gate does not sleep. */
void taskbar_set_hover_delay_ms(int ms) { g_hover_delay_ms = ms < 0 ? 0 : ms; }

static long wm_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* Sample a rect of the frame into a smaller buffer. Public so a test can drive it with a
 * known pattern instead of a screen. Returns 1 when at least one pixel was sampled. */
int wm_thumb_capture(const uint32_t *fb, int fb_w, int fb_h,
                     int wx, int wy, int ww, int wh,
                     uint32_t *dst, int dw, int dh) {
    if (!fb || !dst || ww <= 0 || wh <= 0 || dw <= 0 || dh <= 0) return 0;
    int got = 0;
    for (int y = 0; y < dh; y++) {
        int sy = wy + (y * wh) / dh;
        for (int x = 0; x < dw; x++) {
            int sx = wx + (x * ww) / dw;
            if (sx < 0 || sy < 0 || sx >= fb_w || sy >= fb_h) { dst[y * dw + x] = 0; continue; }
            dst[y * dw + x] = fb[sy * fb_w + sx];
            got = 1;
        }
    }
    return got;
}

/* Which slot the pointer is over (-1 = none). Exposed so a test can drive the hover tracker
 * with coordinates instead of waiting 300 ms for a card. */
int taskbar_hover_slot(void) { return g_hover_slot; }

static void draw_hover_preview(uint32_t *fb, int fb_w, int fb_h) {
    if (g_hover_slot < 0) return;
    if (wm_now_ms() - g_hover_since_ms < g_hover_delay_ms) return;

    WmTaskLayout L;
    taskbar_modern_layout(fb_w, fb_h, &L);
    if (g_hover_slot >= L.n_slots) return;
    const WmTaskSlot *s = &L.slot[g_hover_slot];
    int wi = slot_window(s);
    if (wi < 0) return;                       /* pins with no window have nothing to show */

    DosGuiWindow *w = &g_dwm.windows[wi];
    static uint32_t thumb[WM_THUMB_W * WM_THUMB_H];
    if (!wm_thumb_capture(fb, fb_w, fb_h, w->x, w->y, w->w, w->h,
                          thumb, WM_THUMB_W, WM_THUMB_H))
        return;

    int pad = WUBU_SPACE_XS;
    int card_w = WM_THUMB_W + pad * 2;
    int text_h = vbe_text_baseline_off();   /* the renderer's own baseline, not a guess */
    int card_h = WM_THUMB_H + pad * 2 + text_h;
    int cx = s->x + s->w / 2 - card_w / 2;
    int cy = L.bar_y - card_h - WUBU_SPACE_SM;
    if (cx < WUBU_SPACE_XS) cx = WUBU_SPACE_XS;
    if (cx + card_w > fb_w - WUBU_SPACE_XS) cx = fb_w - WUBU_SPACE_XS - card_w;
    if (cy < WUBU_SPACE_XS) cy = WUBU_SPACE_XS;

    /* Paint into the SAME buffer the thumbnail was sampled from (fb), not through the vbe
     * global: those are the same surface in production but not in a test, and a preview that
     * samples one buffer and draws into another is a bug waiting for a second caller. */
    for (int y = 0; y < card_h; y++) {
        int py = cy + y;
        if (py < 0 || py >= fb_h) continue;
        for (int x = 0; x < card_w; x++) {
            int px = cx + x;
            if (px < 0 || px >= fb_w) continue;
            int edge = (x == 0 || y == 0 || x == card_w - 1 || y == card_h - 1);
            uint32_t c = edge ? tc()->border_face : tc()->win_face;
            if (!edge && x >= pad && x < pad + WM_THUMB_W &&
                y >= pad && y < pad + WM_THUMB_H)
                c = thumb[(y - pad) * WM_THUMB_W + (x - pad)];
            fb[py * fb_w + px] = c;
        }
    }
    /* Title via the text renderer (it targets vbe's surface, which IS fb in production).
     * The card and thumbnail above are painted into fb directly, so the test can count them. */
    const char *title = wm_window_title_at(wi);
    vbe_draw_text(cx + pad, cy + pad + WM_THUMB_H + 2, title ? title : "",
                  tc()->win_title_text, 1);
}

bool taskbar_modern_handle(int x, int y, int btn, int kind) {
    if (wubu_theme_current() != THEME_DARK) return false;

    WmTaskLayout L;
    taskbar_modern_layout(g_dwm.screen_w, g_dwm.screen_h, &L);
    if (x < L.bar_x || x >= L.bar_x + L.bar_w ||
        y < L.bar_y || y >= L.bar_y + L.bar_h) {
        /* Leaving the bar must clear the hover, and this early return is where a pointer
         * that left actually arrives -- the hover block below never sees it. */
        if (kind == 0) g_hover_slot = -1;
        return false;
    }

    /* Hover tracking for the thumbnail preview (kind 0 = move). A pointer that leaves the
     * bar entirely clears it; moving between slots restarts the dwell timer. */
    if (kind == 0) {
        int hit = -1;
        for (int i = 0; i < L.n_slots; i++) {
            const WmTaskSlot *s = &L.slot[i];
            if (x >= s->x && x < s->x + s->w && y >= s->y && y < s->y + s->h) { hit = i; break; }
        }
        if (hit != g_hover_slot) { g_hover_slot = hit; g_hover_since_ms = wm_now_ms(); }
        return true;                 /* inside the bar: swallow moves too */
    }
    if (kind != 1) return true;      /* inside the bar: swallow moves too */

    if (L.overflow_n > 0 && x >= L.overflow_x && x < L.overflow_x + L.overflow_w &&
        y >= L.overflow_y && y < L.overflow_y + L.overflow_h) {
        /* The chip lists the hidden windows. Weak so a build without the menu still has a
         * reachable path: task view shows every window, hidden ones included. */
        extern __attribute__((weak)) void taskbar_overflow_open(void);
        if (taskbar_overflow_open) taskbar_overflow_open();
        else wm_overview_toggle();
        return true;
    }

    if (x >= L.start_x && x < L.start_x + L.start_w &&
        y >= L.start_y && y < L.start_y + L.start_h) {
        startmenu_toggle();
        return true;
    }
    /* Win11 shell buttons: Search (Start with the search box focused), Task
     * View, Widgets. */
    for (int i = 0; i < 3; i++) {
        if (x < L.shell_x[i] || x >= L.shell_x[i] + L.shell_w ||
            y < L.shell_y || y >= L.shell_y + L.shell_h)
            continue;
        if (i == 0) startmenu_open();
        else if (i == 1) wm_overview_toggle();
        else if (win11_widgets_toggle) win11_widgets_toggle();
        return true;
    }

    if (x >= L.clock_x && x < L.clock_x + L.clock_w &&
        y >= L.clock_y && y < L.clock_y + L.clock_h) {
        clock_menu_toggle();
        return true;
    }
    /* Tray icons: one hit-test owner (systray_icon_at) against the well
     * the renderer publishes. The bar used to swallow these clicks
     * silently — tray icons were drawn but dead in the modern theme. */
    {
        int ti = systray_icon_at(x, y);
        if (ti >= 0) {
            if (btn == 2 && g_dwm.systray_icons[ti].on_right_click)
                g_dwm.systray_icons[ti].on_right_click();
            else if (g_dwm.systray_icons[ti].on_click)
                g_dwm.systray_icons[ti].on_click();
            return true;
        }
    }
    for (int i = 0; i < L.n_slots; i++) {
        const WmTaskSlot *s = &L.slot[i];
        if (x < s->x || x >= s->x + s->w || y < s->y || y >= s->y + s->h)
            continue;
        /* Wheel over the bar walks the window list; middle click opens a new instance.
         * Windows does both, and neither existed: the bar only knew left and right. */
        if ((btn == 4 || btn == 5) && kind == 1) {
            wm_wheel_cycle(btn == 4 ? -1 : +1);
            return true;
        }
        if (btn == 3 && kind == 1) {
            if (taskbar_new_instance(i)) return true;
        }
        /* Shift + left click = new instance (Windows). Checked before the plain left path,
         * which would otherwise focus the existing window instead. */
        if (btn == 1 && kind == 1 && taskbar_shift_new_instance(i)) return true;
        /* Right-click = jump list (Windows). Before this the bar ignored the right
         * button entirely, so a taskbar button could only be clicked. */
        if (btn == 2) {
            if (taskbar_show_jump_list) taskbar_show_jump_list(s->index, x, y);
            return true;
        }
        if (s->is_pin) {
            const WubuTask *t = wubu_taskbar_get(s->index);
            /* Pins store "wubu-app <Name>" exactly like our shortcuts do. Resolving it
             * with launch_app() looked up the literal string and found nothing, so a
             * pinned button did nothing. The same resolver serves both. */
            if (t) {
                const char *x = t->exec[0] ? t->exec : t->title;
                if (desktop_launch_exec) desktop_launch_exec(x);
                else if (launch_app)     launch_app(x);
            }
        } else {
            DosGuiWindow *w = &g_dwm.windows[s->index];
            if (w->flags & DOSGUI_WIN_MINIMIZED) {
                w->flags &= ~DOSGUI_WIN_MINIMIZED;
                wm_set_focus(w);
            } else if (g_dwm.focused_id == s->index) {
                w->flags |= DOSGUI_WIN_MINIMIZED;
            } else {
                wm_set_focus(w);
            }
        }
        return true;
    }
    return true;    /* empty bar space still belongs to the bar */
}

/* -- Taskbar ----------------------------------------------------- */

int taskbar_height(void) { return taskbar_height_dynamic(); }

/* The Start button's emblem: the classic Win95-style 4-pane flag
 * (red / green / blue / yellow) with a black outline. */
static void draw_start_flag(int x, int y) {
    /* Windows 11's Start mark is ONE colour (the accent) in four panes. The four-colour
     * Win95 flag is a different operating system's logo, and it was the loudest "this is
     * not Windows 11" element on the bar. Panes are separated by a 2px gutter rather than
     * outlined, which is how the modern mark reads at 14px. */
    uint32_t accent = tc()->select_bg;
    vbe_fill_rect(x,     y,     6, 6, accent);
    vbe_fill_rect(x + 8, y,     6, 6, accent);
    vbe_fill_rect(x,     y + 8, 6, 6, accent);
    vbe_fill_rect(x + 8, y + 8, 6, 6, accent);
}

void taskbar_render(uint32_t *fb, int fb_w, int fb_h) {
    if (wubu_theme_current() == THEME_DARK) {
        render_modern_taskbar(fb, fb_w, fb_h);
        draw_hover_preview(fb, fb_w, fb_h);   /* after the slots, over the composed frame */
        return;
    }
    int th = taskbar_height_dynamic();
    int ty = taskbar_top();

    /* THE BAR OWNS THE BOTTOM EDGE. The bar was painted for exactly `th` rows from
     * taskbar_top(), but taskbar_top() is derived from g_dwm.screen_h while the frames
     * being rendered can be shorter -- 1024x768 frames with a 719..758 bar left NINE
     * rows of untouched (black) framebuffer underneath it. Whatever the arithmetic
     * says, fill from ty to the real bottom, so there is no unpainted strip. */
    if (ty < 0) ty = 0;
    if (ty > fb_h) ty = fb_h;
    vbe_fill_rect(0, ty, fb_w, fb_h - ty, tc()->taskbar_bg);
    vbe_hline(0, fb_w - 1, ty, tc()->taskbar_border);
    int by = ty + (th - WUBU_TASKBAR_CLASSIC_BTN_H) / 2;
    int start_w = theme()->Luna_start_button ? WUBU_TASKBAR_CLASSIC_START_W_LUNA
                                             : WUBU_TASKBAR_CLASSIC_START_W;

    if (theme()->Luna_start_button) {
        int orb_w = start_w + WUBU_TASKBAR_CLASSIC_LUNA_EXTRA;
        vbe_fill_rect_rounded(WUBU_SPACE_XS, by, orb_w,
                              WUBU_TASKBAR_CLASSIC_LUNA_H,
                              WUBU_METRIC_RADIUS_CONTROL, tc()->start_btn_face);
        vbe_3d_raised_rounded_colors(WUBU_SPACE_XS, by, orb_w,
                              WUBU_TASKBAR_CLASSIC_LUNA_H,
                              WUBU_METRIC_RADIUS_CONTROL,
                              tc()->border_light, tc()->border_face,
                              tc()->border_dark, tc()->border_darkest);
        /* Win95-style 4-pane flag emblem (the Start button must have an icon). */
        draw_start_flag(WUBU_TASKBAR_CLASSIC_EDGE_PAD,
                        by + (WUBU_TASKBAR_CLASSIC_LUNA_H - WUBU_TASKBAR_FLAG_SIZE) / 2);
        vbe_draw_text(WUBU_TASKBAR_CLASSIC_EDGE_PAD + WUBU_TASKBAR_FLAG_SIZE + WUBU_SPACE_XS,
                      by + WUBU_SPACE_SM, "Start", tc()->start_btn_text, 1);
    } else {
        /* Win98 Classic Start button: full-height-ish 3D raised silver
         * button, 4-pane flag emblem + text, with a 2px inner margin. */
        int sbh = WUBU_TASKBAR_CLASSIC_BTN_H;
        int sby = ty + (th - sbh) / 2;
        vbe_fill_rect(WUBU_SPACE_XS, sby, WUBU_TASKBAR_CLASSIC_START_W, sbh,
                      tc()->start_btn_face);
        vbe_3d_raised_colors(WUBU_SPACE_XS, sby, WUBU_TASKBAR_CLASSIC_START_W, sbh,
                             tc()->border_light, tc()->border_face,
                             tc()->border_dark, tc()->border_darkest);
        draw_start_flag(WUBU_SPACE_SM, sby + (sbh - WUBU_TASKBAR_FLAG_SIZE) / 2);
        vbe_draw_text(WUBU_SPACE_SM + WUBU_TASKBAR_FLAG_SIZE + WUBU_SPACE_XS,
                      sby + (sbh - WUBU_FONT_INK_H) / 2, "Start",
                      tc()->start_btn_text, 1);
    }

    int bx = theme()->Luna_start_button
        ? WUBU_SPACE_XS + start_w + WUBU_TASKBAR_CLASSIC_LUNA_EXTRA + WUBU_SPACE_XS
        : WUBU_SPACE_XS + start_w + WUBU_SPACE_SM;

    /* Reserve space for clock + tray icons on the right */
    taskbar_update_clock(time(NULL));
    char *clk = taskbar_get_clock_str();
    int clk_w = vbe_text_width(clk, 1);
    int clock_reserve = clk_w + 2 * WUBU_TASKBAR_CLASSIC_EDGE_PAD;
    int tray_reserve = g_dwm.systray_count * (DOSGUI_SYSTRAY_SIZE + WUBU_SPACE_XS)
                       + WUBU_TASKBAR_CLASSIC_EDGE_PAD;
    int right_reserve = clock_reserve + tray_reserve;

    for (int j = 0; j < g_dwm.nz; j++) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
        if (!w->alive) continue;
        if (w->desktop != g_dwm.current_desktop) continue;
        /* MINIMIZED windows MUST still draw a taskbar button — otherwise
         * the window vanishes from the taskbar entirely and can never be
         * restored (the "lost window" bug). Draw it with a sunken look. */
        bool minimized = (w->flags & DOSGUI_WIN_MINIMIZED);
        int tw = vbe_text_width(w->title, 1);
        int bw = tw + WUBU_TASKBAR_CLASSIC_BTN_PAD;
        if (bw < WUBU_TASKBAR_CLASSIC_BTN_MIN_W) bw = WUBU_TASKBAR_CLASSIC_BTN_MIN_W;
        if (bw > WUBU_TASKBAR_CLASSIC_BTN_MAX_W) bw = WUBU_TASKBAR_CLASSIC_BTN_MAX_W;
        bool focused = (g_dwm.zorder[j] == g_dwm.focused_id);

        /* Check if button would overlap reserved area */
        if (bx + bw > g_dwm.screen_w - right_reserve) break;

        int bh = WUBU_TASKBAR_CLASSIC_BTN_H;
        int bby = ty + (th - bh) / 2;
        char truncated[64];
        strncpy(truncated, w->title, sizeof(truncated) - 1);
        truncated[sizeof(truncated) - 1] = '\0';
        while (strlen(truncated) > 1 &&
               vbe_text_width(truncated, 1) > bw - WUBU_TASKBAR_CLASSIC_TEXT_RESERVE) {
            truncated[strlen(truncated) - 1] = '\0';
        }

        if (focused && !minimized) {
            vbe_fill_rect(bx, bby, bw, bh, tc()->select_bg);
            vbe_3d_sunken_colors(bx, bby, bw, bh,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);
            vbe_draw_text(bx + WUBU_SPACE_SM, bby + (bh - WUBU_FONT_INK_H) / 2,
                          truncated, tc()->select_text, 1);
        } else if (minimized) {
            /* Sunken + dimmed: clearly "not on screen" but present. */
            vbe_fill_rect(bx, bby, bw, bh, tc()->btn_face);
            vbe_3d_sunken_colors(bx, bby, bw, bh,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);
            vbe_draw_text(bx + WUBU_SPACE_SM, bby + (bh - WUBU_FONT_INK_H) / 2,
                          truncated, tc()->btn_text, 1);
        } else {
            vbe_fill_rect(bx, bby, bw, bh, tc()->btn_face);
            vbe_3d_raised_colors(bx, bby, bw, bh,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);
            vbe_draw_text(bx + WUBU_SPACE_SM, bby + (bh - WUBU_FONT_INK_H) / 2,
                          truncated, tc()->btn_text, 1);
        }
        bx += bw + WUBU_TASKBAR_CLASSIC_BTN_GAP;
        if (bx > g_dwm.screen_w - right_reserve) break;
    }

    /* Pinned tasks with no running window.  A pin is the user saying "keep
     * this here", so it stays after the app closes -- the behaviour every
     * mainstream taskbar has and WuBuOS did not. */
    for (int i = 0; i < wubu_taskbar_count(); i++) {
        const WubuTask *t = wubu_taskbar_get(i);
        if (!t || !t->pinned || t->win_id >= 0) continue;
        if (bx + WUBU_TASKBAR_CLASSIC_PIN_W > g_dwm.screen_w - right_reserve) break;

        int bh = WUBU_TASKBAR_CLASSIC_BTN_H;
        int bby = ty + (th - bh) / 2;
        vbe_fill_rect(bx, bby, WUBU_TASKBAR_CLASSIC_PIN_W, bh, tc()->btn_face);
        vbe_3d_raised_colors(bx, bby, WUBU_TASKBAR_CLASSIC_PIN_W, bh,
                             tc()->border_light, tc()->btn_face,
                             tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(bx + WUBU_SPACE_SM, bby + (bh - WUBU_FONT_INK_H) / 2,
                      t->title, tc()->btn_text, 1);
        /* A pin marker: a small raised square at the button's right edge. */
        vbe_fill_rect(bx + WUBU_TASKBAR_CLASSIC_PIN_W - 2 * WUBU_TASKBAR_CLASSIC_PIN_MARK,
                      bby + WUBU_TASKBAR_CLASSIC_PIN_MARK,
                      WUBU_TASKBAR_CLASSIC_PIN_MARK, WUBU_TASKBAR_CLASSIC_PIN_MARK,
                      tc()->select_bg);
        bx += WUBU_TASKBAR_CLASSIC_PIN_W + WUBU_TASKBAR_CLASSIC_BTN_GAP;
    }

    /* System tray icons (drawn from right to left, before clock) */
    int tray_x = fb_w - WUBU_TASKBAR_CLASSIC_EDGE_PAD;

    /* Clock - use clk/clk_w from earlier in function */
    taskbar_update_clock(time(NULL));

    /* The clock stays RIGHT-ALIGNED.  The window-button loop above already
     * stops at screen_w - right_reserve, so buttons can never reach the
     * clock.  The old "if (clock_x + clk_w > bx) clock_x = bx - clk_w - 10"
     * check fired when bx=72 (no windows) and dragged the clock to x=22 —
     * ON TOP of the Start button (user-reported 2026-08-06).  Never do
     * that: clamp only at the Start-button edge as a hard floor. */
    int clock_x = fb_w - clk_w - WUBU_TASKBAR_CLASSIC_EDGE_PAD;
    if (clock_x < WUBU_TASKBAR_CLASSIC_CLOCK_FLOOR)
        clock_x = WUBU_TASKBAR_CLASSIC_CLOCK_FLOOR;
    if (clock_x + clk_w > fb_w - WUBU_SPACE_XS)
        clock_x = fb_w - WUBU_SPACE_XS - clk_w;

    /* Win98 parity: the clock sits in a RECESSED (sunken) well and is a
     * clickable button — clicking it opens the clock/calendar popup. */
    int cw_y = ty + (th - WUBU_TASKBAR_CLASSIC_CLOCK_H) / 2;
    g_dwm.clock_well_x = clock_x - WUBU_TASKBAR_CLASSIC_CLOCK_PAD;
    g_dwm.clock_well_y = cw_y;
    g_dwm.clock_well_w = clk_w + 2 * WUBU_TASKBAR_CLASSIC_CLOCK_PAD;
    g_dwm.clock_well_h = WUBU_TASKBAR_CLASSIC_CLOCK_H;
    vbe_fill_rect(clock_x - WUBU_TASKBAR_CLASSIC_CLOCK_PAD, cw_y,
                  clk_w + 2 * WUBU_TASKBAR_CLASSIC_CLOCK_PAD,
                  WUBU_TASKBAR_CLASSIC_CLOCK_H, tc()->btn_face);
    vbe_3d_sunken_colors(clock_x - WUBU_TASKBAR_CLASSIC_CLOCK_PAD, cw_y,
                         clk_w + 2 * WUBU_TASKBAR_CLASSIC_CLOCK_PAD,
                         WUBU_TASKBAR_CLASSIC_CLOCK_H,
                         tc()->border_light, tc()->border_face,
                         tc()->border_dark, tc()->border_darkest);
    vbe_draw_text(clock_x, ty + (th - WUBU_FONT_INK_H) / 2, clk,
                  theme()->Luna_start_button ? tc()->start_btn_text
                                             : tc()->icon_text, 1);

    tray_x = clock_x - WUBU_TASKBAR_CLASSIC_EDGE_PAD;

    /* Draw system tray icons */
    for (int i = g_dwm.systray_count - 1; i >= 0; i--) {
        if (g_dwm.systray_icons[i].visible) {
            int x = tray_x - DOSGUI_SYSTRAY_SIZE - WUBU_SPACE_XS;
            int y = ty + (th - DOSGUI_SYSTRAY_SIZE) / 2;

            vbe_fill_rect(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE, tc()->btn_face);
            vbe_3d_raised_colors(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE,
                                 tc()->border_light, tc()->border_face,
                                 tc()->border_dark, tc()->border_darkest);

            vbe_fill_rect(x + WUBU_SPACE_XS, y + WUBU_SPACE_XS,
                          WUBU_TASKBAR_TRAY_ICON, WUBU_TASKBAR_TRAY_ICON,
                          g_dwm.systray_icons[i].icon_color);

            /* Draw notification badge if count > 0 */
            if (g_dwm.systray_icons[i].notification_count > 0) {
                char badge[8];
                int cnt = g_dwm.systray_icons[i].notification_count;
                if (cnt < 0) cnt = 0;
                snprintf(badge, sizeof(badge), "%d", cnt > 9 ? 9 : cnt);
                int bx2 = x + DOSGUI_SYSTRAY_SIZE - WUBU_SPACE_SM;
                int by2 = y;
                vbe_fill_rect_rounded(bx2, by2, WUBU_TASKBAR_BADGE,
                                      WUBU_TASKBAR_BADGE,
                                      WUBU_TASKBAR_BADGE / 2,
                                      WUBU_TASKBAR_BADGE_BG);
                vbe_draw_text(bx2 + WUBU_SPACE_2XS, by2 + 1, badge,
                              WUBU_TASKBAR_BADGE_FG, 1);
            }
            tray_x -= DOSGUI_SYSTRAY_SIZE + WUBU_SPACE_XS;
        }
    }

    /* Publish the tray well (classic walks right-to-left; tray_x is now
     * the left edge). The input path hit-tests THIS layout. */
    g_dwm.tray_well_x = tray_x;
    g_dwm.tray_well_y = ty + (th - DOSGUI_SYSTRAY_SIZE) / 2;
    g_dwm.tray_well_w = (clock_x - WUBU_TASKBAR_CLASSIC_EDGE_PAD) - tray_x;
    g_dwm.tray_well_h = DOSGUI_SYSTRAY_SIZE;

    int desk_x = tray_x - WUBU_TASKBAR_CLASSIC_PAGER_RESERVE;
    /* Virtual-desktop pager: only show when more than one desktop exists.
     * Rendering 9 always-on numbered tabs on a single-desktop machine is
     * the confusing "1 2 3 4 5 6 7 8 9 COMPUTER" garbage. */
    if (g_dwm.desktop_count > 1) {
    for (int d = 0; d < g_dwm.desktop_count; d++) {
        int dx = desk_x + d * WUBU_TASKBAR_CLASSIC_PAGER_PITCH;
        int dy = ty + (th - WUBU_TASKBAR_CLASSIC_PAGER_H) / 2;
        char label = (d == 9) ? 'M' : ('1' + d);
        int lx = dx + (WUBU_TASKBAR_CLASSIC_PAGER_W - vbe_text_width(&label, 1)) / 2;
        if (d == g_dwm.current_desktop) {
            vbe_fill_rect_rounded(dx, dy, WUBU_TASKBAR_CLASSIC_PAGER_W,
                                  WUBU_TASKBAR_CLASSIC_PAGER_H, WUBU_RADIUS_XS,
                                  tc()->select_bg);
            vbe_3d_sunken_rounded_colors(dx, dy, WUBU_TASKBAR_CLASSIC_PAGER_W,
                                  WUBU_TASKBAR_CLASSIC_PAGER_H, WUBU_RADIUS_XS,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);
            vbe_draw_text(lx, ty + (th - WUBU_FONT_INK_H) / 2, &label,
                          tc()->select_text, 1);
        } else {
            vbe_fill_rect_rounded(dx, dy, WUBU_TASKBAR_CLASSIC_PAGER_W,
                                  WUBU_TASKBAR_CLASSIC_PAGER_H, WUBU_RADIUS_XS,
                                  tc()->btn_face);
            vbe_3d_raised_rounded_colors(dx, dy, WUBU_TASKBAR_CLASSIC_PAGER_W,
                                  WUBU_TASKBAR_CLASSIC_PAGER_H, WUBU_RADIUS_XS,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);
            vbe_draw_text(lx, ty + (th - WUBU_FONT_INK_H) / 2, &label,
                          tc()->btn_text, 1);
        }
    }
    }
    /* The cycling developer-hint text that used to live here is gone: a
     * product taskbar does not print its own cheat sheet (GUI slerm 0.5).
     * Shortcuts belong in the walkthrough and the docs. */
}
