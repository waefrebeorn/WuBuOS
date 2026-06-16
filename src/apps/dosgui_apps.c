/*
 * dosgui_apps.c  --  WuBuOS DosGui App Wrappers
 *
 * Provides dosgui_wm-compatible draw callbacks for in-process apps.
 * Adapts legacy WmWindow apps to DosGuiWindow signature.
 * Cell 401+402: Desktop + StartMenu launch real app content.
 */

#include "dosgui_apps.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_desktop.h"
#include "../kernel/vbe.h"
#include "../gui/wm.h"
#include "../gui/wubu_theme.h"
#include "../runtime/wubu_host_exec.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

/* ================================================================
 * DosGuiWindow to WmWindow adapter (kept for potential future use)
 * ================================================================ */

static void dosgui_to_wm_window(DosGuiWindow *dos_win, WmWindow *wm_win,
                                 uint32_t *fb, int fb_w, int fb_h) {
    wm_win->id = dos_win->id;
    wm_win->flags = WIN_VISIBLE | (dos_win->flags == DOSGUI_WIN_FOCUSED ? WIN_FOCUSED : 0);
    wm_win->x = dos_win->x;
    wm_win->y = dos_win->y;
    wm_win->w = dos_win->w;
    wm_win->h = dos_win->h;
    strncpy(wm_win->title, dos_win->title, sizeof(wm_win->title) - 1);
    wm_win->title_color = (dos_win->flags == DOSGUI_WIN_FOCUSED) ? 0x00000080 : 0x00808080;
    wm_win->on_draw = NULL;
    wm_win->user_data = dos_win->user_data;
}

/* ================================================================
 * Calculator App (Win98-style)
 * ================================================================ */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CALC_WIN_W      280
#define CALC_WIN_H      380
#define CALC_DISPLAY_H  48
#define CALC_BTN_W      56
#define CALC_BTN_H      40
#define CALC_GAP        4

typedef enum { CALC_STANDARD = 0, CALC_SCIENTIFIC, CALC_PROGRAMMER, CALC_MODE_COUNT } CalcMode;

typedef struct {
    double  display_val;
    double  memory;
    double  pending_val;
    int     pending_op;
    bool    new_entry;
    bool    error_state;
    CalcMode mode;
    int     base;
} CalcState;

static CalcState g_calc = {0};

#define BTN_DIGIT_BASE  100
#define BTN_OP_BASE     200
#define BTN_FUNC_BASE   300
#define BTN_MEM_BASE    400
#define BTN_MODE_BASE   500

