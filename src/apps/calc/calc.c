/*
 * calc.c  --  Calculator App Implementation (minimal stub for linking)
 */

#include "calc.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* Full definition of opaque struct */
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

CalcState* calc_create(void) {
    CalcState *calc = calloc(1, sizeof(CalcState));
    if (!calc) return NULL;
    calc->graph_x_min = -10.0;
    calc->graph_x_max = 10.0;
    calc->graph_y_min = -10.0;
    calc->graph_y_max = 10.0;
    strcpy(calc->graph_expr, "sin(x)");
    calc->graph_point_count = 200;
    calc->base = 10;
    return calc;
}

void calc_destroy(CalcState *calc) {
    free(calc);
}

static void calc_layout(CalcState *calc) {
    (void)calc;
}

static void calc_draw_display(uint32_t *fb, int x, int y, int w, int h, CalcState *calc) {
    (void)fb; (void)x; (void)y; (void)w; (void)h; (void)calc;
}

static void calc_draw_graph(DosGuiWindow *win, int cx, int cy, int cw, int ch, CalcState *calc) {
    (void)win; (void)cx; (void)cy; (void)cw; (void)ch; (void)calc;
}

static void calc_draw_button(DosGuiWindow *win, int btn_idx, CalcState *calc) {
    (void)win; (void)btn_idx; (void)calc;
}

void calc_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, CalcState *calc) {
    (void)fb; (void)fb_w; (void)fb_h;
    (void)win; (void)calc;
}

DosGuiWindow* calc_launch(void) {
    int x = 80 + (rand() % 300);
    int y = 60 + (rand() % 200);
    return dosgui_wm_create(x, y, 280, 380, "Calculator");
}

void calc_input_digit(CalcState *calc, int digit) { (void)calc; (void)digit; }
void calc_input_op(CalcState *calc, int op) { (void)calc; (void)op; }
void calc_input_func(CalcState *calc, int func) { (void)calc; (void)func; }
void calc_set_mode(CalcState *calc, CalcMode mode) { (void)calc; calc->mode = mode; }
void calc_set_base(CalcState *calc, int base) { (void)calc; calc->base = base; }