/*
 * dosgui_wm.c  --  WuBuOS DosGui Window Manager Implementation
 *
 * Cell 400: Fable Windowing Agent — THEMED EDITION.
 * Ports ZealOS/WuBuDos bare-metal window management into WuBuOS.
 * Based on Mythos Fable's wm.c (filipvabrousek/osdev).
 *
 * Features:
 *   - Draggable windows with title bars (XP gradient or Win98 flat)
 *   - Z-order + focus management
 *   - Taskbar with window buttons, clock, Start button (Luna orb on XP)
 *   - Close box (red X on Win98, themed on XP)
 *   - Software mouse cursor (18-row arrow)
 *   - Desktop icons with click handlers + drag-drop rearrange
 *   - Drop shadow under windows
 *   - FULL THEME ENGINE INTEGRATION: Win98 Classic, XP Luna Blue, XP Media Orange, WuBu Green
 *   - Rounded buttons on XP themes, square on Win98
 *   - Gradient title bars on XP themes
 *   - System tray (volume, network, battery)
 *   - Virtual desktops (Ctrl+Alt+Left/Right, 1-9 indicators)
 *   - GAAD snap regions for window placement
 *   - Wallpaper support (center/tile/stretch)
 *   - Maximize/Minimize window buttons
 */

#include "dosgui_wm.h"
#include "dosgui_startmenu.h"
#include "dosgui_desktop.h"
#include "wubu_notify.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../compiler/holyc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/* -- Global State ------------------------------------------------- */

typedef struct {
    int             screen_w, screen_h;
    DosGuiWindow    windows[DOSGUI_MAX_WINDOWS];
    int             next_id;
    int             focused_id;

    /* Z-order: indices into windows[], bottom..top */
    int             zorder[DOSGUI_MAX_WINDOWS];
    int             nz;

    /* Drag state */
    int             drag_id;
    int             drag_ox, drag_oy;

    /* Desktop icons */
    DosGuiIcon       icons[DOSGUI_MAX_ICONS];
    int             icon_count;

    /* Icon drag state */
    int             drag_icon_id;
    int             drag_icon_ox, drag_icon_oy;

    /* Wallpaper */
    uint32_t       *wallpaper;
    int             wallpaper_w, wallpaper_h;
    int             wallpaper_mode; /* 0=center, 1=tile, 2=stretch */

    /* Virtual desktops */
    int             current_desktop;
    int             desktop_count;

    /* System Tray */
    DosGuiSysTrayIcon systray_icons[DOSGUI_MAX_SYSTRAY_ICONS];
    int             systray_count;

    /* Notification Center */
    DosGuiNotification notifications[DOSGUI_NOTIF_CENTER_MAX];
    int             notif_count;
    int             next_notif_id;
    bool            notif_center_open;

    /* Last real time for clock */
    time_t          last_clock_update;

    /* Mouse state */
    int             mouse_x, mouse_y;
    int             ticks;
} DosGuiWM;

static DosGuiWM g_dwm = {0};

/* ================================================================
 * HolyC Terminal — Persistent HCCompiler per window
 * ================================================================ */

#define HOLYC_TERM_MAX_LINES  2000
#define HOLYC_TERM_LINE_LEN   512
#define HOLYC_TERM_HISTORY    100

typedef struct {
    HCCompiler       compiler;       /* Persistent HolyC compiler state */
    char             lines[HOLYC_TERM_MAX_LINES][HOLYC_TERM_LINE_LEN];
    int              line_count;
    char             input[HOLYC_TERM_LINE_LEN];
    int              input_pos;
    int              cursor_blink;
    char             history[HOLYC_TERM_HISTORY][HOLYC_TERM_LINE_LEN];
    int              history_count;
    int              history_pos;    /* -1 = current input, 0..history_count-1 = history */
    bool             initialized;
} HolycTerm;

static HolycTerm g_holyc_terms[DOSGUI_MAX_WINDOWS] = {0};

/* Initialize HolyC compiler for a terminal window */
static void holyc_term_init_compiler(HolycTerm *term) {
    if (term->initialized) return;
    hc_gen_init(&term->compiler.gen);
    term->compiler.gen.symbols.n_locals = 0;
    term->compiler.gen.symbols.stack_size = 0;
    term->compiler.gen.n_functions = 0;
    term->compiler.gen.label_count = 0;
    term->initialized = true;
}

/* Forward declarations for WM internal functions */
static void raise_win(int i);
static void close_win(int i);
static int  hit_test(int x, int y);
static void draw_window(int idx);
static void draw_desktop_bg(int fb_w, int fb_h);
static const WubuThemeColors *tc(void);
static const WubuTheme *theme(void);
static int title_bar_height(void);
static int taskbar_height_dynamic(void);
static int border_width(void);
static int theme_radius(void);
static void load_default_wallpaper(void);
static void draw_wallpaper(int fb_w, int fb_h);
static void snap_icon_to_grid(DosGuiIcon *icon);
static int icon_grid_x(int x);
static int icon_grid_y(int y);
static void snap_window_to_gaad(DosGuiWindow *w);

/* Forward declarations for new API functions */
void dosgui_taskbar_update_clock(time_t now);
char *dosgui_taskbar_get_clock_str(void);

/* Draw HolyC terminal content */
static void holyc_term_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    HolycTerm *term = (HolycTerm*)win->user_data;
    if (!term) return;
    
    const int tbh = title_bar_height();
    const int bw = border_width();
    
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;
    
    /* Fill background */
    vbe_fill_rect(cx, cy, cw, ch, 0x00000000);
    
    /* Draw output lines */
    int x = cx + 4;
    int y = cy + 4;
    int line_h = 10;  /* 8px font + 2px spacing */
    int max_visible = (ch - 8) / line_h;
    
    int start = term->line_count - max_visible;
    if (start < 0) start = 0;
    
    for (int i = start; i < term->line_count; i++) {
        if (y + line_h > cy + ch - 4) break;
        vbe_draw_text(x, y, term->lines[i], 0x00FFFFFF, 1);
        y += line_h;
    }
    
    /* Draw input line with cursor */
    if (y + line_h <= cy + ch - 4) {
        char prompt_line[HOLYC_TERM_LINE_LEN + 8];
        snprintf(prompt_line, sizeof(prompt_line), "$ %s", term->input);
        vbe_draw_text(x, y, prompt_line, 0x00FFFF00, 1);
        
        /* Blinking cursor */
        term->cursor_blink++;
        if ((term->cursor_blink / 5) % 2 == 0) {
            int cursor_x = x + (2 + term->input_pos) * 8;  /* 2 for "$ " */
            vbe_vline(cursor_x, y, y + 8, 0x00FFFF00);
        }
    }
}

/* Add a line to terminal output */
static void holyc_term_add_line(HolycTerm *term, const char *line) {
    if (term->line_count < HOLYC_TERM_MAX_LINES) {
        strncpy(term->lines[term->line_count], line, HOLYC_TERM_LINE_LEN - 1);
        term->lines[term->line_count][HOLYC_TERM_LINE_LEN - 1] = '\0';
        term->line_count++;
    } else {
        /* Scroll: shift all lines up */
        for (int i = 1; i < HOLYC_TERM_MAX_LINES; i++) {
            strcpy(term->lines[i-1], term->lines[i]);
        }
        strncpy(term->lines[HOLYC_TERM_MAX_LINES-1], line, HOLYC_TERM_LINE_LEN - 1);
    }
}

/* Add to history */
static void holyc_term_add_history(HolycTerm *term, const char *line) {
    if (term->history_count < HOLYC_TERM_HISTORY) {
        strncpy(term->history[term->history_count], line, HOLYC_TERM_LINE_LEN - 1);
        term->history_count++;
    } else {
        for (int i = 1; i < HOLYC_TERM_HISTORY; i++) {
            strcpy(term->history[i-1], term->history[i]);
        }
        strncpy(term->history[HOLYC_TERM_HISTORY-1], line, HOLYC_TERM_LINE_LEN - 1);
    }
    term->history_pos = -1;
}

