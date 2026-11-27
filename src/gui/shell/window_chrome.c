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
#include "../icons/wubu_icons_data.h"   /* baked icon masters (assets/icons) */
#include <string.h>

/* -- Chrome constants -------------------------------------------------
 * Every number here comes from wubu_tokens.h.  This file used to carry its
 * own values (title bar 20, button 14, icon padding 6, text padding 8) while
 * the taskbar and menus carried different ones, which is exactly why the
 * margins disagreed everywhere.  Geometry has one owner now.
 */

#define CHROME_TITLE_BAR_MIN_H    WUBU_TITLE_BAR_H
#define CHROME_BUTTON_SIZE        14                    /* classic square */
#define CHROME_BUTTON_GAP         WUBU_SPACE_2XS
#define CHROME_ICON_SIZE          WUBU_ICON_SM
#define CHROME_ICON_PADDING       WUBU_TITLE_PAD
#define CHROME_TEXT_PADDING       WUBU_SPACE_SM

/* Win11 caption buttons run the full height of the caption bar and touch the
 * corner; classic keeps square beveled buttons inset by a hairline. */
bool chrome_is_win11(void) {
    return wubu_theme_current() == THEME_DARK;
}

static inline int chrome_caption_btn_w(void) {
    return chrome_is_win11() ? WUBU_TITLE_BTN_W : CHROME_BUTTON_SIZE;
}

/* Defined below; declared here so the metrics accessor can be the single
 * place that answers "how big is the caption" without a second walk. */
static inline int chrome_title_bar_height(void);

ChromeCaptionMetrics chrome_caption_metrics(void) {
    int tbh = chrome_title_bar_height();
    bool win11 = chrome_is_win11();
    ChromeCaptionMetrics m;
    m.bar_h = tbh;
    m.btn_w = chrome_caption_btn_w();
    m.btn_h = win11 ? tbh : CHROME_BUTTON_SIZE;
    m.gap   = win11 ? 0 : CHROME_BUTTON_GAP;
    return m;
}

/* -- Internal helpers ----------------------------------------------- */

static inline int chrome_title_bar_height(void) {
    const WubuTheme *th = theme();
    if (chrome_is_win11()) return WUBU_TITLE_BAR_H;
    return th->Luna_start_button ? WUBU_SPACE_XL : DOSGUI_TITLE_H;
}

int chrome_border_width(void) {
    if (wubu_theme_is_fluent()) return WUBU_METRIC_BORDER_HAIRLINE;   /* not a literal */
    return theme()->rounded_buttons ? 2 : 1;
}

static inline bool chrome_is_active(DosGuiWindow *win) {
    return g_dwm.focused_id == win->id;
}

/* Draw 3D raised border (Win98 style) */
static void chrome_draw_3d_raised(int x, int y, int w, int h, int rad) {
    const WubuThemeColors *c = tc();
    /* FLUENT HAS NO BEVELS. A raised edge is the single strongest "this is 1995" signal
     * on the whole desktop, and it was still drawn on every button and list row while
     * the theme claimed Windows 11. Flat face, control radius, hairline edge. */
    if (wubu_theme_is_fluent()) {
        int r = rad > 0 ? rad : WUBU_METRIC_RADIUS_CONTROL;
        vbe_fill_rect_rounded(x, y, w, h, r, c->btn_face);
        vbe_rect_rounded(x, y, w, h, r, c->border_face);
        return;
    }
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
    if (wubu_theme_is_fluent()) {   /* pressed/gutter: flat, never an inset well */
        int r = rad > 0 ? rad : WUBU_METRIC_RADIUS_CONTROL;
        vbe_fill_rect_rounded(x, y, w, h, r, c->btn_pressed);
        vbe_rect_rounded(x, y, w, h, r, c->border_face);
        return;
    }
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
    bool win11 = chrome_is_win11();
    int bwid  = chrome_caption_btn_w();
    int bh    = win11 ? tbh : CHROME_BUTTON_SIZE;
    int gap   = win11 ? 0 : CHROME_BUTTON_GAP;
    if (win->w < 3 * bwid + WUBU_SPACE_XS) return false;

    int right = win->x + win->w - bw - (win11 ? 0 : 2);
    int top   = win->y + bw + (tbh - bh) / 2;

    if (close) { close->x = right - bwid;            close->y = top;
                 close->w = bwid;                    close->h = bh; }
    if (max)   { max->x   = close->x - bwid - gap;   max->y = top;
                 max->w = bwid;                      max->h = bh; }
    if (min)   { min->x   = max->x - bwid - gap;     min->y = top;
                 min->w = bwid;                      min->h = bh; }
    return true;
}

