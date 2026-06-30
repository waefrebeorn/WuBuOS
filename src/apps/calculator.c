/*
 * calculator.c  --  WuBuOS Calculator Implementation
 *
 * Standard -> Scientific -> Programmer -> Graphing (BearRL matplotlib)
 * Opaque struct, minimal includes, C11 only.
 */

#include "calculator.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CALC_WIN_W      280
#define CALC_WIN_H      380
#define CALC_DISPLAY_H  48
#define CALC_BTN_W      56
#define CALC_BTN_H      40
#define CALC_GAP        4

#define BTN_DIGIT_BASE  100
#define BTN_OP_BASE     200
#define BTN_FUNC_BASE   300
#define BTN_MEM_BASE    400
#define BTN_MODE_BASE   500
#define BTN_SCI_BASE    600
#define BTN_GRAPH_BASE  700

/* ================================================================
 * Opaque State Definition
 * ================================================================ */

struct CalcState {
    double  display_val;
    double  memory;
    double  pending_val;
    int     pending_op;
    bool    new_entry;
    bool    error_state;
    CalcMode mode;
    int     base;
    /* Graphing state */
    double  graph_x_min, graph_x_max;
    double  graph_y_min, graph_y_max;
    char    graph_expr[256];
    int     graph_point_count;
};

/* Global instance (single calculator) */
static CalcState g_calc = {
    .graph_x_min = -10.0,
    .graph_x_max = 10.0,
    .graph_y_min = -10.0,
    .graph_y_max = 10.0,
    .graph_expr = "sin(x)",
    .graph_point_count = 200,
    .base = 10
};