/* Evaluate HolyC input via persistent compiler */
static void holyc_term_eval(HolycTerm *term, const char *input) {
    if (!input || !input[0]) return;
    
    /* Add to history */
    holyc_term_add_history(term, input);
    
    /* Echo input */
    char echo_line[HOLYC_TERM_LINE_LEN + 8];
    snprintf(echo_line, sizeof(echo_line), "$ %s", input);
    holyc_term_add_line(term, echo_line);
    
    /* Evaluate via HolyC compiler - use hc_eval which uses JIT */
    int64_t result = hc_eval(input);
    
    /* Format result - HolyC uses I64 by default */
    char result_line[HOLYC_TERM_LINE_LEN];
    snprintf(result_line, sizeof(result_line), "= %ld", (long)result);
    holyc_term_add_line(term, result_line);
}

/* Handle keyboard input for HolyC terminal */
static void holyc_term_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    HolycTerm *term = (HolycTerm*)win->user_data;
    if (!term) return;
    
    if (key == '\n' || key == '\r') {
        /* Execute the input line */
        if (term->input[0]) {
            holyc_term_eval(term, term->input);
            term->input[0] = '\0';
            term->input_pos = 0;
        }
    } else if (key == 8 && term->input_pos > 0) {  /* Backspace */
        term->input[--term->input_pos] = '\0';
    } else if (key == 0xE048) {  /* Up arrow - history */
        if (term->history_count > 0) {
            if (term->history_pos < term->history_count - 1) {
                if (term->history_pos == -1) {
                    /* Save current input to temp */
                    strcpy(term->history[term->history_count], term->input);
                }
                term->history_pos++;
                strcpy(term->input, term->history[term->history_count - 1 - term->history_pos]);
                term->input_pos = strlen(term->input);
            }
        }
    } else if (key == 0xE050) {  /* Down arrow - history */
        if (term->history_pos > 0) {
            term->history_pos--;
            strcpy(term->input, term->history[term->history_count - 1 - term->history_pos]);
            term->input_pos = strlen(term->input);
        } else if (term->history_pos == 0) {
            term->history_pos = -1;
            strcpy(term->input, term->history[term->history_count]);
            term->input_pos = strlen(term->input);
        }
    } else if (key >= 32 && key < 127 && term->input_pos < HOLYC_TERM_LINE_LEN - 1) {
        term->input[term->input_pos++] = (char)key;
        term->input[term->input_pos] = '\0';
    }
}

/* Spawn a HolyC terminal window */
DosGuiWindow *dosgui_wm_spawn_holyc_term(int x, int y, int w, int h) {
    DosGuiWindow *win = dosgui_wm_create(x, y, w, h, "HolyC Terminal");
    if (!win) return NULL;
    
    int idx = -1;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) { idx = i; break; }
    }
    if (idx < 0) return NULL;
    
    HolycTerm *term = &g_holyc_terms[idx];
    memset(term, 0, sizeof(*term));
    
    /* Initialize persistent HolyC compiler */
    holyc_term_init_compiler(term);
    
    win->on_draw = holyc_term_draw;
    win->on_key = holyc_term_key;
    win->user_data = term;
    
    /* Add welcome line */
    holyc_term_add_line(term, "WuBuOS HolyC Terminal v0.1");
    holyc_term_add_line(term, "Type HolyC code. Ctrl+C to exit.");
    holyc_term_add_line(term, "");
    
    return win;
}

/* ================================================================
 * RENDERING — Themed Window Chrome
 * ================================================================ */

static void raise_win(int i) {
    int j = 0;
    while (j < g_dwm.nz && g_dwm.zorder[j] != i) j++;
    if (j == g_dwm.nz) return;
    for (; j < g_dwm.nz - 1; j++)
        g_dwm.zorder[j] = g_dwm.zorder[j + 1];
    g_dwm.zorder[g_dwm.nz - 1] = i;
}

static int spawn_window(int x, int y, int w, int h, const char *title) {
    DosGuiWindow *win = NULL;
    int i;
    for (i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (!g_dwm.windows[i].alive) { win = &g_dwm.windows[i]; break; }
    }
    if (!win) return -1;

    memset(win, 0, sizeof(*win));
    win->id = g_dwm.next_id++;
    win->flags = DOSGUI_WIN_NORMAL;
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->alive = true;
    strncpy(win->title, title ? title : "Window", sizeof(win->title) - 1);

    g_dwm.zorder[g_dwm.nz++] = i;
    g_dwm.focused_id = i;
    return i;
}

static void close_win(int i) {
    if (i < 0 || i >= DOSGUI_MAX_WINDOWS) return;
    g_dwm.windows[i].alive = false;
    g_dwm.windows[i].flags = DOSGUI_WIN_UNUSED;
    int j = 0;
    while (j < g_dwm.nz && g_dwm.zorder[j] != i) j++;
    for (; j < g_dwm.nz - 1; j++)
        g_dwm.zorder[j] = g_dwm.zorder[j + 1];
    if (j < g_dwm.nz) g_dwm.nz--;
    if (g_dwm.drag_id == i) g_dwm.drag_id = -1;
    if (g_dwm.focused_id == i)
        g_dwm.focused_id = g_dwm.nz ? g_dwm.zorder[g_dwm.nz - 1] : -1;
}

static int hit_test(int x, int y) {
    for (int j = g_dwm.nz - 1; j >= 0; j--) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
        if (w->alive && x >= w->x && x < w->x + w->w &&
            y >= w->y && y < w->y + w->h)
            return g_dwm.zorder[j];
    }
    return -1;
}

/* -- Theme Helpers ------------------------------------------------ */

static const WubuThemeColors *tc(void) { return wubu_theme_colors(); }
static const WubuTheme *theme(void) { return wubu_theme_get(); }
static int title_bar_height(void) { return theme()->Luna_start_button ? 24 : DOSGUI_TITLE_H; }
static int taskbar_height_dynamic(void) { return theme()->Luna_start_button ? 30 : DOSGUI_TASK_H; }
static int border_width(void) { return theme()->rounded_buttons ? 3 : DOSGUI_BORDER; }
static int theme_radius(void) { return theme()->rounded_buttons ? 4 : 0; }

/* ================================================================
 * RENDERING — Themed Window Chrome
 * ================================================================ */

static void draw_desktop_bg(int fb_w, int fb_h) {
    (void)vbe_state();
    draw_wallpaper(fb_w, fb_h);
}

static void load_default_wallpaper(void) {
    if (!g_dwm.wallpaper) {
        g_dwm.wallpaper_w = g_dwm.screen_w;
        g_dwm.wallpaper_h = g_dwm.screen_h;
        g_dwm.wallpaper = (uint32_t*)malloc((size_t)g_dwm.wallpaper_w * g_dwm.wallpaper_h * 4);
        if (g_dwm.wallpaper) {
            for (int y = 0; y < g_dwm.wallpaper_h; y++) {
                for (int x = 0; x < g_dwm.wallpaper_w; x++) {
                    float fy = (float)y / g_dwm.wallpaper_h;
                    int r = (int)((0x00 * (1-fy) + 0x00 * fy));
                    int g = (int)((0x80 * (1-fy) + 0x40 * fy));
                    int b = (int)((0x80 * (1-fy) + 0x00 * fy));
                    uint32_t c = (uint32_t)((b << 16) | (g << 8) | r);
                    g_dwm.wallpaper[y * g_dwm.wallpaper_w + x] = c;
                }
            }
        }
        g_dwm.wallpaper_mode = 1;
    }
}