static struct {
    int id;
    int x, y, w, h;
    const char *label;
    int type;
    int modes;
} g_calc_buttons[] = {
    {BTN_MEM_BASE+0,0,0,CALC_BTN_W,CALC_BTN_H,"MC",3,1<<CALC_STANDARD},
    {BTN_MEM_BASE+1,0,0,CALC_BTN_W,CALC_BTN_H,"MR",3,1<<CALC_STANDARD},
    {BTN_MEM_BASE+2,0,0,CALC_BTN_W,CALC_BTN_H,"MS",3,1<<CALC_STANDARD},
    {BTN_MEM_BASE+3,0,0,CALC_BTN_W,CALC_BTN_H,"M+",3,1<<CALC_STANDARD},
    {BTN_MEM_BASE+4,0,0,CALC_BTN_W,CALC_BTN_H,"M-",3,1<<CALC_STANDARD},
    {BTN_FUNC_BASE+0,0,0,CALC_BTN_W,CALC_BTN_H,"\x08",2,1<<CALC_STANDARD},
    {BTN_FUNC_BASE+1,0,0,CALC_BTN_W,CALC_BTN_H,"CE",2,1<<CALC_STANDARD},
    {BTN_FUNC_BASE+2,0,0,CALC_BTN_W,CALC_BTN_H,"C",2,1<<CALC_STANDARD},
    {BTN_OP_BASE+0,  0,0,CALC_BTN_W,CALC_BTN_H,"+/",1,1<<CALC_STANDARD},
    {BTN_FUNC_BASE+3,0,0,CALC_BTN_W,CALC_BTN_H,"sqrt",2,1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+7,0,0,CALC_BTN_W,CALC_BTN_H,"7",0,1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+8,0,0,CALC_BTN_W,CALC_BTN_H,"8",0,1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+9,0,0,CALC_BTN_W,CALC_BTN_H,"9",0,1<<CALC_STANDARD},
    {BTN_OP_BASE+1,  0,0,CALC_BTN_W,CALC_BTN_H,"/",1,1<<CALC_STANDARD},
    {BTN_OP_BASE+2,  0,0,CALC_BTN_W,CALC_BTN_H,"%",1,1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+4,0,0,CALC_BTN_W,CALC_BTN_H,"4",0,1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+5,0,0,CALC_BTN_W,CALC_BTN_H,"5",0,1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+6,0,0,CALC_BTN_W,CALC_BTN_H,"6",0,1<<CALC_STANDARD},
    {BTN_OP_BASE+3,  0,0,CALC_BTN_W,CALC_BTN_H,"*",1,1<<CALC_STANDARD},
    {BTN_FUNC_BASE+4,0,0,CALC_BTN_W,CALC_BTN_H,"1/x",2,1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+1,0,0,CALC_BTN_W,CALC_BTN_H,"1",0,1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+2,0,0,CALC_BTN_W,CALC_BTN_H,"2",0,1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+3,0,0,CALC_BTN_W,CALC_BTN_H,"3",0,1<<CALC_STANDARD},
    {BTN_OP_BASE+4,   0,0,CALC_BTN_W,CALC_BTN_H,"-",1,1<<CALC_STANDARD},
    {BTN_OP_BASE+5,   0,0,CALC_BTN_W*2+CALC_GAP,CALC_BTN_H,"=",1,1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+0,0,0,CALC_BTN_W*2+CALC_GAP,CALC_BTN_H,"0",0,1<<CALC_STANDARD},
    {BTN_FUNC_BASE+5, 0,0,CALC_BTN_W,CALC_BTN_H,".",0,1<<CALC_STANDARD},
    {BTN_MODE_BASE+4, 0,0,60,20,"Standard",4,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)},
    {BTN_MODE_BASE+5, 0,0,60,20,"Scientific",4,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)},
    {BTN_MODE_BASE+6, 0,0,60,20,"Programmer",4,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)},
};

#define NUM_CALC_BUTTONS (sizeof(g_calc_buttons)/sizeof(g_calc_buttons[0]))

static void calc_layout(void) {
    int start_y = CALC_DISPLAY_H + 8;
    int row_h = CALC_BTN_H + CALC_GAP;
    int col_w = CALC_BTN_W + CALC_GAP;
    int idx = 0;

    if (g_calc.mode == CALC_STANDARD) {
        for (int c = 0; c < 5; c++) { g_calc_buttons[idx].x = 4 + c * col_w; g_calc_buttons[idx].y = start_y; idx++; }
        start_y += row_h;
        for (int c = 0; c < 5; c++) { g_calc_buttons[idx].x = 4 + c * col_w; g_calc_buttons[idx].y = start_y; idx++; }
        start_y += row_h;
        for (int c = 0; c < 5; c++) { g_calc_buttons[idx].x = 4 + c * col_w; g_calc_buttons[idx].y = start_y; idx++; }
        start_y += row_h;
        for (int c = 0; c < 5; c++) { g_calc_buttons[idx].x = 4 + c * col_w; g_calc_buttons[idx].y = start_y; idx++; }
        start_y += row_h;
        for (int c = 0; c < 4; c++) { g_calc_buttons[idx].x = 4 + c * col_w; g_calc_buttons[idx].y = start_y; idx++; }
        g_calc_buttons[idx].x = 4 + 4 * col_w; g_calc_buttons[idx].y = start_y; g_calc_buttons[idx].w = CALC_BTN_W * 2 + CALC_GAP; idx++;
        start_y += row_h;
        g_calc_buttons[idx].x = 4; g_calc_buttons[idx].y = start_y; g_calc_buttons[idx].w = CALC_BTN_W * 2 + CALC_GAP; idx++;
        g_calc_buttons[idx].x = 4 + 3 * col_w; g_calc_buttons[idx].y = start_y; idx++;
    }

    int tab_y = CALC_DISPLAY_H + 4;
    g_calc_buttons[25].x = 4; g_calc_buttons[25].y = tab_y;
    g_calc_buttons[26].x = 68; g_calc_buttons[26].y = tab_y;
    g_calc_buttons[27].x = 132; g_calc_buttons[27].y = tab_y;
}

