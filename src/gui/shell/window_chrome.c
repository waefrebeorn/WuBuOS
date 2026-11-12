/*
 * window_chrome.c  --  Standardized Window Chrome for WuBuOS
 *
 * Centralized drawing of ALL window decorations so every app looks consistent:
 *   - Window frame/border (3D raised/sunken)
 *   - Title bar (flat Win98 or gradient XP/Luna/WuBu)
 *   - Title bar buttons: [icon] [title] [ _ ] [ [] ] [ X ]
 *   - Minimize / Maximize / Close hit testing
 *   - Content area clipping rect for app on_draw
 *
 * All apps should call chrome_draw_window(win, fb, fb_w, fb_h)
 * instead of writing their own frame drawing. This replaces the inline
 * drawing scattered across wm_render.c, calc.c, notepad.c, etc.
 *
 * Theme-driven: uses wubu_theme.h colors + flags (Luna_start_button,
 * gradient_title, rounded_buttons, etc.)
 */

#include "../wm.h"
#include "../wm_internal.h"
#include "window_chrome.h"
#include "../../kernel/vbe.h"
#include "../../gui/wubu_theme.h"
#include "../wubu_tokens.h"   /* unified design tokens (CLI + GUI cohesion) */
#include <string.h>

/* -- Chrome constants ----------------------------------------------- */

#define CHROME_TITLE_BAR_MIN_H    20
#define CHROME_BUTTON_SIZE        14
#define CHROME_BUTTON_GAP         2
#define CHROME_ICON_SIZE          14
#define CHROME_ICON_PADDING       6
#define CHROME_TEXT_PADDING       8

/* -- Internal helpers ----------------------------------------------- */

static inline int chrome_title_bar_height(void) {
    const WubuTheme *th = theme();
    return th->Luna_start_button ? 24 : DOSGUI_TITLE_H;
}

static inline int chrome_border_width(void) {
    return theme()->rounded_buttons ? 2 : 1;
}

static inline bool chrome_is_active(DosGuiWindow *win) {
    return g_dwm.focused_id == win->id;
}

/* Draw 3D raised border (Win98 style) */
static void chrome_draw_3d_raised(int x, int y, int w, int h, int rad) {
    const WubuThemeColors *c = tc();
    if (rad > 0) {
        vbe_rect_rounded(x, y, w, h, rad, c->border_dark);
        vbe_3d_raised_rounded_colors(x, y, w, h, rad,
                                     c->border_light, c->border_face,
                                     c->border_dark, c->border_darkest);
    } else {
        vbe_rect(x, y, w, h, c->border_dark);
        vbe_3d_raised_colors(x, y, w, h,
                             c->border_light, c->border_face,
                             c->border_dark, c->border_darkest);
    }
}

/* Draw 3D sunken border (for active window or pressed buttons) */
static void chrome_draw_3d_sunken(int x, int y, int w, int h, int rad) {
    const WubuThemeColors *c = tc();
    if (rad > 0) {
        vbe_3d_sunken_rounded_colors(x, y, w, h, rad,
                                     c->border_light, c->border_face,
                                     c->border_dark, c->border_darkest);
    } else {
        vbe_3d_sunken_colors(x, y, w, h,
                             c->border_light, c->border_face,
                             c->border_dark, c->border_darkest);
    }
}

/* Draw flat filled rect with theme color */
static void chrome_fill(int x, int y, int w, int h, uint32_t color) {
    vbe_fill_rect(x, y, w, h, color);
}

/* -- Title bar buttons hit-test ------------------------------------- */

bool chrome_button_rects(DosGuiWindow *win, ChromeButtonRect *min,
                         ChromeButtonRect *max, ChromeButtonRect *close) {
    if (!win) return false;
    int bw = chrome_border_width();
    int tbh = chrome_title_bar_height();
    if (win->w < 3 * CHROME_BUTTON_SIZE + 4) return false;

    int right = win->x + win->w - bw - 2;
    int top   = win->y + bw + (tbh - CHROME_BUTTON_SIZE) / 2;

    if (close) { close->x = right - CHROME_BUTTON_SIZE; close->y = top;
                 close->w = CHROME_BUTTON_SIZE;         close->h = CHROME_BUTTON_SIZE; }
    if (max)   { max->x   = close->x - CHROME_BUTTON_SIZE - CHROME_BUTTON_GAP;
                 max->y = top; max->w = CHROME_BUTTON_SIZE; max->h = CHROME_BUTTON_SIZE; }
    if (min)   { min->x   = max->x - CHROME_BUTTON_SIZE - CHROME_BUTTON_GAP;
                 min->y = top; min->w = CHROME_BUTTON_SIZE; min->h = CHROME_BUTTON_SIZE; }
    return true;
}