static void draw_wallpaper(int fb_w, int fb_h) {
    int task_h = taskbar_height_dynamic();
    
    if (!g_dwm.wallpaper) {
        vbe_fill_rect(0, 0, fb_w, fb_h - task_h, tc()->desktop_bg);
        return;
    }
    
    int mode = g_dwm.wallpaper_mode;
    if (mode == 0) {
        int x = (fb_w - g_dwm.wallpaper_w) / 2;
        int y = (fb_h - task_h - g_dwm.wallpaper_h) / 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        for (int dy = 0; dy < g_dwm.wallpaper_h && y + dy < fb_h - task_h; dy++) {
            for (int dx = 0; dx < g_dwm.wallpaper_w && x + dx < fb_w; dx++) {
                vbe_set_pixel(x + dx, y + dy, g_dwm.wallpaper[dy * g_dwm.wallpaper_w + dx]);
            }
        }
    } else if (mode == 1) {
        for (int y = 0; y < fb_h - task_h; y++) {
            for (int x = 0; x < fb_w; x++) {
                int sx = x % g_dwm.wallpaper_w;
                int sy = y % g_dwm.wallpaper_h;
                vbe_set_pixel(x, y, g_dwm.wallpaper[sy * g_dwm.wallpaper_w + sx]);
            }
        }
    } else {
        for (int y = 0; y < fb_h - task_h; y++) {
            for (int x = 0; x < fb_w; x++) {
                int sx = (x * g_dwm.wallpaper_w) / fb_w;
                int sy = (y * g_dwm.wallpaper_h) / (fb_h - task_h);
                vbe_set_pixel(x, y, g_dwm.wallpaper[sy * g_dwm.wallpaper_w + sx]);
            }
        }
    }
}

static int icon_grid_x(int x) {
    int grid_x = (x - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    if (grid_x < 0) grid_x = 0;
    if (grid_x > 15) grid_x = 15;
    return 20 + grid_x * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
}

static int icon_grid_y(int y) {
    int grid_y = (y - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    if (grid_y < 0) grid_y = 0;
    if (grid_y > 15) grid_y = 15;
    return 20 + grid_y * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
}

static void snap_icon_to_grid(DosGuiIcon *icon) {
    icon->x = icon_grid_x(icon->x);
    icon->y = icon_grid_y(icon->y);
    icon->grid_x = (icon->x - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    icon->grid_y = (icon->y - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
}

static void snap_window_to_gaad(DosGuiWindow *w) {
    (void)w;
}

/* ================================================================
 * RENDERING — Themed Window Chrome
 * ================================================================ */

static void draw_window(int idx) {
    DosGuiWindow *w = &g_dwm.windows[idx];
    if (!w->alive) return;
    bool active = (idx == g_dwm.focused_id);

    const int rad = theme_radius();
    const int tbh = title_bar_height();
    const int bw = border_width();

    if (!theme()->Luna_start_button || true) {
        vbe_shade_rect(w->x + 4, w->y + 4, w->w, w->h);
    }

    vbe_fill_rect_rounded(w->x, w->y, w->w, w->h, rad, tc()->win_face);
    if (rad > 0) vbe_rect_rounded(w->x, w->y, w->w, w->h, rad, tc()->border_dark);
    else vbe_rect(w->x, w->y, w->w, w->h, tc()->border_dark);

    if (theme()->gradient_title) {
        if (active) {
            vbe_hgradient(w->x + rad, w->y + rad, w->w - 2*rad, tbh - rad,
                          theme()->title_gradient.color_start,
                          theme()->title_gradient.color_end);
        } else {
            vbe_hgradient(w->x + rad, w->y + rad, w->w - 2*rad, tbh - rad,
                          theme()->title_gradient_ina.color_start,
                          theme()->title_gradient_ina.color_end);
        }
    } else {
        vbe_title_bar(w->x + rad, w->y + rad, w->w - 2*rad, tbh - rad, active);
    }

    uint32_t title_color = active ? tc()->win_title_text : tc()->win_title_text_ina;
    int text_x = w->x + 8;
    int text_y = w->y + rad + (tbh - rad - 8) / 2;
    vbe_draw_text(text_x, text_y, w->title, title_color, 1);

    int close_x = w->x + w->w - rad - 18;
    int close_y = w->y + rad + 2;
    vbe_fill_rect_rounded(close_x, close_y, 14, 12, 2, active ? tc()->border_darkest : tc()->btn_face);
    vbe_rect_rounded(close_x, close_y, 14, 12, 2, tc()->border_dark);
    vbe_draw_text(close_x + 5, close_y + 2, "X", active ? 0xFFFFFF : 0x808080, 1);

    if (theme()->Luna_start_button) {
        int max_x = close_x - 20;
        vbe_fill_rect_rounded(max_x, close_y, 14, 12, 2, active ? tc()->border_face : tc()->btn_face);
        vbe_rect_rounded(max_x, close_y, 14, 12, 2, tc()->border_dark);
        vbe_draw_text(max_x + 4, close_y + 2, "[ ]", active ? 0xFFFFFF : 0x808080, 1);
    }

    if (theme()->Luna_start_button) {
        int min_x = close_x - 40;
        vbe_fill_rect_rounded(min_x, close_y, 14, 12, 2, active ? tc()->border_face : tc()->btn_face);
        vbe_rect_rounded(min_x, close_y, 14, 12, 2, tc()->border_dark);
        vbe_draw_text(min_x + 5, close_y + 2, "_", active ? 0xFFFFFF : 0x808080, 1);
    }

    int cx = w->x + bw;
    int cy = w->y + tbh;
    int cw = w->w - 2 * bw;
    int ch = w->h - tbh - bw;

    vbe_fill_rect_rounded(cx, cy, cw, ch, rad, tc()->win_face);
    if (rad > 0) {
        vbe_3d_sunken_rounded_colors(cx - 1, cy - 1, cw + 2, ch + 2, rad + 1,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
    } else {
        vbe_3d_sunken_colors(cx - 1, cy - 1, cw + 2, ch + 2,
                              tc()->border_light, tc()->border_face,
                              tc()->border_dark, tc()->border_darkest);
    }

    if (w->on_draw) {
        w->on_draw(w, NULL, cw, ch);
    }
}

/* ================================================================
 * PUBLIC API
 * ================================================================ */

int dosgui_wm_init(int screen_w, int screen_h) {
    memset(&g_dwm, 0, sizeof(g_dwm));
    g_dwm.screen_w = screen_w;
    g_dwm.screen_h = screen_h;
    g_dwm.focused_id = -1;
    g_dwm.drag_id = -1;
    g_dwm.drag_icon_id = -1;
    g_dwm.current_desktop = 0;
    g_dwm.desktop_count = 9;
    g_dwm.systray_count = 0;
    g_dwm.notif_count = 0;
    g_dwm.next_notif_id = 1;
    g_dwm.notif_center_open = false;
    g_dwm.last_clock_update = 0;
    load_default_wallpaper();
    return 0;
}

void dosgui_wm_shutdown(void) {
    if (g_dwm.wallpaper) {
        free(g_dwm.wallpaper);
        g_dwm.wallpaper = NULL;
    }
    memset(&g_dwm, 0, sizeof(g_dwm));
}

DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h,
                                const char *title) {
    int i = spawn_window(x, y, w, h, title);
    if (i < 0) return NULL;
    return &g_dwm.windows[i];
}

void dosgui_wm_destroy(DosGuiWindow *win) {
    if (!win) return;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) { close_win(i); return; }
    }
}

void dosgui_wm_set_focus(DosGuiWindow *win) {
    if (!win) return;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) {
            raise_win(i);
            g_dwm.focused_id = i;
            return;
        }
    }
}

DosGuiWindow *dosgui_wm_get_focused(void) {
    if (g_dwm.focused_id < 0) return NULL;
    return &g_dwm.windows[g_dwm.focused_id];
}

DosGuiWindow *dosgui_wm_find_by_id(int id) {
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++)
        if (g_dwm.windows[i].alive && g_dwm.windows[i].id == id)
            return &g_dwm.windows[i];
    return NULL;
}

int dosgui_wm_window_count(void) {
    return g_dwm.nz;
}

DosGuiWindow *dosgui_wm_spawn(int x, int y, int w, int h,
                               const char *title,
                               void (*on_draw)(DosGuiWindow *, uint32_t *, int, int)) {
    int i = spawn_window(x, y, w, h, title);
    if (i < 0) return NULL;
    g_dwm.windows[i].on_draw = on_draw;
    return &g_dwm.windows[i];
}

/* -- Input ------------------------------------------------------- */