static void calc_draw_display(uint32_t *fb, int x, int y, int w, int h) {
    const WubuThemeColors *tc = wubu_theme_colors();
    vbe_fill_rect(x, y, w, h, 0x00FFFFFF);
    vbe_rect(x, y, w, h, tc->border_dark);

    char buf[32];
    if (g_calc.error_state) snprintf(buf, sizeof(buf), "Error");
    else if (g_calc.mode == CALC_PROGRAMMER) {
        if (g_calc.base == 16) snprintf(buf, sizeof(buf), "%lX", (long long)g_calc.display_val);
        else if (g_calc.base == 2) {
            long long v = (long long)g_calc.display_val;
            for (int i = 31; i >= 0; i--) buf[31-i] = (v >> i) & 1 ? '1' : '0';
            buf[32] = '\0';
        } else snprintf(buf, sizeof(buf), "%lld", (long long)g_calc.display_val);
    } else snprintf(buf, sizeof(buf), "%g", g_calc.display_val);

    int tw = vbe_text_width(buf, 2);
    vbe_draw_text(x + w - tw - 8, y + 8, buf, 0x00000000, 2);
}

static void calc_draw_button(DosGuiWindow *win, int btn_idx) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int bx = win->x + g_calc_buttons[btn_idx].x;
    int by = win->y + DOSGUI_TITLE_H + g_calc_buttons[btn_idx].y;
    int bw = g_calc_buttons[btn_idx].w;
    int bh = g_calc_buttons[btn_idx].h;

    bool is_mode_btn = (g_calc_buttons[btn_idx].id >= BTN_MODE_BASE && g_calc_buttons[btn_idx].id < BTN_MODE_BASE+3);
    int mode_idx = g_calc_buttons[btn_idx].id - BTN_MODE_BASE;
    bool active = (mode_idx == g_calc.mode);

    uint32_t bg = is_mode_btn ?
        (active ? tc->select_bg : tc->btn_face) :
        tc->btn_face;

    vbe_fill_rect(bx, by, bw, bh, bg);
    if (active) vbe_3d_sunken(bx, by, bw, bh);
    else vbe_3d_raised(bx, by, bw, bh);
    vbe_draw_text(bx + (bw - vbe_text_width(g_calc_buttons[btn_idx].label, 1))/2,
                  by + (bh - 8)/2,
                  g_calc_buttons[btn_idx].label, 0x00000000, 1);
}

void dosgui_calc_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w; (void)fb_h;
    const WubuThemeColors *tc = wubu_theme_colors();

    int cx = win->x + DOSGUI_BORDER;
    int cy = win->y + DOSGUI_TITLE_H;
    int cw = win->w - 2 * DOSGUI_BORDER;
    int ch = win->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    vbe_fill_rect(cx, cy, cw, ch, tc->win_face);
    vbe_3d_sunken(cx - 1, cy - 1, cw + 2, ch + 2);

    calc_layout();
    calc_draw_display(fb, cx + 4, cy + 4, cw - 8, CALC_DISPLAY_H);

    for (int i = 0; i < NUM_CALC_BUTTONS; i++) {
        if ((g_calc_buttons[i].modes & (1 << g_calc.mode)) ||
            (g_calc_buttons[i].id >= BTN_MODE_BASE && g_calc_buttons[i].id < BTN_MODE_BASE+3)) {
            calc_draw_button(win, i);
        }
    }
}

/* ================================================================
 * Notepad Adapter (self-contained)
 * ================================================================ */

static char g_notepad_text[65536] = {0};
static int g_notepad_len = 0;
static int g_notepad_cursor = 0;

