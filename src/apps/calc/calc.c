/*
 * calc.c  --  Calculator App Implementation (minimal stub for linking)
 */

#include "calc.h"
#include "calc_internal.h"
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

/* ====================================================================
 * CALCULATION ENGINE  (real implementation)
 *
 * State machine: digits accumulate into display_val while a new entry
 * is being typed; an operator moves display_val into pending_val and
 * records pending_op; the next digit sequence starts a fresh entry;
 * '=' (or another operator) applies pending_op(pending_val, display_val).
 * Unary functions apply immediately to display_val.
 * ================================================================== */



void calc_input_digit(CalcState *calc, int digit) {
    if (!calc || calc->error_state) return;
    if (digit < 0 || digit > 15) return;          /* base up to 16 */
    if (calc->base >= 2 && digit >= calc->base) return;  /* out of range for base */

    if (calc->new_entry) {
        calc->display_val = 0.0;
        calc->new_entry = false;
    }
    /* Build the mantissa; accumulate as decimal regardless of display base. */
    calc->display_val = calc->display_val * 10.0 + (double)digit;
}

void calc_input_op(CalcState *calc, int op) {
    if (!calc || calc->error_state) return;

    if (op == CALC_FUNC_CLEAR || op == CALC_OP_NONE) {
        calc->display_val = 0.0;
        calc->pending_val = 0.0;
        calc->pending_op = CALC_OP_NONE;
        calc->new_entry = true;
        return;
    }

    /* Evaluate any pending operation first (chaining: 2 + 3 + =). */
    if (calc->pending_op != CALC_OP_NONE && !calc->new_entry) {
        bool err = false;
        double r = calc_apply_op(calc->pending_op, calc->pending_val, calc->display_val, &err);
        if (err) { calc->error_state = true; return; }
        calc->display_val = r;
    }

    if (op == CALC_OP_EQ) {
        calc->pending_op = CALC_OP_NONE;
        calc->new_entry = true;
        return;
    }

    /* Stash the current display and arm the next operator. */
    calc->pending_val = calc->display_val;
    calc->pending_op = op;
    calc->new_entry = true;
}

void calc_input_func(CalcState *calc, int func) {
    if (!calc || calc->error_state) return;
    bool err = false;
    double r = calc_apply_func(func, calc->display_val, &err);
    if (err) { calc->error_state = true; return; }
    calc->display_val = r;
    calc->new_entry = true;
}

void calc_set_mode(CalcState *calc, CalcMode mode) {
    if (!calc) return;
    calc->mode = mode;
}

void calc_set_base(CalcState *calc, int base) {
    if (!calc) return;
    if (base >= 2 && base <= 16) calc->base = base;
}

double calc_get_display(const CalcState *calc) {
    return calc ? calc->display_val : 0.0;
}

bool calc_in_error(const CalcState *calc) {
    return calc ? calc->error_state : false;
}