/* Returns which button (if any) contains point (mx, my), absolute screen
 * coordinates: ChromeButton values, CHROME_BTN_NONE when outside. */
int chrome_hit_test_button(DosGuiWindow *win, int mx, int my) {
    ChromeButtonRect bmin, bmax, bclose;
    if (!chrome_button_rects(win, &bmin, &bmax, &bclose)) return CHROME_BTN_NONE;
    if (mx >= bclose.x && mx < bclose.x + bclose.w &&
        my >= bclose.y && my < bclose.y + bclose.h) return CHROME_BTN_CLOSE;
    if (mx >= bmax.x && mx < bmax.x + bmax.w &&
        my >= bmax.y && my < bmax.y + bmax.h) return CHROME_BTN_MAXIMIZE;
    if (mx >= bmin.x && mx < bmin.x + bmin.w &&
        my >= bmin.y && my < bmin.y + bmin.h) return CHROME_BTN_MINIMIZE;
    return CHROME_BTN_NONE;
}

/* -- Main chrome drawing entry point -------------------------------- */

/* Draw complete window chrome (frame, title bar, buttons).
 * Returns content rect for app to draw within. */
/* -- Caption glyphs ----------------------------------------------------
 * Drawn centred inside whatever rect the button occupies, so the caption
 * stays correct when the metrics change.  The old loops drew into a fixed
 * 3..10 box, which produced a lopsided X the moment the button stopped being
 * 14px square -- the exact kind of drift a shared metric is meant to kill.
 */

/* ---------------------------------------------------------------------------
 * Icon table blit. The glyphs are DATA now (assets/icons -> tools/icon_bake.py ->
 * src/gui/icons/wubu_icons_data.h), not procedures inside this file: one icon set,
 * one bake, and the same coverage antialiasing as the chrome's own corners.
 *
 * Nearest-neighbour scaling is deliberate for the small sizes we ship (24 -> 16..24);
 * below 16 the mask's own 4x4 supersampling already carries the softness.
 * ------------------------------------------------------------------------- */
static void chrome_icon_blit(int x, int y, int w, int h, const unsigned char *mask,
                             int mask_sz, uint32_t fg);

/* Title -> baked app icon. The dock and the taskbar used to draw the app's INITIAL because
 * the icon set only had four action glyphs; the masters now include the shipped applets, so a
 * window can carry its real mark. Matching is by name fragment, and a miss returns NULL --
 * the caller then falls back to the letter, which is honest rather than wrong. */
const unsigned char *chrome_app_icon_for(const char *title) {
    if (!title || !*title) return NULL;
    struct { const char *needle; const unsigned char *mask; } kMap[] = {
        { "calc",      wubu_icon_apps_calculator },
        { "holyc",     wubu_icon_apps_holyc },
        { "term",      wubu_icon_apps_terminal },
        { "file",      wubu_icon_apps_folder },
        { "explor",    wubu_icon_apps_folder },
        { "folder",    wubu_icon_apps_folder },
        { "paint",     wubu_icon_apps_paint },
        { "canvas",    wubu_icon_apps_paint },
        { "notepad",   wubu_icon_apps_notepad },
        { "editor",    wubu_icon_apps_editor },
        { "setting",   wubu_icon_apps_settings },
        { "control",   wubu_icon_apps_settings },
        { "task mgr",  wubu_icon_apps_settings },
    };
    for (size_t i = 0; i < sizeof kMap / sizeof kMap[0]; i++) {
        const char *h = title, *n = kMap[i].needle;
        /* case-insensitive substring search, bounded by the title */
        for (; *h; h++) {
            const char *a = h, *b = n;
            while (*a && *b && (*a | 0x20) == (*b | 0x20)) { a++; b++; }
            if (!*b) return kMap[i].mask;
        }
    }
    return NULL;
}