void dosgui_wm_handle_key(uint32_t key, uint32_t mods) {
    /* Alt+Tab: cycle through windows */
    bool alt_held = (mods & 0x08) != 0;
    if (alt_held && key == 0x09 && g_dwm.nz > 1) {
        /* Find current focused index in zorder */
        int cur_idx = 0;
        for (int j = 0; j < g_dwm.nz; j++) {
            if (g_dwm.zorder[j] == g_dwm.focused_id) { cur_idx = j; break; }
        }
        /* Focus next window (wrap around) */
        int next_idx = (cur_idx + 1) % g_dwm.nz;
        int next_id = g_dwm.zorder[next_idx];
        if (next_id >= 0 && next_id < DOSGUI_MAX_WINDOWS && g_dwm.windows[next_id].alive) {
            raise_win(next_id);
            g_dwm.focused_id = next_id;
        }
        return;
    }

    /* Win key (left or right): toggle start menu */
    if (key == 0xE05B || key == 0xE05C) {
        dosgui_startmenu_toggle();
        return;
    }

    /* Win+H: spawn HolyC terminal */
    if ((mods & 0x08) && (key == 0x48 || key == 'h' || key == 'H')) {
        dosgui_wm_spawn_holyc_term(100, 100, 700, 500);
        return;
    }

    /* First, try to dispatch to focused window */
    if (g_dwm.focused_id >= 0) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
        if (w->alive && w->on_key) {
            w->on_key(w, key, mods);
            return;
        }
    }
    
    /* Global hotkeys */
    if (key == 111 && g_dwm.focused_id >= 0) {
        close_win(g_dwm.focused_id);
    }
    if ((mods & 0x4) && key == 0x14) {
        wubu_theme_cycle();
        fprintf(stderr, "Theme cycled to: %s\n", wubu_theme_name(wubu_theme_current()));
    }
    if (key == 0x3F) {
        wubu_theme_cycle();
        fprintf(stderr, "Theme cycled to: %s\n", wubu_theme_name(wubu_theme_current()));
    }
    if (key == 0x57 && g_dwm.focused_id >= 0) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
        if (w->flags & DOSGUI_WIN_MAXIMIZED) {
            w->x = w->min_x; w->y = w->min_y;
            w->w = w->min_w; w->h = w->min_h;
            w->flags &= ~DOSGUI_WIN_MAXIMIZED;
        } else {
            w->min_x = w->x; w->min_y = w->y;
            w->min_w = w->w; w->min_h = w->h;
            w->x = 0; w->y = 0;
            w->w = g_dwm.screen_w; w->h = g_dwm.screen_h - taskbar_height_dynamic();
            w->flags |= DOSGUI_WIN_MAXIMIZED;
        }
    }
    if ((mods & 0x4) && (mods & 0x8)) {
        if (key == 0xE04B) {
            g_dwm.current_desktop = (g_dwm.current_desktop - 1 + g_dwm.desktop_count) % g_dwm.desktop_count;
        } else if (key == 0xE04D) {
            g_dwm.current_desktop = (g_dwm.current_desktop + 1) % g_dwm.desktop_count;
        }
    }
}

void dosgui_wm_handle_mouse(int x, int y, int btn, int kind) {
    (void)btn;
    g_dwm.mouse_x = x;
    g_dwm.mouse_y = y;

    int task_h = taskbar_height_dynamic();
    int tbh = title_bar_height();
    (void)border_width();

    if (y >= g_dwm.screen_h - task_h) {
        int by = g_dwm.screen_h - task_h + (task_h - 24) / 2;
        int start_w = theme()->Luna_start_button ? 54 : 60;
        
        if (x >= 4 && x < 4 + start_w + 20 && y >= by && y < by + 24) {
            dosgui_startmenu_toggle();
            return;
        }
        
        int bx = theme()->Luna_start_button ? 82 : 72;
        for (int j = 0; j < g_dwm.nz; j++) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
            if (!w->alive || (w->flags & DOSGUI_WIN_MINIMIZED)) continue;
            int bw = (int)strlen(w->title) * 6 + 16;
            if (bw > 160) bw = 160;
            if (x >= bx && x < bx + bw && y >= by && y < by + 22) {
                if (w->flags & DOSGUI_WIN_MINIMIZED) {
                    w->flags &= ~DOSGUI_WIN_MINIMIZED;
                } else if (g_dwm.focused_id == g_dwm.zorder[j]) {
                    w->flags |= DOSGUI_WIN_MINIMIZED;
                } else {
                    dosgui_wm_set_focus(w);
                }
                return;
            }
            bx += bw + 2;
            if (bx > g_dwm.screen_w - 160) break;
        }
        
        int desk_x = g_dwm.screen_w - 150;
        for (int d = 0; d < g_dwm.desktop_count; d++) {
            int dx = desk_x + d * 16;
            if (x >= dx && x < dx + 14 && y >= by && y < by + 16) {
                g_dwm.current_desktop = d;
                return;
            }
        }

        /* Check system tray icons */
        int tray_x = g_dwm.screen_w - 10;
        dosgui_taskbar_update_clock(time(NULL));
        char *clk = dosgui_taskbar_get_clock_str();
        int clk_w = vbe_text_width(clk, 1);
        tray_x -= clk_w + 10;

        for (int i = g_dwm.systray_count - 1; i >= 0; i--) {
            if (g_dwm.systray_icons[i].visible) {
                int sx = tray_x - DOSGUI_SYSTRAY_SIZE - 4;
                int sy = g_dwm.screen_h - task_h + (task_h - DOSGUI_SYSTRAY_SIZE) / 2;
                if (x >= sx && x < sx + DOSGUI_SYSTRAY_SIZE && y >= sy && y < sy + DOSGUI_SYSTRAY_SIZE) {
                    if (kind == 1 && g_dwm.systray_icons[i].on_click) {
                        g_dwm.systray_icons[i].on_click();
                    } else if (kind == 1 && btn == 2 && g_dwm.systray_icons[i].on_right_click) {
                        g_dwm.systray_icons[i].on_right_click();
                    }
                    return;
                }
                tray_x -= DOSGUI_SYSTRAY_SIZE + 4;
            }
        }

        /* Check notification center toggle (far right before clock) */
        if (x >= tray_x - 30 && x < tray_x && y >= by && y < by + 22) {
            dosgui_notif_center_toggle();
            return;
        }

        return;
    }

    if (kind == 1) {
        if (y >= g_dwm.screen_h - task_h) {
            return;
        }
        
        for (int j = g_dwm.nz - 1; j >= 0; j--) {
            int idx = g_dwm.zorder[j];
            DosGuiWindow *w = &g_dwm.windows[idx];
            if (!w->alive || (w->flags & DOSGUI_WIN_MINIMIZED)) continue;
            if (!(w->flags & DOSGUI_WIN_MAXIMIZED)) {
                int close_x = w->x + w->w - theme_radius() - 18;
                int close_y = w->y + theme_radius() + 2;
                if (x >= close_x && x < close_x + 14 && y >= close_y && y < close_y + 12) {
                    close_win(idx);
                    return;
                }
                if (theme()->Luna_start_button) {
                    int max_x = close_x - 20;
                    if (x >= max_x && x < max_x + 14 && y >= close_y && y < close_y + 12) {
                        if (w->flags & DOSGUI_WIN_MAXIMIZED) {
                            w->x = w->min_x; w->y = w->min_y;
                            w->w = w->min_w; w->h = w->min_h;
                            w->flags &= ~DOSGUI_WIN_MAXIMIZED;
                        } else {
                            w->min_x = w->x; w->min_y = w->y;
                            w->min_w = w->w; w->min_h = w->h;
                            w->x = 0; w->y = 0;
                            w->w = g_dwm.screen_w; w->h = g_dwm.screen_h - task_h;
                            w->flags |= DOSGUI_WIN_MAXIMIZED;
                        }
                        return;
                    }
                    int min_x = close_x - 40;
                    if (x >= min_x && x < min_x + 14 && y >= close_y && y < close_y + 12) {
                        w->flags |= DOSGUI_WIN_MINIMIZED;
                        return;
                    }
                }
            }
        }

        int i = hit_test(x, y);
        if (i < 0) {
            int icon_idx = dosgui_icon_hit_test(x, y);
            if (icon_idx >= 0) {
                if (btn == 2) { /* Right click */
                    dosgui_icon_show_context_menu(icon_idx, x, y);
                    return;
                }
                if (g_dwm.icons[icon_idx].on_click) {
                    g_dwm.icons[icon_idx].on_click();
                } else if (g_dwm.icons[icon_idx].on_execute) {
                    g_dwm.icons[icon_idx].on_execute();
                }
                g_dwm.drag_icon_id = icon_idx;
                g_dwm.drag_icon_ox = x - g_dwm.icons[icon_idx].x;
                g_dwm.drag_icon_oy = y - g_dwm.icons[icon_idx].y;
                g_dwm.focused_id = -1;
                return;
            }
            g_dwm.focused_id = -1;
            if (btn == 2) { /* Right click on empty desktop */
                dosgui_desktop_show_context_menu(x, y);
                return;
            }
            return;
        }

        raise_win(i);
        g_dwm.focused_id = i;
        DosGuiWindow *w = &g_dwm.windows[i];

        int close_x = w->x + w->w - theme_radius() - 18;
        int close_y = w->y + theme_radius() + 2;
        if (x >= close_x && x < close_x + 14 && y >= close_y && y < close_y + 12) {
            close_win(i);
            return;
        }

        if (theme()->Luna_start_button) {
            int max_x = close_x - 20;
            if (x >= max_x && x < max_x + 14 && y >= close_y && y < close_y + 12) {
                if (w->flags & DOSGUI_WIN_MAXIMIZED) {
                    w->x = w->min_x; w->y = w->min_y;
                    w->w = w->min_w; w->h = w->min_h;
                    w->flags &= ~DOSGUI_WIN_MAXIMIZED;
                } else {
                    w->min_x = w->x; w->min_y = w->y;
                    w->min_w = w->w; w->min_h = w->h;
                    w->x = 0; w->y = 0;
                    w->w = g_dwm.screen_w; w->h = g_dwm.screen_h - task_h;
                    w->flags |= DOSGUI_WIN_MAXIMIZED;
                }
                return;
            }
            int min_x = close_x - 40;
            if (x >= min_x && x < min_x + 14 && y >= close_y && y < close_y + 12) {
                w->flags |= DOSGUI_WIN_MINIMIZED;
                return;
            }
        }

        if (y < w->y + tbh) {
            g_dwm.drag_id = i;
            g_dwm.drag_ox = x - w->x;
            g_dwm.drag_oy = y - w->y;
        } else {
            /* Client area click - dispatch to window */
            if (w->on_mouse) {
                w->on_mouse(w, x - w->x, y - w->y, btn, kind);
            }
        }
    } else if (kind == 2) {
        g_dwm.drag_id = -1;
        if (g_dwm.drag_icon_id >= 0) {
            snap_icon_to_grid(&g_dwm.icons[g_dwm.drag_icon_id]);
            g_dwm.drag_icon_id = -1;
        }
    } else if (kind == 0) {
        if (g_dwm.drag_id >= 0 && g_dwm.windows[g_dwm.drag_id].alive) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.drag_id];
            if (!(w->flags & DOSGUI_WIN_MAXIMIZED)) {
                w->x = x - g_dwm.drag_ox;
                w->y = y - g_dwm.drag_oy;
                if (w->x < -w->w + 60) w->x = -w->w + 60;
                if (w->x > g_dwm.screen_w - 60) w->x = g_dwm.screen_w - 60;
                if (w->y < 0) w->y = 0;
                if (w->y > g_dwm.screen_h - task_h - tbh)
                    w->y = g_dwm.screen_h - task_h - tbh;
            }
        } else {
            /* Mouse move over client area - dispatch to focused window */
            if (g_dwm.focused_id >= 0) {
                DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
                if (w->alive && w->on_mouse) {
                    w->on_mouse(w, x - w->x, y - w->y, btn, kind);
                }
            }
        }
        if (g_dwm.drag_icon_id >= 0) {
            DosGuiIcon *icon = &g_dwm.icons[g_dwm.drag_icon_id];
            icon->x = x - g_dwm.drag_icon_ox;
            icon->y = y - g_dwm.drag_icon_oy;
            if (icon->x < 0) icon->x = 0;
            if (icon->x > g_dwm.screen_w - DOSGUI_ICON_SIZE) icon->x = g_dwm.screen_w - DOSGUI_ICON_SIZE;
            if (icon->y < 0) icon->y = 0;
            if (icon->y > g_dwm.screen_h - task_h - DOSGUI_ICON_SIZE) icon->y = g_dwm.screen_h - task_h - DOSGUI_ICON_SIZE;
        }
    }
}