/* Returns which button (if any) contains point (mx, my), absolute screen
 * coordinates: 1=min 2=max 3=close, 0=none. */
int chrome_hit_test_button(DosGuiWindow *win, int mx, int my) {
    ChromeButtonRect bmin, bmax, bclose;
    if (!chrome_button_rects(win, &bmin, &bmax, &bclose)) return 0;
    if (mx >= bclose.x && mx < bclose.x + bclose.w &&
        my >= bclose.y && my < bclose.y + bclose.h) return 3;
    if (mx >= bmax.x && mx < bmax.x + bmax.w &&
        my >= bmax.y && my < bmax.y + bmax.h) return 2;
    if (mx >= bmin.x && mx < bmin.x + bmin.w &&
        my >= bmin.y && my < bmin.y + bmin.h) return 1;
    return 0;
}

/* -- Main chrome drawing entry point -------------------------------- */

/* Draw complete window chrome (frame, title bar, buttons).
 * Returns content rect for app to draw within. */
ChromeContentRect chrome_draw_window(DosGuiWindow *win,
                                            uint32_t *fb, int fb_w, int fb_h) {
    if (!win || !win->alive) {
        ChromeContentRect empty = {0, 0, 0, 0};
        return empty;
    }

    const WubuThemeColors *c = tc();
    const WubuTheme *th = theme();

    int tbh = chrome_title_bar_height();
    int bw = chrome_border_width();
    int rad = th->rounded_buttons ? (wubu_theme_current() == THEME_DARK ? 8 : 4) : 0;

    bool active = (g_dwm.focused_id == win->id);
    bool maximized = win->flags & DOSGUI_WIN_MAXIMIZED;

    /* -- Window frame background -- */
    if (th->Luna_start_button && tc()->win_shadow != 0 && !maximized) {
        int off = 4;
        vbe_blend_rect(win->x + off + 1, win->y + off + 1, win->w, win->h,
                       tc()->win_shadow, 40);
        vbe_blend_rect(win->x + off, win->y + off, win->w, win->h,
                       tc()->win_shadow, 90);
    }

    /* Window body */
    if (rad > 0) {
        vbe_fill_rect_rounded(win->x, win->y, win->w, win->h, rad, tc()->win_face);
    } else {
        vbe_fill_rect(win->x, win->y, win->w, win->h, tc()->win_face);
    }

    /* Outer border */
    if (rad > 0) {
        vbe_rect_rounded(win->x, win->y, win->w, win->h, rad, tc()->border_dark);
    } else {
        vbe_rect(win->x, win->y, win->w, win->h, tc()->border_dark);
    }

    /* -- Title bar -- */
    int tb_x = win->x + bw;
    int tb_y = win->y + bw;
    int tb_w = win->w - 2 * bw;
    int tb_h = chrome_title_bar_height();

    uint32_t title_color = active ? tc()->win_title_active : tc()->win_title_inactive;
    uint32_t title_text_color = active ? tc()->win_title_text : tc()->win_title_text_ina;

    if (theme()->gradient_title) {
        /* Gradient title bar (XP Luna / WuBu) */
        uint32_t start = active ? theme()->title_gradient.color_start
                                : theme()->title_gradient_ina.color_start;
        uint32_t end   = active ? theme()->title_gradient.color_end
                                : theme()->title_gradient_ina.color_end;

        vbe_hgradient(win->x + bw, win->y + bw,
                      win->w - 2 * bw, tbh, start, end);

        /* Restore rounded top corners */
        if (rad > 0) {
            vbe_fill_rect_rounded(win->x, win->y, win->w, win->h, rad, tc()->win_face);
            vbe_hgradient(win->x, win->y, win->w, tbh - rad,
                          start, end);
            vbe_rect_rounded(win->x, win->y, win->w, win->h, rad, tc()->border_dark);
        }
    } else if (wubu_theme_current() == THEME_DARK) {
        /* Windows 11: the title bar is flush with the frame. */
        chrome_fill(win->x + bw, win->y + bw, win->w - 2 * bw, tbh, tc()->win_face);
    } else {
        /* Flat title bar (Win98 Classic) */
        chrome_fill(win->x + bw, win->y + bw, win->w - 2 * bw, tbh, title_color);
    }

    /* Title bar border bottom (separator) */
    vbe_hline(win->x + bw, win->x + win->w - bw - 1, win->y + bw + tbh,
              wubu_theme_current() == THEME_DARK ? tc()->border_light
                                                 : tc()->border_dark);

    /* -- Window icon -- */
    int icon_x = win->x + bw + 4;
    int icon_y = win->y + bw + (tbh - 14) / 2;
    if (win->icon_color) {
        vbe_fill_rect(icon_x, icon_y, 14, 14, win->icon_color);
        vbe_rect(icon_x, icon_y, 14, 14, tc()->border_dark);
    }

    /* -- Title text -- */
    int text_x = win->x + bw + 20;  /* after icon + padding */
    int text_y = win->y + bw + (tbh - 8) / 2;
    if (win->title[0]) {
        vbe_draw_text(text_x, text_y, win->title,
                      active ? tc()->win_title_text : tc()->win_title_text_ina, 1);
    }

    /* Title bar border bottom (separator) */
    vbe_hline(win->x + bw, win->x + win->w - bw - 1, win->y + bw + tbh,
              wubu_theme_current() == THEME_DARK ? tc()->border_light
                                                 : tc()->border_dark);

    /* -- Title bar buttons -- */
    ChromeButtonRect bmin, bmax, bclose;
    if (chrome_button_rects(win, &bmin, &bmax, &bclose)) {
        if (wubu_theme_current() == THEME_DARK) {
            /* Windows 11: flat buttons, no bevel.  Hover reveals a
             * background; Close turns red. */
            int mx = g_dwm.mouse_x, my = g_dwm.mouse_y;
            bool hclose = (mx >= bclose.x && mx < bclose.x + bclose.w &&
                           my >= bclose.y && my < bclose.y + bclose.h);
            bool hmax   = (mx >= bmax.x && mx < bmax.x + bmax.w &&
                           my >= bmax.y && my < bmax.y + bmax.h);
            bool hmin   = (mx >= bmin.x && mx < bmin.x + bmin.w &&
                           my >= bmin.y && my < bmin.y + bmin.h);
            uint32_t bar = tc()->win_face;
            chrome_fill(bclose.x, bclose.y, bclose.w, bclose.h,
                        hclose ? 0xE81123 : bar);
            chrome_fill(bmax.x, bmax.y, bmax.w, bmax.h,
                        hmax ? tc()->btn_face : bar);
            chrome_fill(bmin.x, bmin.y, bmin.w, bmin.h,
                        hmin ? tc()->btn_face : bar);
            uint32_t fg = hclose ? 0xFFFFFF : tc()->win_title_text;
            for (int i = 3; i < 11; i++) {
                vbe_set_pixel(bclose.x + i,     bclose.y + i,     fg);
                vbe_set_pixel(bclose.x + i + 1, bclose.y + i,     fg);
                vbe_set_pixel(bclose.x + i,     bclose.y + 14 - i, fg);
                vbe_set_pixel(bclose.x + i + 1, bclose.y + 14 - i, fg);
            }
            for (int i = 3; i < 11; i++) {
                vbe_set_pixel(bmax.x + i,     bmax.y + 3,     fg);
                vbe_set_pixel(bmax.x + i,     bmax.y + 10,    fg);
                vbe_set_pixel(bmax.x + 3,     bmax.y + i,     fg);
                vbe_set_pixel(bmax.x + 10,    bmax.y + i,     fg);
            }
            for (int i = 3; i < 11; i++)
                vbe_set_pixel(bmin.x + i, bmin.y + 10, fg);
        } else {
            /* Win98-authentic: silver 3D-raised buttons with BOLD BLACK
             * pixel glyphs (drawn, NOT 8x8-font characters). */
            chrome_fill(bclose.x, bclose.y, bclose.w, bclose.h, tc()->btn_face);
            chrome_draw_3d_raised(bclose.x, bclose.y, bclose.w, bclose.h, 2);
            for (int i = 3; i < 11; i++) {
                vbe_set_pixel(bclose.x + i,     bclose.y + i,     0x000000);
                vbe_set_pixel(bclose.x + i + 1, bclose.y + i,     0x000000);
                vbe_set_pixel(bclose.x + i,     bclose.y + 14 - i, 0x000000);
                vbe_set_pixel(bclose.x + i + 1, bclose.y + 14 - i, 0x000000);
            }
            chrome_fill(bmax.x, bmax.y, bmax.w, bmax.h, tc()->btn_face);
            chrome_draw_3d_raised(bmax.x, bmax.y, bmax.w, bmax.h, 2);
            for (int i = 3; i < 11; i++) {
                vbe_set_pixel(bmax.x + i,     bmax.y + 3,     0x000000);
                vbe_set_pixel(bmax.x + i,     bmax.y + 10,    0x000000);
                vbe_set_pixel(bmax.x + 3,     bmax.y + i,     0x000000);
                vbe_set_pixel(bmax.x + 10,    bmax.y + i,     0x000000);
            }
            chrome_fill(bmin.x, bmin.y, bmin.w, bmin.h, tc()->btn_face);
            chrome_draw_3d_raised(bmin.x, bmin.y, bmin.w, bmin.h, 2);
            for (int i = 3; i < 11; i++)
                vbe_set_pixel(bmin.x + i, bmin.y + 10, 0x000000);
        }
    }

    /* -- Content rect for app to draw within -- */
    ChromeContentRect content = {
        .x = win->x + bw,
        .y = win->y + bw + tbh,
        .w = win->w - 2 * bw,
        .h = win->h - tbh - bw
    };

    return content;
}