void dosgui_notepad_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w; (void)fb_h;
    const WubuThemeColors *tc = wubu_theme_colors();
    
    int cx = win->x + DOSGUI_BORDER;
    int cy = win->y + DOSGUI_TITLE_H;
    int cw = win->w - 2 * DOSGUI_BORDER;
    int ch = win->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    vbe_fill_rect(cx, cy, cw, ch, 0x00FFFFFF);
    vbe_3d_sunken(cx - 1, cy - 1, cw + 2, ch + 2);

    int x = cx + 4;
    int y = cy + 4;
    int line_h = 10;

    for (int i = 0; i < g_notepad_len && y + line_h < cy + ch - 4; i++) {
        if (g_notepad_text[i] == '\n') {
            y += line_h;
            x = cx + 4;
        } else if (g_notepad_text[i] >= 32 && g_notepad_text[i] < 127) {
            char s[2] = { g_notepad_text[i], 0 };
            vbe_draw_text(x, y, s, 0x00000000, 1);
            x += 8;
        }
    }

    /* Draw cursor */
    if ((g_notepad_cursor / 2) % 2 == 0) {
        int cursor_x = cx + 4 + (g_notepad_cursor % 80) * 8;
        int cursor_y = cy + 4 + (g_notepad_cursor / 80) * 10;
        vbe_vline(cursor_x, cursor_y, cursor_y + 8, 0x00000000);
    }
}

/* ================================================================
 * Paint Adapter (self-contained)
 * ================================================================ */

void dosgui_paint_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w; (void)fb_h;
    const WubuThemeColors *tc = wubu_theme_colors();
    int cx = win->x + DOSGUI_BORDER;
    int cy = win->y + DOSGUI_TITLE_H;
    int cw = win->w - 2 * DOSGUI_BORDER;
    int ch = win->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    vbe_fill_rect(cx, cy, cw, ch, tc->win_face);
    vbe_3d_sunken(cx - 1, cy - 1, cw + 2, ch + 2);

    int canvas_w = cw - 160; /* palette on right */
    int canvas_h = ch - 8;
    vbe_fill_rect(cx + 4, cy + 4, canvas_w, canvas_h, 0x00FFFFFF);
    vbe_rect(cx + 4, cy + 4, canvas_w, canvas_h, tc->border_dark);

    /* Simple palette on right */
    static const uint32_t palette[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
    };
    for (int i = 0; i < 16; i++) {
        int px = cx + canvas_w + 12;
        int py = cy + 4 + i * 16;
        vbe_fill_rect(px, py, 12, 12, palette[i]);
        vbe_rect(px, py, 12, 12, 0x00000000);
    }
    vbe_draw_text(cx + canvas_w + 12, cy + 4 + 17*16, "Colors", 0x00000000, 1);
}

/* ================================================================
 * REPL Adapter (self-contained)
 * ================================================================ */

#define REPL_MAX_LINES  500
#define REPL_LINE_LEN   256

static struct {
    char lines[REPL_MAX_LINES][REPL_LINE_LEN];
    int line_count;
    char input[REPL_LINE_LEN];
    int input_pos;
} g_repl_state = {0};

static void repl_add_line(const char *line) {
    if (g_repl_state.line_count < REPL_MAX_LINES) {
        strncpy(g_repl_state.lines[g_repl_state.line_count], line, REPL_LINE_LEN - 1);
        g_repl_state.lines[g_repl_state.line_count][REPL_LINE_LEN - 1] = '\0';
        g_repl_state.line_count++;
    } else {
        for (int i = 1; i < REPL_MAX_LINES; i++) {
            strcpy(g_repl_state.lines[i-1], g_repl_state.lines[i]);
        }
        strncpy(g_repl_state.lines[REPL_MAX_LINES-1], line, REPL_LINE_LEN - 1);
    }
}

void dosgui_repl_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w; (void)fb_h;
    const WubuThemeColors *tc = wubu_theme_colors();

    int cx = win->x + DOSGUI_BORDER;
    int cy = win->y + DOSGUI_TITLE_H;
    int cw = win->w - 2 * DOSGUI_BORDER;
    int ch = win->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    vbe_fill_rect(cx, cy, cw, ch, 0x00000000);
    vbe_3d_sunken(cx - 1, cy - 1, cw + 2, ch + 2);

    int x = cx + 4;
    int y = cy + 4;
    int line_h = 10;
    int max_visible = (ch - 8) / line_h;

    int start = g_repl_state.line_count - max_visible;
    if (start < 0) start = 0;

    for (int i = start; i < g_repl_state.line_count; i++) {
        if (y + line_h > cy + ch - 4) break;
        vbe_draw_text(x, y, g_repl_state.lines[i], 0x00FFFFFF, 1);
        y += line_h;
    }

    /* Input line */
    if (y + line_h <= cy + ch - 4) {
        char prompt_line[REPL_LINE_LEN + 8];
        snprintf(prompt_line, sizeof(prompt_line), "$ %s", g_repl_state.input);
        vbe_draw_text(x, y, prompt_line, 0x00FFFF00, 1);

        /* Cursor */
        int cursor_x = x + (2 + g_repl_state.input_pos) * 8;
        vbe_vline(cursor_x, y, y + 8, 0x00FFFF00);
    }
}