/* -- Desktop Icons ------------------------------------------------------ */

int dosgui_icon_add(const char *name, int gx, int gy,
                        void (*on_click)(void)) {
    if (g_dwm.icon_count >= DOSGUI_MAX_ICONS) return -1;
    DosGuiIcon *icon = &g_dwm.icons[g_dwm.icon_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->grid_x = gx; icon->grid_y = gy;
    icon->x = 20 + gx * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    icon->y = 20 + gy * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    icon->on_click = on_click;
    icon->type = DESK_ICON_APP;
    icon->icon_color = 0x0080FF;  /* Default blue */
    return g_dwm.icon_count++;
}

int dosgui_icon_add_ex(const char *name, DeskIconType type,
                        const char *target, int gx, int gy,
                        uint32_t icon_color, void (*on_execute)(void)) {
    if (g_dwm.icon_count >= DOSGUI_MAX_ICONS) return -1;
    DosGuiIcon *icon = &g_dwm.icons[g_dwm.icon_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->grid_x = gx; icon->grid_y = gy;
    icon->x = 20 + gx * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    icon->y = 20 + gy * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    icon->type = type;
    icon->icon_color = icon_color ? icon_color : 0x0080FF;
    if (target) strncpy(icon->target, target, sizeof(icon->target) - 1);
    icon->on_execute = on_execute;
    icon->alive = true;
    return g_dwm.icon_count++;
}

void dosgui_icon_remove(int grid_x, int grid_y) {
    for (int i = 0; i < g_dwm.icon_count; i++) {
        if (g_dwm.icons[i].alive && g_dwm.icons[i].grid_x == grid_x && g_dwm.icons[i].grid_y == grid_y) {
            g_dwm.icons[i].alive = false;
            /* Compact array */
            for (int j = i; j < g_dwm.icon_count - 1; j++) {
                g_dwm.icons[j] = g_dwm.icons[j + 1];
            }
            g_dwm.icon_count--;
            return;
        }
    }
}

int dosgui_icon_find_at(int grid_x, int grid_y) {
    for (int i = 0; i < g_dwm.icon_count; i++) {
        if (g_dwm.icons[i].alive && g_dwm.icons[i].grid_x == grid_x && g_dwm.icons[i].grid_y == grid_y) {
            return i;
        }
    }
    return -1;
}

void dosgui_icon_set_position(int grid_x, int grid_y, int new_gx, int new_gy) {
    int idx = dosgui_icon_find_at(grid_x, grid_y);
    if (idx >= 0) {
        DosGuiIcon *icon = &g_dwm.icons[idx];
        /* Check if target position is occupied */
        if (dosgui_icon_find_at(new_gx, new_gy) < 0) {
            icon->grid_x = new_gx;
            icon->grid_y = new_gy;
            icon->x = 20 + new_gx * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
            icon->y = 20 + new_gy * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
        }
    }
}

/* Shortcut Creation */

int dosgui_shortcut_create(const char *name, const char *target,
                            const char *description, int grid_x, int grid_y) {
    (void)description;
    return dosgui_icon_add_ex(name, DESK_ICON_SHORTCUT, target, grid_x, grid_y, 0x00FF00, NULL);
}

int dosgui_shortcut_create_url(const char *name, const char *url, int grid_x, int grid_y) {
    return dosgui_icon_add_ex(name, DESK_ICON_URL, url, grid_x, grid_y, 0xFF8000, NULL);
}

int dosgui_icon_hit_test(int mx, int my) {
    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *ic = &g_dwm.icons[i];
        if (mx >= ic->x && mx < ic->x + DOSGUI_ICON_SIZE &&
            my >= ic->y && my < ic->y + DOSGUI_ICON_SIZE)
            return i;
    }
    return -1;
}

/* -- Taskbar ----------------------------------------------------- */

int dosgui_taskbar_height(void) { return taskbar_height_dynamic(); }

void dosgui_taskbar_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    int th = taskbar_height_dynamic();
    int ty = fb_h - th;

    vbe_fill_rect(0, ty, fb_w, th, tc()->taskbar_bg);
    vbe_hline(0, fb_w - 1, ty, tc()->taskbar_border);

    int by = ty + (th - 24) / 2;
    int start_w = theme()->Luna_start_button ? 54 : 60;
    
    if (theme()->Luna_start_button) {
        vbe_fill_rect_rounded(4, by, start_w + 20, 24, 4, tc()->start_btn_face);
        vbe_3d_raised_rounded_colors(4, by, start_w + 20, 24, 4,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(8, by + 8, "Start", tc()->start_btn_text, 1);
    } else {
        vbe_fill_rect(4, by, 60, 22, tc()->start_btn_face);
        vbe_3d_raised_colors(4, by, 60, 22,
                              tc()->border_light, tc()->border_face,
                              tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(8, by + 6, "+ NEW", tc()->start_btn_text, 1);
    }

    int bx = theme()->Luna_start_button ? 82 : 72;
    for (int j = 0; j < g_dwm.nz; j++) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
        if (!w->alive || (w->flags & DOSGUI_WIN_MINIMIZED)) continue;
        int bw = (int)strlen(w->title) * 6 + 16;
        if (bw > 160) bw = 160;
        bool focused = (g_dwm.zorder[j] == g_dwm.focused_id);
        
        if (theme()->rounded_buttons) {
            if (focused) {
                vbe_fill_rect_rounded(bx, by, bw, 22, 3, tc()->select_bg);
                vbe_3d_sunken_rounded_colors(bx, by, bw, 22, 3,
                                              tc()->border_light, tc()->border_face,
                                              tc()->border_dark, tc()->border_darkest);
                vbe_draw_text(bx + 8, by + 7, w->title, tc()->select_text, 1);
            } else {
                vbe_fill_rect_rounded(bx, by, bw, 22, 3, tc()->btn_face);
                vbe_3d_raised_rounded_colors(bx, by, bw, 22, 3,
                                              tc()->border_light, tc()->border_face,
                                              tc()->border_dark, tc()->border_darkest);
                vbe_draw_text(bx + 8, by + 7, w->title, tc()->btn_text, 1);
            }
        } else {
            if (focused) {
                vbe_fill_rect(bx, by, bw, 22, 0x000080);
                vbe_3d_sunken_colors(bx, by, bw, 22,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
                vbe_draw_text(bx + 8, by + 6, w->title, 0xFFFFFF, 1);
            } else {
                vbe_fill_rect(bx, by, bw, 22, tc()->btn_face);
                vbe_3d_raised_colors(bx, by, bw, 22,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
                vbe_draw_text(bx + 8, by + 6, w->title, tc()->btn_text, 1);
            }
        }
        bx += bw + 2;
        if (bx > g_dwm.screen_w - 160) break;
    }

    /* System tray icons (drawn from right to left, before clock) */
    int tray_x = fb_w - 10;

    /* Clock */
    dosgui_taskbar_update_clock(time(NULL));
    char *clk = dosgui_taskbar_get_clock_str();
    int clk_w = vbe_text_width(clk, 1);
    tray_x -= clk_w + 10;

    vbe_draw_text(tray_x, ty + (th - 8) / 2, clk,
                  theme()->Luna_start_button ? 0xFFFFFF : tc()->icon_text, 1);

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

    vbe_draw_text(fb_w - clk_w - 10, ty + (th - 8) / 2, clk, 
                  theme()->Luna_start_button ? 0xFFFFFF : tc()->icon_text, 1);
}

/* -- Full Render ------------------------------------------------- */

void dosgui_wm_render(uint32_t *fb, int fb_w, int fb_h) {
    dosgui_wm_render_desktop(fb, fb_w, fb_h);
}

void dosgui_wm_render_desktop(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    draw_desktop_bg(fb_w, fb_h);

    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *icon = &g_dwm.icons[i];
        vbe_fill_rect(icon->x, icon->y, DOSGUI_ICON_SIZE, DOSGUI_ICON_SIZE, tc()->icon_bg);
        vbe_rect(icon->x, icon->y, DOSGUI_ICON_SIZE, DOSGUI_ICON_SIZE, tc()->icon_border);
        vbe_draw_text(icon->x + 1, icon->y + DOSGUI_ICON_SIZE + 3, icon->name,
                      tc()->icon_text_shadow, 1);
        vbe_draw_text(icon->x, icon->y + DOSGUI_ICON_SIZE + 2, icon->name,
                      tc()->icon_text, 1);
    }

    for (int j = 0; j < g_dwm.nz; j++)
        draw_window(g_dwm.zorder[j]);

    dosgui_taskbar_render(fb, fb_w, fb_h);

    /* Render notification center if open (on top of everything) */
    dosgui_notif_center_render(fb, fb_w, fb_h);

    vbe_draw_cursor(g_dwm.mouse_x, g_dwm.mouse_y);
}

/* ================================================================
 * SYSTEM TRAY / NOTIFICATION AREA
 * ================================================================ */

static void draw_systray_icon(int idx, int ty, int th) {
    DosGuiSysTrayIcon *icon = &g_dwm.systray_icons[idx];
    if (!icon->visible) return;

    int x = g_dwm.screen_w - 50 - idx * (DOSGUI_SYSTRAY_SIZE + 4);
    int y = ty + (th - DOSGUI_SYSTRAY_SIZE) / 2;

    /* Draw icon background */
    vbe_fill_rect(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE, tc()->btn_face);
    vbe_3d_raised_colors(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE,
                         tc()->border_light, tc()->border_face,
                         tc()->border_dark, tc()->border_darkest);

    /* Draw simple colored square as icon */
    vbe_fill_rect(x + 4, y + 4, 16, 16, icon->icon_color);

    /* Draw notification badge if count > 0 */
    if (icon->notification_count > 0) {
        char badge[8];
        snprintf(badge, sizeof(badge), "%d", icon->notification_count > 9 ? 9 : icon->notification_count);
        int bx = x + DOSGUI_SYSTRAY_SIZE - 8;
        int by = y;
        vbe_fill_rect_rounded(bx, by, 12, 12, 6, 0xFF0000);
        vbe_draw_text(bx + 2, by + 1, badge, 0xFFFFFF, 1);
    }
}

int dosgui_systray_add(const char *name, uint32_t color,
                        void (*on_click)(void),
                        void (*on_right_click)(void)) {
    if (g_dwm.systray_count >= DOSGUI_MAX_SYSTRAY_ICONS) return -1;

    DosGuiSysTrayIcon *icon = &g_dwm.systray_icons[g_dwm.systray_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->icon_color = color;
    icon->visible = true;
    icon->on_click = on_click;
    icon->on_right_click = on_right_click;
    icon->notification_count = 0;

    return g_dwm.systray_count++;
}

void dosgui_systray_remove(const char *name) {
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (strcmp(g_dwm.systray_icons[i].name, name) == 0) {
            for (int j = i; j < g_dwm.systray_count - 1; j++) {
                g_dwm.systray_icons[j] = g_dwm.systray_icons[j + 1];
            }
            g_dwm.systray_count--;
            return;
        }
    }
}

void dosgui_systray_set_notification_count(const char *name, int count) {
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (strcmp(g_dwm.systray_icons[i].name, name) == 0) {
            g_dwm.systray_icons[i].notification_count = count;
            return;
        }
    }
}

/* ================================================================
 * NOTIFICATION CENTER
 * ================================================================ */

int dosgui_notif_center_add(const char *app_name, const char *summary,
                             const char *body, int urgency) {
    if (g_dwm.notif_count >= DOSGUI_NOTIF_CENTER_MAX) {
        /* Shift oldest out */
        for (int i = 1; i < g_dwm.notif_count; i++) {
            g_dwm.notifications[i - 1] = g_dwm.notifications[i];
        }
        g_dwm.notif_count--;
    }

    DosGuiNotification *n = &g_dwm.notifications[g_dwm.notif_count];
    memset(n, 0, sizeof(*n));
    n->id = g_dwm.next_notif_id++;
    strncpy(n->app_name, app_name, sizeof(n->app_name) - 1);
    strncpy(n->summary, summary, sizeof(n->summary) - 1);
    if (body) strncpy(n->body, body, sizeof(n->body) - 1);
    n->timestamp = (uint32_t)time(NULL);
    n->urgency = urgency;
    n->read = false;
    n->expanded = false;

    g_dwm.notif_count++;

    /* Update systray notification badge */
    dosgui_systray_set_notification_count("Notifications", g_dwm.notif_count);

    /* Also send to wubu_notify daemon if available */
    (void)wubu_notify_simple(app_name, summary, body ? body : "",
                              NULL, urgency, urgency == 2 ? 0 : 5000);

    return n->id;
}

void dosgui_notif_center_mark_read(uint32_t id) {
    for (int i = 0; i < g_dwm.notif_count; i++) {
        if (g_dwm.notifications[i].id == id) {
            g_dwm.notifications[i].read = true;
            return;
        }
    }
}

void dosgui_notif_center_clear(void) {
    g_dwm.notif_count = 0;
    dosgui_systray_set_notification_count("Notifications", 0);
}

bool dosgui_notif_center_is_open(void) {
    return g_dwm.notif_center_open;
}

void dosgui_notif_center_toggle(void) {
    g_dwm.notif_center_open = !g_dwm.notif_center_open;
    /* Mark all as read when opening */
    if (g_dwm.notif_center_open) {
        for (int i = 0; i < g_dwm.notif_count; i++) {
            g_dwm.notifications[i].read = true;
        }
        dosgui_systray_set_notification_count("Notifications", 0);
    }
}

void dosgui_notif_center_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_dwm.notif_center_open) return;

    int th = taskbar_height_dynamic();
    int ty = fb_h - th;

    /* Draw panel on right side, above taskbar */
    int panel_w = 350;
    int panel_h = fb_h - th;
    int panel_x = fb_w - panel_w;
    int panel_y = ty - panel_h;

    vbe_fill_rect_rounded(panel_x, panel_y, panel_w, panel_h, 8, tc()->win_face);
    vbe_3d_sunken_rounded_colors(panel_x, panel_y, panel_w, panel_h, 8,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);

    /* Header */
    vbe_fill_rect_rounded(panel_x + 4, panel_y + 4, panel_w - 8, 30, 4, tc()->select_bg);
    vbe_draw_text(panel_x + 10, panel_y + 10, "Notification Center", tc()->select_text, 1);

    /* Notifications list */
    int ny = panel_y + 40;
    for (int i = 0; i < g_dwm.notif_count; i++) {
        DosGuiNotification *n = &g_dwm.notifications[i];
        if (ny + 60 > panel_y + panel_h - 10) break;

        uint32_t bg = n->read ? 0xFF303030 : tc()->select_bg;
        vbe_fill_rect_rounded(panel_x + 4, ny, panel_w - 8, 56, 4, bg);
        vbe_3d_raised_rounded_colors(panel_x + 4, ny, panel_w - 8, 56, 4,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);

        /* Urgency indicator */
        uint32_t urg_color = (n->urgency == 2) ? 0xFF0000 : (n->urgency == 1 ? 0xFFFF00 : 0x00FF00);
        vbe_fill_rect(panel_x + 6, ny + 6, 4, 44, urg_color);

        /* App name */
        vbe_draw_text(panel_x + 14, ny + 6, n->app_name, tc()->icon_text, 1);

        /* Summary */
        vbe_draw_text(panel_x + 14, ny + 18, n->summary, n->read ? tc()->icon_text_shadow : tc()->win_title_text, 1);

        /* Body */
        if (n->body[0]) {
            vbe_draw_text(panel_x + 14, ny + 30, n->body, tc()->icon_text_shadow, 1);
        }

        /* Time */
        char time_str[16];
        time_t t = n->timestamp;
        struct tm *tm = localtime(&t);
        snprintf(time_str, sizeof(time_str), "%02d:%02d", tm->tm_hour, tm->tm_min);
        vbe_draw_text(panel_x + panel_w - 60, ny + 6, time_str, tc()->icon_text_shadow, 1);

        ny += 60;
    }
}