static struct {
    int id;
    int x, y, w, h;
    const char *label;
    int type;
    int modes;
} g_calc_buttons[] = {
    {BTN_MEM_BASE+0,0,0,CALC_BTN_W,CALC_BTN_H,"MC",3,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)|(1<<CALC_GRAPHING)},
    {BTN_MEM_BASE+1,0,0,CALC_BTN_W,CALC_BTN_H,"MR",3,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)|(1<<CALC_GRAPHING)},
    {BTN_MEM_BASE+2,0,0,CALC_BTN_W,CALC_BTN_H,"MS",3,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)|(1<<CALC_GRAPHING)},
    {BTN_MEM_BASE+3,0,0,CALC_BTN_W,CALC_BTN_H,"M+",3,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)|(1<<CALC_GRAPHING)},
    {BTN_MEM_BASE+4,0,0,CALC_BTN_W,CALC_BTN_H,"M-",3,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)|(1<<CALC_GRAPHING)},
    {BTN_FUNC_BASE+0,0,0,CALC_BTN_W,CALC_BTN_H,"\x08",2,1<<CALC_STANDARD},
    {BTN_FUNC_BASE+1,0,0,CALC_BTN_W,CALC_BTN_H,"CE",2,1<<CALC_STANDARD},
    {BTN_FUNC_BASE+2,0,0,CALC_BTN_W,CALC_BTN_H,"C",2,1<<CALC_STANDARD},
    {BTN_OP_BASE+0,  0,0,CALC_BTN_W,CALC_BTN_H,"+/-",1,1<<CALC_STANDARD},
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
    {BTN_FUNC_BASE+5, 0,0,CALC_BTN_W,CALC_BTN_H,".",0,1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+0,0,0,CALC_BTN_W*2+CALC_GAP,CALC_BTN_H,"0",0,1<<CALC_STANDARD},
    {BTN_OP_BASE+5,   0,0,CALC_BTN_W,CALC_BTN_H,"+",1,1<<CALC_STANDARD},
    {BTN_OP_BASE+6,   0,0,CALC_BTN_W*2+CALC_GAP,CALC_BTN_H,"=",1,1<<CALC_STANDARD},
    {BTN_SCI_BASE+0,  0,0,CALC_BTN_W,CALC_BTN_H,"sin",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+1,  0,0,CALC_BTN_W,CALC_BTN_H,"cos",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+2,  0,0,CALC_BTN_W,CALC_BTN_H,"tan",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+3,  0,0,CALC_BTN_W,CALC_BTN_H,"asin",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+4,  0,0,CALC_BTN_W,CALC_BTN_H,"acos",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+5,  0,0,CALC_BTN_W,CALC_BTN_H,"atan",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+6,  0,0,CALC_BTN_W,CALC_BTN_H,"log",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+7,  0,0,CALC_BTN_W,CALC_BTN_H,"ln",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+8,  0,0,CALC_BTN_W,CALC_BTN_H,"x^2",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+9,  0,0,CALC_BTN_W,CALC_BTN_H,"x^y",1,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+10, 0,0,CALC_BTN_W,CALC_BTN_H,"e^x",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+11, 0,0,CALC_BTN_W,CALC_BTN_H,"10^x",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+12, 0,0,CALC_BTN_W,CALC_BTN_H,"pi",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+13, 0,0,CALC_BTN_W,CALC_BTN_H,"e",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+14, 0,0,CALC_BTN_W,CALC_BTN_H,"n!",2,1<<CALC_SCIENTIFIC},
    {BTN_SCI_BASE+15, 0,0,CALC_BTN_W,CALC_BTN_H,"Deg/Rad",3,1<<CALC_SCIENTIFIC},
    {BTN_GRAPH_BASE+0, 0,0,CALC_BTN_W,CALC_BTN_H,"Y=",3,1<<CALC_GRAPHING},
    {BTN_GRAPH_BASE+1, 0,0,CALC_BTN_W,CALC_BTN_H,"Zoom",3,1<<CALC_GRAPHING},
    {BTN_GRAPH_BASE+2, 0,0,CALC_BTN_W,CALC_BTN_H,"Trace",3,1<<CALC_GRAPHING},
    {BTN_GRAPH_BASE+3, 0,0,CALC_BTN_W,CALC_BTN_H,"Table",3,1<<CALC_GRAPHING},
    {BTN_GRAPH_BASE+4, 0,0,CALC_BTN_W,CALC_BTN_H,"Grid",3,1<<CALC_GRAPHING},
    {BTN_MODE_BASE+4, 0,0,55,20,"Standard",4,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)|(1<<CALC_GRAPHING)},
    {BTN_MODE_BASE+5, 0,0,55,20,"Scientific",4,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)|(1<<CALC_GRAPHING)},
    {BTN_MODE_BASE+6, 0,0,55,20,"Programmer",4,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)|(1<<CALC_GRAPHING)},
    {BTN_MODE_BASE+7, 0,0,55,20,"Graphing",4,(1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)|(1<<CALC_GRAPHING)},
};

#define NUM_CALC_BUTTONS (sizeof(g_calc_buttons)/sizeof(g_calc_buttons[0]))

/* ================================================================
 * Internal Helpers
 * ================================================================ */

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
    } else if (g_calc.mode == CALC_SCIENTIFIC) {
        int sci_start = 28;
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 5; c++) {
                if (sci_start < NUM_CALC_BUTTONS && (g_calc_buttons[sci_start].modes & (1<<CALC_SCIENTIFIC))) {
                    g_calc_buttons[sci_start].x = 4 + c * col_w;
                    g_calc_buttons[sci_start].y = start_y;
                    sci_start++;
                }
            }
            start_y += row_h;
        }
    } else if (g_calc.mode == CALC_GRAPHING) {
        int graph_start = 44;
        for (int c = 0; c < 5; c++) {
            if (graph_start < NUM_CALC_BUTTONS && (g_calc_buttons[graph_start].modes & (1<<CALC_GRAPHING))) {
                g_calc_buttons[graph_start].x = 4 + c * col_w;
                g_calc_buttons[graph_start].y = start_y;
                graph_start++;
            }
        }
    }

    int tab_y = CALC_DISPLAY_H + 4;
    g_calc_buttons[59].x = 4; g_calc_buttons[59].y = tab_y;
    g_calc_buttons[60].x = 62; g_calc_buttons[60].y = tab_y;
    g_calc_buttons[61].x = 120; g_calc_buttons[61].y = tab_y;
    g_calc_buttons[62].x = 178; g_calc_buttons[62].y = tab_y;
}