/* Draw an app icon in a box, in one ink colour. Returns 0 when there is no icon for the title. */
int chrome_app_icon_draw(const char *title, int x, int y, int w, int h, uint32_t ink) {
    const unsigned char *mask = chrome_app_icon_for(title);
    if (!mask) return 0;
    chrome_icon_blit(x, y, w, h, mask, WUBU_ICON_SIZE, ink);
    return 1;
}

static void chrome_icon_blit(int x, int y, int w, int h, const unsigned char *mask,
                             int mask_sz, uint32_t fg) {
    if (!mask || mask_sz <= 0 || w <= 0 || h <= 0) return;
    int fr = (int)((fg >> 16) & 0xff), fgn = (int)((fg >> 8) & 0xff), fb = (int)(fg & 0xff);

    /* Map the mask's INK BOUNDS, not its full grid. The baked 24px masters carry padding
     * around the glyph, so drawing the whole grid into a 10px box produced an X of about
     * 6px next to a 10px dash and square -- "the x is tiny". Filling the target box with the
     * ink makes every caption glyph the same apparent size regardless of how the master was
     * padded. */
    int ix0 = mask_sz, iy0 = mask_sz, ix1 = -1, iy1 = -1;
    for (int yy = 0; yy < mask_sz; yy++)
        for (int xx = 0; xx < mask_sz; xx++)
            if (mask[yy * mask_sz + xx]) {
                if (xx < ix0) ix0 = xx;
                if (xx > ix1) ix1 = xx;
                if (yy < iy0) iy0 = yy;
                if (yy > iy1) iy1 = yy;
            }
    if (ix1 < ix0 || iy1 < iy0) return;            /* empty mask: draw nothing */
    const int iw = ix1 - ix0 + 1, ih = iy1 - iy0 + 1;
    /* AREA AVERAGE, not point sampling. A 24px master drawn into a 10px box by nearest
     * neighbour drops most of a diagonal, so the close X came out ragged and asymmetric --
     * "the x close looks off". Averaging the source cell each destination pixel covers keeps
     * the stroke's weight however far it is scaled down. */
    for (int py = 0; py < h; py++) {
        int y0 = iy0 + py * ih / h, y1 = iy0 + (py + 1) * ih / h;
        if (y1 <= y0) y1 = y0 + 1;
        if (y1 > iy1 + 1) y1 = iy1 + 1;
        for (int px = 0; px < w; px++) {
            int x0 = ix0 + px * iw / w, x1 = ix0 + (px + 1) * iw / w;
            if (x1 <= x0) x1 = x0 + 1;
            if (x1 > ix1 + 1) x1 = ix1 + 1;
            unsigned sum = 0, cnt = 0;
            for (int sy2 = y0; sy2 < y1; sy2++)
                for (int sx2 = x0; sx2 < x1; sx2++) { sum += mask[sy2 * mask_sz + sx2]; cnt++; }
            unsigned a = cnt ? sum / cnt : 0;
            if (!a) continue;
            int sx = x + px, sy = y + py;
            uint32_t dst = vbe_get_pixel(sx, sy);
            int dr = (int)((dst >> 16) & 0xff), dg = (int)((dst >> 8) & 0xff), db = (int)(dst & 0xff);
            int r = (fr * (int)a + dr * (255 - (int)a)) / 255;
            int g = (fgn * (int)a + dg * (255 - (int)a)) / 255;
            int b = (fb * (int)a + db * (255 - (int)a)) / 255;
            vbe_set_pixel(sx, sy, ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
        }
    }
}

/* Caption glyph size is FIXED (Win11: 10x10 at 100%), centred in whatever button the theme
 * gives. Scaling it to the button box is what "minimize and maximize are stretched" meant:
 * a 46x32 caption button drew a 27x19 rectangle, i.e. a stretched square and a
 * hairline-thin dash that grew with the button. */
#define CHROME_GLYPH_PX 10

static void chrome_glyph_box(int x, int y, int w, int h, int *gx, int *gy, int *gs) {
    int s = CHROME_GLYPH_PX;
    if (s > w - 6) s = w - 6;
    if (s > h - 6) s = h - 6;
    if (s < 6) s = 6;
    *gx = x + (w - s) / 2;
    *gy = y + (h - s) / 2;
    *gs = s;
}

static void chrome_glyph_close(int x, int y, int w, int h, uint32_t fg) {
    /* A clean 2px-stroke X that FILLS the glyph box. The baked master carried padding and
     * soft edges, so even after ink-bounds mapping the X read about 6px next to a 10px square:
     * "the x is tiny". Drawn here as distance-to-diagonal with coverage, which is the same
     * anti-aliasing rule the rest of the chrome uses -- one stroke width, symmetric, full size.
     * (The old defect was FOUR hand-drawn diagonals; this is two, from one loop, so it cannot
     * reintroduce that.) */
    int gx, gy, s;
    chrome_glyph_box(x, y, w, h, &gx, &gy, &s);
    const float half = 1.1f;                 /* ~2.2px stroke */
    int fr = (int)((fg >> 16) & 0xff), fgn = (int)((fg >> 8) & 0xff), fb = (int)(fg & 0xff);
    for (int py = 0; py < s; py++) {
        for (int px = 0; px < s; px++) {
            float d1 = (float)px - (float)py;                       /* main diagonal */
            float d2 = (float)px - (float)(s - 1 - py);             /* anti diagonal */
            if (d1 < 0) d1 = -d1;
            if (d2 < 0) d2 = -d2;
            float d = d1 < d2 ? d1 : d2;
            float cov = half + 0.5f - d * 0.70710678f;
            if (cov <= 0.0f) continue;
            if (cov > 1.0f) cov = 1.0f;
            int sx = gx + px, sy = gy + py;
            uint32_t dst = vbe_get_pixel(sx, sy);
            int dr = (int)((dst >> 16) & 0xff), dg = (int)((dst >> 8) & 0xff), db = (int)(dst & 0xff);
            int a = (int)(cov * 255.0f);
            int r = (fr * a + dr * (255 - a)) / 255;
            int g = (fgn * a + dg * (255 - a)) / 255;
            int b = (fb * a + db * (255 - a)) / 255;
            vbe_set_pixel(sx, sy, ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
        }
    }
}

static void chrome_glyph_max(int x, int y, int w, int h, uint32_t fg) {
    int gx, gy, s;
    chrome_glyph_box(x, y, w, h, &gx, &gy, &s);
    for (int i = 0; i < s; i++) {
        vbe_set_pixel(gx + i, gy, fg);
        vbe_set_pixel(gx + i, gy + s - 1, fg);
        vbe_set_pixel(gx, gy + i, fg);
        vbe_set_pixel(gx + s - 1, gy + i, fg);
    }
}

static void chrome_glyph_min(int x, int y, int w, int h, uint32_t fg) {
    int gx, gy, s;
    chrome_glyph_box(x, y, w, h, &gx, &gy, &s);
    int mid = gy + s / 2;
    for (int i = 0; i < s; i++) vbe_set_pixel(gx + i, mid, fg);
}

ChromeContentRect chrome_content_rect(const DosGuiWindow *win) {
    ChromeContentRect empty = {0, 0, 0, 0};
    if (!win) return empty;
    int tbh = chrome_title_bar_height();
    int bw  = chrome_border_width();
    ChromeContentRect content = {
        .x = win->x + bw,
        .y = win->y + bw + tbh,
        .w = win->w - 2 * bw,
        .h = win->h - tbh - bw
    };
    return content;
}

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
    /* Fluent rounds WINDOWS to 8px; a control-level 4px radius on a window is the
     * "almost Win11" tell, and the dark preset had the right number while the light
     * one did not. */
    int rad = wubu_theme_is_fluent() ? WUBU_METRIC_RADIUS_WINDOW
            : (th->rounded_buttons ? WUBU_METRIC_RADIUS_CONTROL : 0);

    bool active = (g_dwm.focused_id == win->id);
    bool maximized = win->flags & DOSGUI_WIN_MAXIMIZED;

    /* -- Window frame background -- */
    if (th->Luna_start_button && tc()->win_shadow != 0 && !maximized) {
        int off = WUBU_CHROME_SHADOW_OFF;
        vbe_blend_rect(win->x + off + 1, win->y + off + 1, win->w, win->h,
                       tc()->win_shadow, WUBU_CHROME_SHADOW_ALPHA_SOFT);
        vbe_blend_rect(win->x + off, win->y + off, win->w, win->h,
                       tc()->win_shadow, WUBU_CHROME_SHADOW_ALPHA_HARD);
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

    /* Windows 11 draws the caption on the SAME Mica surface as the body: no strip, no
     * separator, no gradient. The old dark theme filled the caption with win_title_active
     * (0x32) over a win_face (0x24) body, which is the single most "not Windows 11" tell in
     * the chrome after the taskbar labels. */
    uint32_t title_color = active ? tc()->win_title_active : tc()->win_title_inactive;
    if (wubu_theme_current() == THEME_DARK && !theme()->gradient_title)
        title_color = tc()->win_face;
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
    int icon_x = win->x + bw + CHROME_ICON_PADDING;
    int icon_y = win->y + bw + (tbh - CHROME_ICON_SIZE) / 2;
    if (win->icon_color) {
        vbe_fill_rect(icon_x, icon_y, CHROME_ICON_SIZE, CHROME_ICON_SIZE,
                      win->icon_color);
        vbe_rect(icon_x, icon_y, CHROME_ICON_SIZE, CHROME_ICON_SIZE,
                 tc()->border_dark);
    }

    /* -- Title text -- */
    int text_x = icon_x + CHROME_ICON_SIZE + CHROME_TEXT_PADDING;
    int text_y = win->y + bw + (tbh - WUBU_FONT_INK_H) / 2;
    if (win->title[0]) {
        /* Titles are the 700-weight face: Windows' guidance is "use Semibold for titles", and a caption drawn in
         * the body weight is most of why a desktop reads as flat rather than as an OS. */
        vbe_draw_text_bold(text_x, text_y, win->title,
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
                        hclose ? tc()->caption_close_hover : bar);
            chrome_fill(bmax.x, bmax.y, bmax.w, bmax.h,
                        hmax ? tc()->btn_face : bar);
            chrome_fill(bmin.x, bmin.y, bmin.w, bmin.h,
                        hmin ? tc()->btn_face : bar);
            uint32_t fg = tc()->win_title_text;
            chrome_glyph_close(bclose.x, bclose.y, bclose.w, bclose.h, fg);
            chrome_glyph_max(bmax.x, bmax.y, bmax.w, bmax.h, fg);
            chrome_glyph_min(bmin.x, bmin.y, bmin.w, bmin.h, fg);
        } else {
            /* Win98-authentic: silver 3D-raised buttons with BOLD BLACK
             * pixel glyphs (drawn, NOT 8x8-font characters). */
            chrome_fill(bclose.x, bclose.y, bclose.w, bclose.h, tc()->btn_face);
            chrome_draw_3d_raised(bclose.x, bclose.y, bclose.w, bclose.h, 2);
            for (int i = 3; i < 11; i++) {
                vbe_set_pixel(bclose.x + i,     bclose.y + i,     0x000000); /* glyph-art */
                vbe_set_pixel(bclose.x + i + 1, bclose.y + i,     0x000000); /* glyph-art */
                vbe_set_pixel(bclose.x + i,     bclose.y + 14 - i, 0x000000); /* glyph-art */
                vbe_set_pixel(bclose.x + i + 1, bclose.y + 14 - i, 0x000000); /* glyph-art */
            }
            chrome_fill(bmax.x, bmax.y, bmax.w, bmax.h, tc()->btn_face);
            chrome_draw_3d_raised(bmax.x, bmax.y, bmax.w, bmax.h, 2);
            for (int i = 3; i < 11; i++) {
                vbe_set_pixel(bmax.x + i,     bmax.y + 3,     0x000000); /* glyph-art */
                vbe_set_pixel(bmax.x + i,     bmax.y + 10,    0x000000); /* glyph-art */
                vbe_set_pixel(bmax.x + 3,     bmax.y + i,     0x000000); /* glyph-art */
                vbe_set_pixel(bmax.x + 10,    bmax.y + i,     0x000000); /* glyph-art */
            }
            chrome_fill(bmin.x, bmin.y, bmin.w, bmin.h, tc()->btn_face);
            chrome_draw_3d_raised(bmin.x, bmin.y, bmin.w, bmin.h, 2);
            for (int i = 3; i < 11; i++)
                vbe_set_pixel(bmin.x + i, bmin.y + 10, 0x000000); /* glyph-art */
        }
    }

    /* -- Content rect for app to draw within -- */
    ChromeContentRect content = chrome_content_rect(win);
    win->content_x = content.x; win->content_y = content.y;
    win->content_w = content.w; win->content_h = content.h;
    return content;
}

/* Handle title bar button click (close/maximize/minimize) */
void chrome_handle_button_click(DosGuiWindow *win, int button) {
    if (!win) return;

    switch (button) {
        case CHROME_BTN_CLOSE:
            wm_destroy(win);
            break;
        case CHROME_BTN_MAXIMIZE:
            if (win->flags & DOSGUI_WIN_MAXIMIZED)
                wm_restore(win);
            else
                wm_maximize(win);
            break;
        case CHROME_BTN_MINIMIZE:
            wm_minimize(win);
            break;
        default:
            break;
    }
}

/* -- Standardized button drawing ------------------------------------ */

/* Draw a standard button in app content area.
 * pressed=true -> sunken; pressed=false -> raised.
 * The face honors the theme's rounded_buttons flag: a rounded theme (the
 * SteamOS-informed Breeze default) draws a 4px-radius face and the hover
 * color, instead of the square SNES bevel -- so "buttons look wrong" is a
 * theme decision, not a hardcoded one. */
#define CHROME_BTN_RADIUS WUBU_METRIC_RADIUS_CONTROL
void chrome_draw_button(int x, int y, int w, int h,
                               const char *label, bool pressed) {
    const WubuThemeColors *c = tc();
    uint32_t face = pressed ? c->btn_pressed : c->btn_face;
    if (theme()->rounded_buttons) {
        vbe_3d_raised_rounded_colors(x, y, w, h, CHROME_BTN_RADIUS,
                                     c->border_light, face,
                                     c->border_dark, c->border_darkest);
    } else {
        vbe_3d_raised_colors(x, y, w, h, c->border_light, face,
                             c->border_dark, c->border_darkest);
    }
    /* Measure with the real text API: strlen*8 assumed an 8px cell while
     * the font advances 6px (and honours the global text scale) — labels
     * sat off-centre. */
    vbe_draw_text(x + (w - vbe_text_width(label, 1)) / 2,
                  y + (h - WUBU_FONT_INK_H) / 2, label, c->btn_text, 1);
}

/* -- Text edit field chrome ----------------------------------------- */

void chrome_draw_edit_field(int x, int y, int w, int h) {
    const WubuThemeColors *c = tc();
    /* Sunken appearance; a rounded theme (SteamOS/Breeze) rounds the field so
     * text inputs match the buttons beside them. */
    if (theme()->rounded_buttons) {
        vbe_3d_sunken_rounded_colors(x, y, w, h, CHROME_BTN_RADIUS,
                                     c->border_light, c->border_face,
                                     c->border_dark, c->border_darkest);
    } else {
        vbe_3d_sunken_colors(x, y, w, h,
                             c->border_light, c->border_face,
                             c->border_dark, c->border_darkest);
    }
}

/* -- Status bar chrome ---------------------------------------------- */

void chrome_draw_statusbar(int x, int y, int w, int h,
                                  const char *left_text,
                                  const char *right_text) {
    const WubuThemeColors *c = tc();
    vbe_fill_rect(x, y, w, h, c->win_face);
    vbe_rect(x, y, w, h, c->border_dark);
    if (left_text) vbe_draw_text(x + WUBU_SPACE_XS, y + (h - WUBU_FONT_INK_H) / 2, left_text, c->btn_text, 1);
    if (right_text) {
        int rw = vbe_text_width(right_text, 1);
        vbe_draw_text(x + w - rw - WUBU_SPACE_XS, y + (h - WUBU_FONT_INK_H) / 2, right_text, c->btn_text, 1);
    }
}

/* -- Progress bar chrome -------------------------------------------- */

void chrome_draw_progress(int x, int y, int w, int h, int percent) {
    const WubuThemeColors *c = tc();
    int pw = (w * percent) / 100; /* percent-math */
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
            vbe_draw_text(tx + (tab_w - vbe_text_width(labels[i], 1)) / 2,
                          y + (h - WUBU_FONT_INK_H) / 2,
                          labels[i], c->select_text, 1);
        } else {
            vbe_fill_rect(tx, y, tab_w, h, c->btn_face);
            vbe_3d_raised_colors(tx, y, tab_w, h,
                                 c->border_light, c->btn_face,
                                 c->border_dark, c->border_darkest);
            vbe_draw_text(tx + (tab_w - vbe_text_width(labels[i], 1)) / 2,
                          y + (h - WUBU_FONT_INK_H) / 2,
                          labels[i], c->btn_text, 1);
        }
    }
}