/* ================================================================
 * CLOCK
 * ================================================================ */

void dosgui_taskbar_update_clock(time_t now) {
    g_dwm.last_clock_update = now;
}

char *dosgui_taskbar_get_clock_str(void) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    static char clk[16];
    snprintf(clk, sizeof(clk), "%02d:%02d", tm->tm_hour, tm->tm_min);
    return clk;
}

/* Global context menu stack */
DosGuiContextMenu *g_dosgui_ctx_stack = NULL;

/* -- Context Menu Stack Management -- */

static void ctx_menu_push(DosGuiContextMenu *menu) {
    menu->parent = g_dosgui_ctx_stack;
    g_dosgui_ctx_stack = menu;
}

static void ctx_menu_pop(void) {
    if (g_dosgui_ctx_stack) {
        DosGuiContextMenu *old = g_dosgui_ctx_stack;
        g_dosgui_ctx_stack = old->parent;
        old->parent = NULL;
    }
}

DosGuiContextMenu *dosgui_ctx_menu_create(int x, int y) {
    DosGuiContextMenu *menu = (DosGuiContextMenu*)calloc(1, sizeof(DosGuiContextMenu));
    if (!menu) return NULL;
    menu->x = x;
    menu->y = y;
    menu->visible = false;
    menu->selected_item = -1;
    menu->item_count = 0;
    return menu;
}