static void calc_draw_display(uint32_t *fb, int x, int y, int w, int h) {
    (void)fb;
    const WubuThemeColors *tc = wubu_theme_colors();
    vbe_fill_rect(x, y, w, h, 0x00FFFFFF);
    vbe_rect(x, y, w, h, tc->border_dark);

    char buf[32];
    if (g_calc.error_state) snprintf(buf, sizeof(buf), "Error");
    else if (g_calc.mode == CALC_PROGRAMMER) {
        if (g_calc.base == 16) snprintf(buf, sizeof(buf), "%llX", (unsigned long long)g_calc.display_val);
        else if (g_calc.base == 2) {
            long long v = (long long)g_calc.display_val;
            for (int i = 31; i >= 0; i--) buf[31-i] = (v >> i) & 1 ? '1' : '0';
            buf[32] = '\0';
        } else snprintf(buf, sizeof(buf), "%lld", (long long)g_calc.display_val);
    } else snprintf(buf, sizeof(buf), "%g", g_calc.display_val);

    int tw = vbe_text_width(buf, 2);
    vbe_draw_text(x + w - tw - 8, y + 8, buf, 0x00000000, 2);
}

static void calc_draw_graph(DosGuiWindow *win, int cx, int cy, int cw, int ch) {
    (void)win;
    const WubuThemeColors *tc = wubu_theme_colors();
    int gx = cx + 4;
    int gy = cy + CALC_DISPLAY_H + 4;
    int gw = cw - 8;
    int gh = ch - CALC_DISPLAY_H - 8;
    
    vbe_fill_rect(gx, gy, gw, gh, 0x00FFFFFF);
    vbe_rect(gx, gy, gw, gh, tc->border_dark);
    
    int zero_x = gx + (int)((0.0 - g_calc.graph_x_min) / (g_calc.graph_x_max - g_calc.graph_x_min) * gw);
    int zero_y = gy + gh - (int)((0.0 - g_calc.graph_y_min) / (g_calc.graph_y_max - g_calc.graph_y_min) * gh);
    
    if (zero_x >= gx && zero_x <= gx + gw) {
        vbe_vline(zero_x, gy, gy + gh, 0x00808080);
    }
    if (zero_y >= gy && zero_y <= gy + gh) {
        vbe_hline(zero_y, gx, gx + gw, 0x00808080);
    }
    
    for (int i = 1; i < 10; i++) {
        int gx_pos = gx + (i * gw) / 10;
        vbe_vline(gx_pos, gy, gy + gh, 0x00E0E0E0);
        int gy_pos = gy + (i * gh) / 10;
        vbe_hline(gy_pos, gx, gx + gw, 0x00E0E0E0);
    }
    
    double prev_x = 0, prev_y = 0;
    bool first = true;
    for (int i = 0; i <= g_calc.graph_point_count; i++) {
        double x = g_calc.graph_x_min + (g_calc.graph_x_max - g_calc.graph_x_min) * i / g_calc.graph_point_count;
        double y = sin(x);
        
        int px = gx + (int)((x - g_calc.graph_x_min) / (g_calc.graph_x_max - g_calc.graph_x_min) * gw);
        int py = gy + gh - (int)((y - g_calc.graph_y_min) / (g_calc.graph_y_max - g_calc.graph_y_min) * gh);
        
        if (px >= gx && px <= gx + gw && py >= gy && py <= gy + gh) {
            if (!first) {
                vbe_line(prev_x, prev_y, px, py, 0x000000FF);
            }
            prev_x = px;
            prev_y = py;
            first = false;
        }
    }
    
    vbe_draw_text(gx + 4, gy + 4, "y = sin(x)", 0x00000080, 1);
    vbe_draw_text(gx + 4, gy + 16, "BearRL: matplotlib window for plots", 0x00808080, 1);
}

