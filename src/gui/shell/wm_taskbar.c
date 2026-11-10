/* wm_taskbar.c -- Taskbar render + geometry.
 *
 * Self-contained: renders the taskbar (Start button, window buttons, clock,
 * systray) using the shared g_dwm state (extern in ../wm_internal.h) and
 * the public vbe_* and theme APIs. Clock strings come from taskbar_* (declared
 * in wm.h). Minimal includes.
 */

#include "../wm_internal.h"
#include "taskbar_tasks.h"
#include "startmenu.h"
#include <time.h>

/* The app registry lives in src/apps; a test or minimal build may not link
 * it.  Weak, exactly as the taskbar's pinned buttons do it. */
__attribute__((weak)) void launch_app(const char *name);

/* -- Modern (Win11-class) floating bar ------------------------------ */

static void draw_start_flag(int x, int y);   /* defined below */

#define MODERN_GAP_V   6      /* floating margin below the screen */
#define MODERN_PAD     8
#define MODERN_RADIUS  8

/*
 * THE geometry for the floating bar.  The renderer and the pointer path both
 * call this, so "drawn here, clicked there" is not reachable.
 */
void taskbar_modern_layout(int fb_w, int fb_h, WmTaskLayout *L) {
    memset(L, 0, sizeof *L);
    int th  = taskbar_height_dynamic();
    int bh  = th - 8;
    int gap = 4;

    /* Measure the button row first so it can be centred. */
    int row_w = 0;
    for (int j = 0; j < g_dwm.nz && L->n_slots < 48; j++) {
        int idx = g_dwm.zorder[j];
        DosGuiWindow *w = &g_dwm.windows[idx];
        if (!w->alive || w->desktop != g_dwm.current_desktop) continue;
        int tw = vbe_text_width(w->title, 1) + 16;
        if (tw < 44) tw = 44;
        if (tw > 160) tw = 160;
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
        L->slot[L->n_slots].w = 44;
        L->slot[L->n_slots].h = bh;
        L->slot[L->n_slots].is_pin = 1;
        L->slot[L->n_slots].index = i;
        row_w += 44 + gap;
        L->n_slots++;
    }
    if (row_w > 0) row_w -= gap;

    int start_w = th - 8;
    int clock_w = 64;
    int tray_w  = g_dwm.systray_count * (DOSGUI_SYSTRAY_SIZE + 4);

    int need = MODERN_PAD + start_w + gap + row_w + gap + tray_w + gap +
               clock_w + MODERN_PAD;
    int bar_w = need;
    if (bar_w > fb_w - 2 * MODERN_GAP_V) bar_w = fb_w - 2 * MODERN_GAP_V;
    int bar_x = (fb_w - bar_w) / 2;
    int bar_y = taskbar_top() - MODERN_GAP_V;

    L->bar_x = bar_x; L->bar_y = bar_y; L->bar_w = bar_w; L->bar_h = th;
    L->start_x = bar_x + MODERN_PAD;
    L->start_y = bar_y + 4;
    L->start_w = start_w;
    L->start_h = bh;
    L->clock_x = bar_x + bar_w - MODERN_PAD - clock_w;
    L->clock_y = bar_y + 4;
    L->clock_w = clock_w;
    L->clock_h = bh;
    L->tray_x  = L->clock_x - gap - tray_w;
    L->tray_y  = bar_y + 4;
    L->tray_w  = tray_w;

    int row_x = L->start_x + start_w + gap;
    int avail = L->tray_x - gap - row_x;
    if (row_w < avail) row_x += (avail - row_w) / 2;
    for (int i = 0; i < L->n_slots; i++) {
        L->slot[i].x = row_x;
        L->slot[i].y = bar_y + 4;
        row_x += L->slot[i].w + gap;
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
    draw_start_flag(L.start_x + L.start_w / 2 - 7,
                    L.start_y + L.start_h / 2 - 7);

    for (int i = 0; i < L.n_slots; i++) {
        const WmTaskSlot *s = &L.slot[i];
        bool focused = false;
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
        vbe_fill_rect_rounded(s->x, s->y, s->w, s->h, 6,
                              focused ? tc()->select_bg : tc()->btn_face);
        vbe_3d_raised_rounded_colors(s->x, s->y, s->w, s->h, 6,
                                     tc()->border_light, tc()->border_face,
                                     tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(s->x + 6, s->y + (s->h - 8) / 2, title,
                      focused ? tc()->select_text : tc()->btn_text, 1);
    }

    int tx = L.tray_x;
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (!g_dwm.systray_icons[i].visible) continue;
        vbe_fill_rect_rounded(tx, L.tray_y, DOSGUI_SYSTRAY_SIZE,
                              DOSGUI_SYSTRAY_SIZE, 4, tc()->btn_face);
        vbe_fill_rect(tx + 4, L.tray_y + 4, 16, 16,
                      g_dwm.systray_icons[i].icon_color);
        tx += DOSGUI_SYSTRAY_SIZE + 4;
    }

    taskbar_update_clock(time(NULL));
    char *clk = taskbar_get_clock_str();
    g_dwm.clock_well_x = L.clock_x;
    g_dwm.clock_well_y = L.clock_y;
    g_dwm.clock_well_w = L.clock_w;
    g_dwm.clock_well_h = L.clock_h;
    vbe_draw_text(L.clock_x + 4, L.clock_y + (L.clock_h - 8) / 2, clk,
                  tc()->icon_text, 1);
}

bool taskbar_modern_handle(int x, int y, int btn, int kind) {
    (void)btn;
    if (wubu_theme_current() != THEME_DARK) return false;

    WmTaskLayout L;
    taskbar_modern_layout(g_dwm.screen_w, g_dwm.screen_h, &L);
    if (x < L.bar_x || x >= L.bar_x + L.bar_w ||
        y < L.bar_y || y >= L.bar_y + L.bar_h)
        return false;

    if (kind != 1) return true;      /* inside the bar: swallow moves too */

    if (x >= L.start_x && x < L.start_x + L.start_w &&
        y >= L.start_y && y < L.start_y + L.start_h) {
        startmenu_toggle();
        return true;
    }
    if (x >= L.clock_x && x < L.clock_x + L.clock_w &&
        y >= L.clock_y && y < L.clock_y + L.clock_h) {
        clock_menu_toggle();
        return true;
    }
    for (int i = 0; i < L.n_slots; i++) {
        const WmTaskSlot *s = &L.slot[i];
        if (x < s->x || x >= s->x + s->w || y < s->y || y >= s->y + s->h)
            continue;
        if (s->is_pin) {
            const WubuTask *t = wubu_taskbar_get(s->index);
            if (t && launch_app) launch_app(t->exec[0] ? t->exec : t->title);
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
    vbe_fill_rect(x,     y,     7, 7, 0xFE0000);   /* red    */
    vbe_fill_rect(x + 7, y,     7, 7, 0x00A800);   /* green  */
    vbe_fill_rect(x,     y + 7, 7, 7, 0x0000A8);   /* blue   */
    vbe_fill_rect(x + 7, y + 7, 7, 7, 0xFFC800);   /* yellow */
    vbe_rect(x, y, 14, 14, 0x000000);              /* outline */
}

void taskbar_render(uint32_t *fb, int fb_w, int fb_h) {
    if (wubu_theme_current() == THEME_DARK) {
        render_modern_taskbar(fb, fb_w, fb_h);
        return;
    }
    int th = taskbar_height_dynamic();
    int ty = taskbar_top();

    vbe_fill_rect(0, ty, fb_w, th, tc()->taskbar_bg);
    vbe_hline(0, fb_w - 1, ty, tc()->taskbar_border);
    int by = ty + (th - 22) / 2;
    int start_w = theme()->Luna_start_button ? 54 : 60;
    
    if (theme()->Luna_start_button) {
        vbe_fill_rect_rounded(4, by, start_w + 20, 24, 4, tc()->start_btn_face);
        vbe_3d_raised_rounded_colors(4, by, start_w + 20, 24, 4,
                                     tc()->border_light, tc()->border_face,
                                     tc()->border_dark, tc()->border_darkest);
        /* Win95-style 4-pane flag emblem (the Start button must have an icon). */
        draw_start_flag(10, by + 5);
        vbe_draw_text(28, by + 8, "Start", tc()->start_btn_text, 1);
    } else {
        /* Win98 Classic Start button: full-height-ish 3D raised silver
         * button, 4-pane flag emblem + text, with a 2px inner margin. */
        int sbh = 22;
        int sby = ty + (th - sbh) / 2;
        vbe_fill_rect(4, sby, 60, sbh, tc()->start_btn_face);
        vbe_3d_raised_colors(4, sby, 60, sbh,
                             tc()->border_light, tc()->border_face,
                             tc()->border_dark, tc()->border_darkest);
        draw_start_flag(8, sby + (sbh - 14) / 2);
        vbe_draw_text(26, sby + (sbh - 8) / 2, "Start", tc()->start_btn_text, 1);
    }

    int bx = theme()->Luna_start_button ? 82 : 72;
    
    /* Reserve space for clock + tray icons on the right */
    taskbar_update_clock(time(NULL));
    char *clk = taskbar_get_clock_str();
    int clk_w = vbe_text_width(clk, 1);
    int clock_reserve = clk_w + 20; /* clock + padding */
    int tray_reserve = g_dwm.systray_count * (DOSGUI_SYSTRAY_SIZE + 4) + 10;
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
        int bw = tw + 24;                      /* padding either side */
        if (bw < 120) bw = 120;                /* min width like real Win98 */
        if (bw > 200) bw = 200;                /* cap so long titles don't eat the bar */
        bool focused = (g_dwm.zorder[j] == g_dwm.focused_id);
        
        /* Check if button would overlap reserved area */
        if (bx + bw > g_dwm.screen_w - right_reserve) break;
        
        int bh = 22;
        int bby = ty + (th - bh) / 2;
        char truncated[64];
        strncpy(truncated, w->title, sizeof(truncated) - 1);
        truncated[sizeof(truncated) - 1] = '\0';
        /* truncate to fit button width (bw - 16 for padding + icon gap) */
        while (strlen(truncated) > 1 && vbe_text_width(truncated, 1) > bw - 26) {
            truncated[strlen(truncated) - 1] = '\0';
        }

        if (focused && !minimized) {
            vbe_fill_rect(bx, bby, bw, bh, tc()->select_bg);
            vbe_3d_sunken_colors(bx, bby, bw, bh,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);
            vbe_draw_text(bx + 8, bby + (bh - 8) / 2, truncated, tc()->select_text, 1);
        } else if (minimized) {
            /* Sunken + dimmed: clearly "not on screen" but present. */
            vbe_fill_rect(bx, bby, bw, bh, tc()->btn_face);
            vbe_3d_sunken_colors(bx, bby, bw, bh,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);
            vbe_draw_text(bx + 8, bby + (bh - 8) / 2, truncated, tc()->btn_text, 1);
        } else {
            vbe_fill_rect(bx, bby, bw, bh, tc()->btn_face);
            vbe_3d_raised_colors(bx, bby, bw, bh,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);
            vbe_draw_text(bx + 8, bby + (bh - 8) / 2, truncated, tc()->btn_text, 1);
        }
        bx += bw + 3;   /* 3px gap between buttons */
        if (bx > g_dwm.screen_w - right_reserve) break;
    }

    /* Pinned tasks with no running window.  A pin is the user saying "keep
     * this here", so it stays after the app closes -- the behaviour every
     * mainstream taskbar has and WuBuOS did not. */
    for (int i = 0; i < wubu_taskbar_count(); i++) {
        const WubuTask *t = wubu_taskbar_get(i);
        if (!t || !t->pinned || t->win_id >= 0) continue;
        if (bx + 120 > g_dwm.screen_w - right_reserve) break;

        int bh = 22;
        int bby = ty + (th - bh) / 2;
        vbe_fill_rect(bx, bby, 120, bh, tc()->btn_face);
        vbe_3d_raised_colors(bx, bby, 120, bh,
                             tc()->border_light, tc()->btn_face,
                             tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(bx + 8, bby + (bh - 8) / 2, t->title, tc()->btn_text, 1);
        /* A pin marker: a small raised square at the button's right edge. */
        vbe_fill_rect(bx + 120 - 12, bby + 6, 6, 6, tc()->select_bg);
        bx += 123;
    }

    /* System tray icons (drawn from right to left, before clock) */
    int tray_x = fb_w - 10;

    /* Clock - use clk/clk_w from earlier in function */
    taskbar_update_clock(time(NULL));

    /* The clock stays RIGHT-ALIGNED.  The window-button loop above already
     * stops at screen_w - right_reserve, so buttons can never reach the
     * clock.  The old "if (clock_x + clk_w > bx) clock_x = bx - clk_w - 10"
     * check fired when bx=72 (no windows) and dragged the clock to x=22 —
     * ON TOP of the Start button (user-reported 2026-08-06).  Never do
     * that: clamp only at the Start-button edge as a hard floor. */
    int clock_x = fb_w - clk_w - 10;
    if (clock_x < 68) clock_x = 68;   /* right of the 60px Start button */
    if (clock_x + clk_w > fb_w - 4) clock_x = fb_w - 4 - clk_w;

    /* Win98 parity: the clock sits in a RECESSED (sunken) well and is a
     * clickable button — clicking it opens the clock/calendar popup. */
    int cw_y = ty + (th - 20) / 2;
    g_dwm.clock_well_x = clock_x - 3;
    g_dwm.clock_well_y = cw_y;
    g_dwm.clock_well_w = clk_w + 6;
    g_dwm.clock_well_h = 20;
    vbe_fill_rect(clock_x - 3, cw_y, clk_w + 6, 20, tc()->btn_face);
    vbe_3d_sunken_colors(clock_x - 3, cw_y, clk_w + 6, 20,
                         tc()->border_light, tc()->border_face,
                         tc()->border_dark, tc()->border_darkest);
    vbe_draw_text(clock_x, ty + (th - 8) / 2, clk,
                  theme()->Luna_start_button ? 0xFFFFFF : tc()->icon_text, 1);

    tray_x = clock_x - 10;

    /* Draw system tray icons */
    for (int i = g_dwm.systray_count - 1; i >= 0; i--) {
        if (g_dwm.systray_icons[i].visible) {
            int x = tray_x - DOSGUI_SYSTRAY_SIZE - 4;
            int y = ty + (th - DOSGUI_SYSTRAY_SIZE) / 2;

            vbe_fill_rect(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE, tc()->btn_face);
            vbe_3d_raised_colors(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE,
                                 tc()->border_light, tc()->border_face,
                                 tc()->border_dark, tc()->border_darkest);

            vbe_fill_rect(x + 4, y + 4, 16, 16, g_dwm.systray_icons[i].icon_color);

            /* Draw notification badge if count > 0 */
            if (g_dwm.systray_icons[i].notification_count > 0) {
                char badge[8];
                snprintf(badge, sizeof(badge), "%d", 
                    g_dwm.systray_icons[i].notification_count > 9 ? 9 : g_dwm.systray_icons[i].notification_count);
                int bx = x + DOSGUI_SYSTRAY_SIZE - 8;
                int by = y;
                vbe_fill_rect_rounded(bx, by, 12, 12, 6, 0xFF0000);
                vbe_draw_text(bx + 2, by + 1, badge, 0xFFFFFF, 1);
            }
            tray_x -= DOSGUI_SYSTRAY_SIZE + 4;
        }
    }

    int desk_x = tray_x - 150;
    /* Virtual-desktop pager: only show when more than one desktop exists.
     * Rendering 9 always-on numbered tabs on a single-desktop machine is
     * the confusing "1 2 3 4 5 6 7 8 9 COMPUTER" garbage. */
    if (g_dwm.desktop_count > 1) {
    for (int d = 0; d < g_dwm.desktop_count; d++) {
        int dx = desk_x + d * 16;
        if (d == g_dwm.current_desktop) {
            vbe_fill_rect_rounded(dx, ty + (th - 16) / 2, 14, 16, 2, tc()->select_bg);
            vbe_3d_sunken_rounded_colors(dx, ty + (th - 16) / 2, 14, 16, 2,
                                          tc()->border_light, tc()->border_face,
                                          tc()->border_dark, tc()->border_darkest);
            char label = (d == 9) ? 'M' : ('1' + d);
            vbe_draw_text(dx + 3, ty + (th - 8) / 2, &label, tc()->select_text, 1);
        } else {
            vbe_fill_rect_rounded(dx, ty + (th - 16) / 2, 14, 16, 2, tc()->btn_face);
            vbe_3d_raised_rounded_colors(dx, ty + (th - 16) / 2, 14, 16, 2,
                                          tc()->border_light, tc()->border_face,
                                          tc()->border_dark, tc()->border_darkest);
            char label = (d == 9) ? 'M' : ('1' + d);
            vbe_draw_text(dx + 3, ty + (th - 8) / 2, &label, tc()->btn_text, 1);
        }
    }
    }

    /* -- Status bar tip (cycling keyboard hint) ------------------ */
    {
        static time_t last_tip_swap = 0;
        static int tip_index = 0;
        time_t now = time(NULL);

        if (now != last_tip_swap) {
            /* Cycle tip every ~10 seconds (checked every render frame) */
            if (now - last_tip_swap >= 10) {
                tip_index = (tip_index + 1) % 6;
                last_tip_swap = now;
            }
            /* Initialize clock on first render */
            if (last_tip_swap == 0) {
                last_tip_swap = now;
                tip_index = 0;
            }
        }

        const char *tips[] = {
            "Ctrl+T = cycle theme",
            "Alt+F4 = close window",
            "Win key = Start menu",
            "Ctrl+Alt+Left/Right = desktop",
            "Shift+F10 = context menu",
            "Ctrl+T = cycle theme",
        };
        const char *tip = tips[tip_index % 6];
        int tip_x = desk_x - vbe_text_width(tip, 1) - 20;
        if (tip_x > start_w + 90) {
            vbe_draw_text(tip_x, ty + (th - 8) / 2, tip, tc()->icon_text, 1);
        }
    }
}