/* Handle title bar button click (close/maximize/minimize) */
void chrome_handle_button_click(DosGuiWindow *win, int button) {
    if (!win) return;

    switch (button) {
        case 3:  /* CHROME_BTN_CLOSE */
            wm_destroy(win);
            break;
        case 2:  /* CHROME_BTN_MAXIMIZE */
            if (win->flags & DOSGUI_WIN_MAXIMIZED)
                wm_restore(win);
            else
                wm_maximize(win);
            break;
        case 1:  /* CHROME_BTN_MINIMIZE */
            wm_minimize(win);
            break;
        default:
            break;
    }
}

/* -- Standardized button drawing ------------------------------------ */

/* Draw a standard 3D button in app content area.
 * pressed=true -> sunken; pressed=false -> raised. */
void chrome_draw_button(int x, int y, int w, int h,
                               const char *label, bool pressed) {
    const WubuThemeColors *c = tc();
    uint32_t face = pressed ? c->btn_pressed : c->btn_face;
    vbe_3d_raised_colors(x, y, w, h, c->border_light, face,
                         c->border_dark, c->border_darkest);
    vbe_draw_text(x + (w - (int)strlen(label)*8)/2, y + (h-8)/2, label,
                  c->btn_text, 1);
}

/* -- Text edit field chrome ----------------------------------------- */