static void calc_draw_button(DosGuiWindow *win, int btn_idx) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int bx = win->x + g_calc_buttons[btn_idx].x;
    int by = win->y + DOSGUI_TITLE_H + g_calc_buttons[btn_idx].y;
    int bw = g_calc_buttons[btn_idx].w;
    int bh = g_calc_buttons[btn_idx].h;

    bool is_mode_btn = (g_calc_buttons[btn_idx].id >= BTN_MODE_BASE && g_calc_buttons[btn_idx].id < BTN_MODE_BASE+4);
    int mode_idx = g_calc_buttons[btn_idx].id - BTN_MODE_BASE;
    bool active = (mode_idx == g_calc.mode);

    uint32_t bg = is_mode_btn ?
        (active ? tc->select_bg : tc->btn_face) :
        tc->btn_face;

    vbe_fill_rect(bx, by, bw, bh, bg);
    if (active) vbe_3d_sunken_colors(bx, by, bw, bh,
                                      tc->border_light, tc->border_face,
                                      tc->border_dark, tc->border_darkest);
    else vbe_3d_raised_colors(bx, by, bw, bh,
                               tc->border_light, tc->border_face,
                               tc->border_dark, tc->border_darkest);
    vbe_draw_text(bx + (bw - vbe_text_width(g_calc_buttons[btn_idx].label, 1))/2,
                  by + (bh - 8)/2,
                  g_calc_buttons[btn_idx].label, 0x00000000, 1);
}

/* ================================================================
 * Public API
 * ================================================================ */

CalcState* calc_state_get(void) {
    return &g_calc;
}

DosGuiWindow* dosgui_calc_launch(void) {
    int x = 80 + (rand() % 300);
    int y = 60 + (rand() % 200);
    DosGuiWindow *win = dosgui_wm_create(x, y, CALC_WIN_W, CALC_WIN_H, "Calculator");
    if (win) {
        win->on_draw = dosgui_calc_draw;
    }
    return win;
}

void dosgui_calc_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    const WubuThemeColors *tc = wubu_theme_colors();

    int cx = win->x + DOSGUI_BORDER;
    int cy = win->y + DOSGUI_TITLE_H;
    int cw = win->w - 2 * DOSGUI_BORDER;
    int ch = win->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    vbe_fill_rect(cx, cy, cw, ch, tc->win_face);
    vbe_3d_sunken_colors(cx - 1, cy - 1, cw + 2, ch + 2,
                          tc->border_light, tc->border_face,
                          tc->border_dark, tc->border_darkest);

    calc_layout();
    calc_draw_display(fb, cx + 4, cy + 4, cw - 8, CALC_DISPLAY_H);

    if (g_calc.mode == CALC_GRAPHING) {
        calc_draw_graph(win, cx, cy, cw, ch);
    } else {
        for (int i = 0; i < NUM_CALC_BUTTONS; i++) {
            if ((g_calc_buttons[i].modes & (1 << g_calc.mode)) ||
                (g_calc_buttons[i].id >= BTN_MODE_BASE && g_calc_buttons[i].id < BTN_MODE_BASE+4)) {
                calc_draw_button(win, i);
            }
        }
    }
}

/* ================================================================
 * State Accessors (for testing)
 * ================================================================ */

double calc_get_display(CalcState *state) { return state->display_val; }
void calc_set_display(CalcState *state, double val) { state->display_val = val; }
double calc_get_memory(CalcState *state) { return state->memory; }
void calc_set_memory(CalcState *state, double val) { state->memory = val; }
CalcMode calc_get_mode(CalcState *state) { return state->mode; }
void calc_set_mode(CalcState *state, CalcMode mode) { state->mode = mode; }
int calc_get_base(CalcState *state) { return state->base; }
void calc_set_base(CalcState *state, int base) { state->base = base; }