void dosgui_ctx_menu_add_item(DosGuiContextMenu *menu, const char *label,
                               void (*action)(void)) {
    if (!menu || menu->item_count >= DOSGUI_MAX_CTX_ITEMS) return;
    DosGuiCtxItem *item = &menu->items[menu->item_count];
    item->type = CTX_ITEM_ACTION;
    item->action = action;
    strncpy(item->label, label, sizeof(item->label) - 1);
    item->disabled = false;
    item->checked = false;
    menu->item_count++;
}

void dosgui_ctx_menu_add_separator(DosGuiContextMenu *menu) {
    if (!menu || menu->item_count >= DOSGUI_MAX_CTX_ITEMS) return;
    DosGuiCtxItem *item = &menu->items[menu->item_count];
    item->type = CTX_ITEM_SEPARATOR;
    menu->item_count++;
}

DosGuiContextMenu *dosgui_ctx_menu_add_submenu(DosGuiContextMenu *menu, const char *label) {
    if (!menu || menu->item_count >= DOSGUI_MAX_CTX_ITEMS) return NULL;
    DosGuiContextMenu *submenu = dosgui_ctx_menu_create(0, 0);
    if (!submenu) return NULL;
    DosGuiCtxItem *item = &menu->items[menu->item_count];
    item->type = CTX_ITEM_SUBMENU;
    strncpy(item->label, label, sizeof(item->label) - 1);
    item->submenu = submenu;
    menu->item_count++;
    return submenu;
}

void dosgui_ctx_menu_show(DosGuiContextMenu *menu, int x, int y) {
    if (!menu) return;
    menu->x = x;
    menu->y = y;
    menu->visible = true;
    menu->selected_item = 0;
    /* Find first non-separator item */
    for (int i = 0; i < menu->item_count; i++) {
        if (menu->items[i].type != CTX_ITEM_SEPARATOR) {
            menu->selected_item = i;
            break;
        }
    }
    ctx_menu_push(menu);
}