void chrome_draw_edit_field(int x, int y, int w, int h) {
    const WubuThemeColors *c = tc();
    /* Sunken appearance */
    vbe_3d_sunken_colors(x, y, w, h,
                         c->border_light, c->border_face,
                         c->border_dark, c->border_darkest);
}

/* -- Status bar chrome ---------------------------------------------- */

void chrome_draw_statusbar(int x, int y, int w, int h,
                                  const char *left_text,
                                  const char *right_text) {
    const WubuThemeColors *c = tc();
    vbe_fill_rect(x, y, w, h, c->win_face);
    vbe_rect(x, y, w, h, c->border_dark);
    if (left_text) vbe_draw_text(x + 4, y + (h-8)/2, left_text, c->btn_text, 1);
    if (right_text) {
        int rw = vbe_text_width(right_text, 1);
        vbe_draw_text(x + w - rw - 4, y + (h-8)/2, right_text, c->btn_text, 1);
    }
}

/* -- Progress bar chrome -------------------------------------------- */

void chrome_draw_progress(int x, int y, int w, int h, int percent) {
    const WubuThemeColors *c = tc();
    int pw = (w * percent) / 100;
    if (pw > w) pw = w;
    vbe_3d_sunken_colors(x, y, w, h,
                         c->border_light, c->border_face,
                         c->border_dark, c->border_darkest);
    vbe_fill_rect(x, y, pw, h, c->select_bg);
}

/* -- Tab control chrome --------------------------------------------- */

void chrome_draw_tabs(int x, int y, int w, int h,
                             const char **labels, int count, int active_tab) {
    const WubuThemeColors *c = tc();
    int tab_w = w / count;
    for (int i = 0; i < count; i++) {
        int tx = x + i * tab_w;
        if (i == active_tab) {
            vbe_fill_rect(tx, y, tab_w, h, c->select_bg);
            vbe_3d_sunken_colors(tx, y, tab_w, h,
                                 c->border_light, c->border_face,
                                 c->border_dark, c->border_darkest);
            vbe_draw_text(tx + (tab_w - (int)strlen(labels[i])*8)/2, y + (h-8)/2,
                          labels[i], c->select_text, 1);
        } else {
            vbe_fill_rect(tx, y, tab_w, h, c->btn_face);
            vbe_3d_raised_colors(tx, y, tab_w, h,
                                 c->border_light, c->btn_face,
                                 c->border_dark, c->border_darkest);
            vbe_draw_text(tx + (tab_w - (int)strlen(labels[i])*8)/2, y + (h-8)/2,
                          labels[i], c->btn_text, 1);
        }
    }
}