/* Mask a window's corners so an app that fills its whole rect cannot square them off.
 *
 * Every app window draws its content into the rect the WM gave it -- the welcome
 * dialog, the terminal, the DOS box -- so the chrome's 8px radius was being painted
 * over by the very window it belongs to. That is why the desktop looked boxy while
 * the theme claimed a radius. The mask samples the pixel just OUTSIDE the window
 * (the real background, wallpaper included) instead of assuming a colour, so the
 * corners read as cut-outs rather than as grey paint.
 *
 * Runs after everything in the rect has been drawn, per window, back to front. */
void chrome_mask_corners(int x, int y, int w, int h, int rad) {
    if (rad <= 0 || w <= 2 * rad || h <= 2 * rad) return;
    /* One background sample per corner: directly outside the corner being cut. */
    uint32_t bg_tl = (y > 0)                 ? vbe_get_pixel(x + rad + 1, y - 1) : tc()->desktop_bg;
    uint32_t bg_tr = (y > 0)                 ? vbe_get_pixel(x + w - rad - 1, y - 1) : tc()->desktop_bg;
    /* Bottom corners must sample BELOW the window, not the top: on a vertical-gradient
     * wallpaper the old code blended the bottom arc toward the top colour, which is why the
     * lower corners never looked rounded. */
    uint32_t bg_bl = (y + h < g_dwm.screen_h)
                     ? vbe_get_pixel(x + rad + 1, y + h) : tc()->desktop_bg;
    uint32_t bg_br = (y + h < g_dwm.screen_h)
                     ? vbe_get_pixel(x + w - rad - 1, y + h) : tc()->desktop_bg;
    /* Coverage, not a staircase. A binary sqrt() cut gives a blocky arc -- "bad edges"
     * is exactly what a 1-bit mask looks like at 8px. Each pixel in the corner box is
     * blended with the background by how much of it lies outside the arc, which is
     * how every real UI toolkit antialiases a rounded rect. */
    for (int dy = 0; dy < rad; dy++) {
        for (int dx = 0; dx < rad; dx++) {
            /* distance from the arc centre for each corner */
            float dxtl = (float)(rad - dx), dytl = (float)(rad - dy);
            float d = __builtin_sqrt(dxtl * dxtl + dytl * dytl);
            float cov = d - (float)rad + 0.5f;      /* >0 outside the arc */
            if (cov <= 0.0f) continue;
            if (cov > 1.0f) cov = 1.0f;
            struct { int px, py; uint32_t bg; } pts[4] = {
                { x + dx,             y + dy,             bg_tl },
                { x + w - 1 - dx,     y + dy,             bg_tr },
                { x + dx,             y + h - 1 - dy,     bg_bl },
                { x + w - 1 - dx,     y + h - 1 - dy,     bg_br },
            };
            for (int k = 0; k < 4; k++) {
                uint32_t src = vbe_get_pixel(pts[k].px, pts[k].py);
                int sr = (int)((src >> 16) & 0xff), sg = (int)((src >> 8) & 0xff), sb = (int)(src & 0xff);
                int br = (int)((pts[k].bg >> 16) & 0xff), bgc = (int)((pts[k].bg >> 8) & 0xff), bb = (int)(pts[k].bg & 0xff);
                int r = (int)((float)sr * (1.0f - cov) + (float)br * cov);
                int g = (int)((float)sg * (1.0f - cov) + (float)bgc * cov);
                int b = (int)((float)sb * (1.0f - cov) + (float)bb * cov);
                vbe_set_pixel(pts[k].px, pts[k].py, ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
            }
        }
    }
}