void dosgui_ctx_menu_hide(DosGuiContextMenu *menu) {
    if (!menu) return;
    menu->visible = false;
    if (g_dosgui_ctx_stack == menu) {
        ctx_menu_pop();
    }
}

void dosgui_ctx_menu_handle_mouse(int x, int y, int btn, int kind) {
    if (!g_dosgui_ctx_stack) return;
    
    DosGuiContextMenu *menu = g_dosgui_ctx_stack;
    int item_h = 24;
    int menu_w = 180;
    int menu_x = menu->x;
    int menu_y = menu->y;
    
    /* Check if click is outside menu */
    if (x < menu_x || x >= menu_x + menu_w || y < menu_y || y >= menu_y + menu->item_count * item_h) {
        /* Pop all menus */
        while (g_dosgui_ctx_stack) {
            ctx_menu_pop();
        }
        return;
    }
    
    if (kind == 0) { /* Mouse move */
        int item = (y - menu_y) / item_h;
        if (item >= 0 && item < menu->item_count && menu->items[item].type != CTX_ITEM_SEPARATOR) {
            menu->selected_item = item;
        }
    } else if (kind == 1) { /* Mouse down */
        int item = (y - menu_y) / item_h;
        if (item >= 0 && item < menu->item_count) {
            DosGuiCtxItem *it = &menu->items[item];
            if (it->type == CTX_ITEM_ACTION && it->action && !it->disabled) {
                it->action();
                while (g_dosgui_ctx_stack) ctx_menu_pop();
            } else if (it->type == CTX_ITEM_SUBMENU && it->submenu) {
                /* Show submenu to the right */
                dosgui_ctx_menu_show(it->submenu, menu_x + menu_w, menu_y + item * item_h);
            }
        }
    }
}

void dosgui_ctx_menu_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    
    DosGuiContextMenu *menu = g_dosgui_ctx_stack;
    while (menu) {
        if (!menu->visible) {
            menu = menu->parent;
            continue;
        }
        
        int item_h = 24;
        int menu_w = 180;
        int menu_h = menu->item_count * item_h;
        int mx = menu->x;
        int my = menu->y;
        
        /* Clamp to screen */
        if (mx + menu_w > fb_w) mx = fb_w - menu_w;
        if (my + menu_h > fb_h) my = fb_h - menu_h;
        if (mx < 0) mx = 0;
        if (my < 0) my = 0;
        menu->x = mx;
        menu->y = my;
        
        /* Draw menu background */
        vbe_fill_rect_rounded(mx, my, menu_w, menu_h, 4, tc()->win_face);
        vbe_3d_sunken_rounded_colors(mx, my, menu_w, menu_h, 4,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
        
        /* Draw items */
        for (int i = 0; i < menu->item_count; i++) {
            int y = my + i * item_h;
            DosGuiCtxItem *it = &menu->items[i];
            
            if (it->type == CTX_ITEM_SEPARATOR) {
                vbe_hline(mx + 10, mx + menu_w - 10, y + item_h / 2, tc()->border_dark);
                continue;
            }
            
            /* Highlight selected */
            if (i == menu->selected_item && it->type != CTX_ITEM_SUBMENU) {
                vbe_fill_rect(mx + 2, y, menu_w - 4, item_h, tc()->select_bg);
            }
            
            /* Draw label */
            uint32_t text_color = it->disabled ? 0x808080 : tc()->win_title_text;
            vbe_draw_text(mx + 10, y + (item_h - 8) / 2, it->label, text_color, 1);
            
            /* Draw submenu indicator */
            if (it->type == CTX_ITEM_SUBMENU && it->submenu) {
                vbe_draw_text(mx + menu_w - 20, y + (item_h - 8) / 2, ">", text_color, 1);
            }
            
            /* Draw checkmark */
            if (it->checked) {
                vbe_draw_text(mx + 2, y + (item_h - 8) / 2, "*", text_color, 1);
            }
        }
        
        menu = menu->parent;
    }
}

/* -- Default Context Menu Actions -- */

static void ctx_action_open(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0 && g_dwm.icons[idx].on_execute) {
        g_dwm.icons[idx].on_execute();
    }
}

static void ctx_action_rename(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0) {
        (void)wubu_notify_simple("Desktop", "Rename", "F2 to rename (stub)", NULL, 1, 3000);
    }
}

static void ctx_action_delete(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0) {
        /* Confirm dialog would go here */
        g_dwm.icons[idx].alive = false;
        while (g_dosgui_ctx_stack) ctx_menu_pop();
    }
}

static void ctx_action_properties(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0) {
        DosGuiIcon *ic = &g_dwm.icons[idx];
        char msg[512];
        snprintf(msg, sizeof(msg), "Name: %s\nType: %d\nTarget: %s", ic->name, ic->type, ic->target);
        (void)wubu_notify_simple("Properties", ic->name, msg, NULL, 1, 5000);
    }
}

static void ctx_action_create_shortcut(void) {
    (void)wubu_notify_simple("Desktop", "Create Shortcut", "Right-click empty space -> New -> Shortcut (stub)", NULL, 1, 3000);
}

static void ctx_action_view_desktop(void) {
    (void)wubu_notify_simple("Desktop", "View", "Desktop view options (stub)", NULL, 1, 3000);
}

static void ctx_action_sort_by_name(void) {
    /* Would sort icons by name */
}

static void ctx_action_refresh(void) {
    (void)wubu_notify_simple("Desktop", "Refresh", "Desktop refreshed", NULL, 1, 2000);
}

/* -- Show Icon Context Menu -- */

void dosgui_icon_show_context_menu(int icon_idx, int mx, int my) {
    if (icon_idx < 0 || icon_idx >= DOSGUI_MAX_ICONS) return;
    if (!g_dwm.icons[icon_idx].alive) return;
    
    DosGuiContextMenu *menu = dosgui_ctx_menu_create(mx, my);
    if (!menu) return;
    
    /* Select the icon */
    g_dwm.icons[icon_idx].selected = true;
    
    dosgui_ctx_menu_add_item(menu, "Open", ctx_action_open);
    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "Rename", ctx_action_rename);
    dosgui_ctx_menu_add_item(menu, "Delete", ctx_action_delete);
    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "Properties", ctx_action_properties);
    
    dosgui_ctx_menu_show(menu, mx, my);
}

void dosgui_desktop_show_context_menu(int mx, int my) {
    DosGuiContextMenu *menu = dosgui_ctx_menu_create(mx, my);
    if (!menu) return;
    
    dosgui_ctx_menu_add_item(menu, "New", NULL);
    
    DosGuiContextMenu *newmenu = dosgui_ctx_menu_add_submenu(menu, "New");
    dosgui_ctx_menu_add_item(newmenu, "Shortcut", ctx_action_create_shortcut);
    dosgui_ctx_menu_add_item(newmenu, "Folder", NULL);
    dosgui_ctx_menu_add_item(newmenu, "Text Document", NULL);
    
    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "View", ctx_action_view_desktop);
    
    DosGuiContextMenu *viewmenu = dosgui_ctx_menu_add_submenu(menu, "Sort By");
    dosgui_ctx_menu_add_item(viewmenu, "Name", ctx_action_sort_by_name);
    dosgui_ctx_menu_add_item(viewmenu, "Size", NULL);
    dosgui_ctx_menu_add_item(viewmenu, "Type", NULL);
    dosgui_ctx_menu_add_item(viewmenu, "Date Modified", NULL);
    
    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "Refresh", ctx_action_refresh);
    dosgui_ctx_menu_add_item(menu, "Properties", ctx_action_properties);
    
    dosgui_ctx_menu_show(menu, mx, my);
}

/* -- Tick ------------------------------------------------------- */

void dosgui_tick(void) {
    g_dwm.ticks++;
}

/* -- Query ------------------------------------------------------- */

int dosgui_wm_screen_w(void) { return g_dwm.screen_w; }
int dosgui_wm_screen_h(void) { return g_dwm.screen_h; }