/* ================================================================
 * Control Panel Adapter (self-contained)
 * ================================================================ */

static int g_ctrl_tab = 0;
static const char *ctrl_tabs[] = {
    "Display", "Theme", "Desktop", "Taskbar", "Input", "Startup", "Containers", "Network", "About"
};

void dosgui_control_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w; (void)fb_h;
    const WubuThemeColors *tc = wubu_theme_colors();

    int cx = win->x + DOSGUI_BORDER;
    int cy = win->y + DOSGUI_TITLE_H;
    int cw = win->w - 2 * DOSGUI_BORDER;
    int ch = win->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    vbe_fill_rect(cx, cy, cw, ch, tc->win_face);
    vbe_3d_sunken(cx - 1, cy - 1, cw + 2, ch + 2);

    /* Tab bar */
    int tab_h = 24;
    int tab_w = cw / 9;
    for (int i = 0; i < 9; i++) {
        int tx = cx + i * tab_w;
        uint32_t bg = (i == g_ctrl_tab) ? tc->select_bg : tc->btn_face;
        vbe_fill_rect(tx, cy, tab_w, tab_h, bg);
        if (i == g_ctrl_tab) vbe_3d_sunken(tx, cy, tab_w, tab_h);
        else vbe_3d_raised(tx, cy, tab_w, tab_h);
        vbe_draw_text(tx + (tab_w - vbe_text_width(ctrl_tabs[i], 1))/2,
                      cy + (tab_h - 8)/2, ctrl_tabs[i], 0x00000000, 1);
    }

    /* Panel */
    int px = cx + 4;
    int py = cy + tab_h + 4;
    int pw = cw - 8;
    int ph = ch - tab_h - 8;
    vbe_fill_rect(px, py, pw, ph, tc->win_face);
    vbe_3d_sunken(px, py, pw, ph);

    if (g_ctrl_tab == 1) { /* Theme tab */
        const char *themes[] = {"Win98 Classic", "XP Luna Blue", "XP Media Orange", "WuBu Green"};
        for (int i = 0; i < 4; i++) {
            int iy = py + 8 + i * 32;
            vbe_fill_rect(px + 8, iy, 200, 28, (i == 0) ? tc->select_bg : tc->btn_face);
            vbe_3d_raised(px + 8, iy, 200, 28);
            vbe_fill_rect(px + 12, iy + 4, 20, 20, tc->desktop_bg);
            vbe_rect(px + 12, iy + 4, 20, 20, tc->border_dark);
            vbe_draw_text(px + 40, iy + 10, themes[i], 0x00000000, 1);
        }
    } else if (g_ctrl_tab == 8) { /* About tab */
        vbe_draw_text(px + 8, py + 8, "WuBuOS v0.1.0", 0x00000080, 1);
        vbe_draw_text(px + 8, py + 24, "ZealOS Kernel: wubu-custom", 0x00000080, 1);
        vbe_draw_text(px + 8, py + 40, "GAAD φ = 1.6180339887", 0x00000080, 1);
    } else {
        vbe_draw_text(px + 8, py + 8, ctrl_tabs[g_ctrl_tab], 0x00000080, 1);
        vbe_draw_text(px + 8, py + 24, "[Settings panel - not yet implemented]", 0x00808080, 1);
    }
}

/* ================================================================
 * Editor (simple wrapper)
 * ================================================================ */

void dosgui_editor_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int cx = win->x + DOSGUI_BORDER;
    int cy = win->y + DOSGUI_TITLE_H;
    int cw = win->w - 2 * DOSGUI_BORDER;
    int ch = win->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    vbe_fill_rect(cx, cy, cw, ch, 0x00FFFFFF);
    vbe_3d_sunken(cx - 1, cy - 1, cw + 2, ch + 2);
    vbe_draw_text(cx + 8, cy + 8, "WuBu Editor - Type to edit", 0x00000000, 1);
}

/* ================================================================
 * WuBu Canvas (simple wrapper)
 * ================================================================ */

void dosgui_canvas_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int cx = win->x + DOSGUI_BORDER;
    int cy = win->y + DOSGUI_TITLE_H;
    int cw = win->w - 2 * DOSGUI_BORDER;
    int ch = win->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    vbe_fill_rect(cx, cy, cw, ch, tc->win_face);
    vbe_3d_sunken(cx - 1, cy - 1, cw + 2, ch + 2);

    /* Draw a simple canvas area */
    vbe_fill_rect(cx + 4, cy + 4, cw - 8, ch - 8, 0x00FFFFFF);
    vbe_rect(cx + 4, cy + 4, cw - 8, ch - 8, tc->border_dark);
    vbe_draw_text(cx + 8, cy + 8, "WuBu Canvas - Draw here", 0x00808080, 1);
}

/* ================================================================
 * File Manager (simple wrapper)
 * ================================================================ */

void dosgui_explorer_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int cx = win->x + DOSGUI_BORDER;
    int cy = win->y + DOSGUI_TITLE_H;
    int cw = win->w - 2 * DOSGUI_BORDER;
    int ch = win->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    vbe_fill_rect(cx, cy, cw, ch, tc->win_face);
    vbe_3d_sunken(cx - 1, cy - 1, cw + 2, ch + 2);

    /* Draw simple file list */
    vbe_draw_text(cx + 8, cy + 8, "File Manager", 0x00000000, 1);
    vbe_draw_text(cx + 8, cy + 24, "/home/wubu", 0x00008000, 1);
    vbe_draw_text(cx + 8, cy + 40, "  ..", 0x00000000, 1);
    vbe_draw_text(cx + 8, cy + 56, "  src/", 0x00FFD700, 1);
    vbe_draw_text(cx + 8, cy + 72, "  apps/", 0x00FFD700, 1);
    vbe_draw_text(cx + 8, cy + 88, "  kernel/", 0x00FFD700, 1);
    vbe_draw_text(cx + 8, cy + 104, "  main.c", 0x00000000, 1);
    vbe_draw_text(cx + 8, cy + 120, "  Makefile", 0x00000000, 1);
}

/* ================================================================
 * Terminal (simple wrapper - non-PTY, just display)
 * ================================================================ */

void dosgui_terminal_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int cx = win->x + DOSGUI_BORDER;
    int cy = win->y + DOSGUI_TITLE_H;
    int cw = win->w - 2 * DOSGUI_BORDER;
    int ch = win->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    vbe_fill_rect(cx, cy, cw, ch, 0x00000000);
    vbe_3d_sunken(cx - 1, cy - 1, cw + 2, ch + 2);

    vbe_draw_text(cx + 8, cy + 8, "WuBuOS Terminal", 0x0000FF00, 1);
    vbe_draw_text(cx + 8, cy + 24, "wubu@arch:~$ ", 0x0000FF00, 1);
    vbe_draw_text(cx + 8, cy + 40, "_", 0x0000FF00, 1);
}

/* ================================================================
 * Desktop Icon Click Handlers
 * ================================================================ */

void dosgui_launch_my_computer(void)    { dosgui_app_launch(DESK_ICON_MY_COMPUTER); }
void dosgui_launch_temple_repl(void)    { dosgui_app_launch(DESK_ICON_TEMPLE_REPL); }
void dosgui_launch_notepad(void)        { dosgui_app_launch(DESK_ICON_NOTEPAD); }
void dosgui_launch_paint(void)          { dosgui_app_launch(DESK_ICON_PAINT); }
void dosgui_launch_calculator(void)     { dosgui_app_launch(DESK_ICON_CALCULATOR); }
void dosgui_launch_terminal(void)       { dosgui_app_launch(DESK_ICON_TERMINAL); }
void dosgui_launch_file_manager(void)   { dosgui_app_launch(DESK_ICON_EXPLORER); }
void dosgui_launch_settings(void)       { dosgui_app_launch(DESK_ICON_SETTINGS); }
void dosgui_launch_editor(void)         { dosgui_app_launch(DESK_ICON_COUNT); }
void dosgui_launch_canvas(void)         { dosgui_app_launch(DESK_ICON_COUNT + 1); }

/* ================================================================
 * App Registry for Desktop/StartMenu
 * ================================================================ */

typedef struct {
    int icon_type;
    const char *title;
    void (*draw_fn)(DosGuiWindow *, uint32_t *, int, int);
    int default_w, default_h;
} DosGuiAppEntry;

static DosGuiAppEntry g_dosgui_apps[] = {
    {DESK_ICON_MY_COMPUTER,   "My Computer",   dosgui_explorer_draw,  600, 450},
    {DESK_ICON_TEMPLE_REPL,  "HolyC REPL",    dosgui_repl_draw,      400, 400},
    {DESK_ICON_NOTEPAD,      "Notepad",       dosgui_notepad_draw,   500, 400},
    {DESK_ICON_PAINT,        "Paint",         dosgui_paint_draw,     700, 500},
    {DESK_ICON_CALCULATOR,   "Calculator",    dosgui_calc_draw,      280, 380},
    {DESK_ICON_TERMINAL,     "Terminal",      dosgui_terminal_draw,  700, 500},
    {DESK_ICON_EXPLORER,     "File Manager",  dosgui_explorer_draw,  700, 500},
    {DESK_ICON_SETTINGS,     "Control Panel", dosgui_control_draw,   520, 440},
    {DESK_ICON_COUNT,        "Editor",        dosgui_editor_draw,    600, 500},
    {DESK_ICON_COUNT+1,      "WuBu Canvas",   dosgui_canvas_draw,    700, 500},
};

static int g_app_count = sizeof(g_dosgui_apps) / sizeof(g_dosgui_apps[0]);

/* ================================================================
 * Public API: Launch app by icon type
 * ================================================================ */

DosGuiWindow* dosgui_app_launch(int icon_type) {
    for (int i = 0; i < g_app_count; i++) {
        if (g_dosgui_apps[i].icon_type == icon_type) {
            int x = 80 + (rand() % 300);
            int y = 60 + (rand() % 200);
            DosGuiWindow *win = dosgui_wm_create(x, y,
                g_dosgui_apps[i].default_w,
                g_dosgui_apps[i].default_h,
                g_dosgui_apps[i].title);
            if (win) {
                win->on_draw = g_dosgui_apps[i].draw_fn;
            }
            return win;
        }
    }
    return NULL;
}

DosGuiWindow* dosgui_app_launch_by_name(const char *name) {
    for (int i = 0; i < g_app_count; i++) {
        if (strcmp(g_dosgui_apps[i].title, name) == 0) {
            int x = 80 + (rand() % 300);
            int y = 60 + (rand() % 200);
            DosGuiWindow *win = dosgui_wm_create(x, y,
                g_dosgui_apps[i].default_w,
                g_dosgui_apps[i].default_h,
                g_dosgui_apps[i].title);
            if (win) {
                win->on_draw = g_dosgui_apps[i].draw_fn;
            }
            return win;
        }
    }
    return NULL;
}

/* ================================================================
 * External Container Apps (bubblewrap)
 * ================================================================ */

void dosgui_launch_freedoom(void) {
    /* Create FreeDoom container via bubblewrap */
    WubuCt *ct = wubu_ct_bwrap_freedoom("freedoom");
    if (!ct) {
        fprintf(stderr, "Failed to create FreeDoom container\n");
        return;
    }
    
    /* Start the container - dsda-doom will create its own Wayland toplevel */
    if (wubu_ct_start_bwrap(ct) != 0) {
        fprintf(stderr, "Failed to start FreeDoom container\n");
        wubu_ct_destroy(ct);
        return;
    }
    
    /* Container is running. 
     * Note: The dsda-doom process creates its own Wayland window.
     * We don't track it as a DosGuiWindow. The container PID is in ct->pid.
     * For taskbar integration, we'd need WM support for external toplevels.
     * For now, just log and let it run. */
    fprintf(stderr, "FreeDoom launched via bubblewrap (PID %d)\n", ct->pid);
    
    /* Don't wait - let it run independently. 
     * Container struct is leaked intentionally - process continues.
     * In production, we'd track it in a global list for cleanup. */
}