/*
 * calc.h  --  Calculator App (Standard, Scientific, Programmer, Graphing)
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_CALC_H
#define WUBU_CALC_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declaration for window manager */
typedef struct DosGuiWindow DosGuiWindow;

/* Calculator modes */
typedef enum {
    CALC_STANDARD = 0,
    CALC_SCIENTIFIC,
    CALC_PROGRAMMER,
    CALC_GRAPHING,
    CALC_MODE_COUNT
} CalcMode;

/* Opaque calculator state */
typedef struct CalcState CalcState;

/* Create and destroy calculator */
CalcState* calc_create(void);
void calc_destroy(CalcState *calc);

/* Draw callback for dosgui_wm */
void calc_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, CalcState *calc);

/* Launch calculator window */
DosGuiWindow* calc_launch(void);

/* Operations */
void calc_input_digit(CalcState *calc, int digit);
void calc_input_op(CalcState *calc, int op);
void calc_input_func(CalcState *calc, int func);
void calc_set_mode(CalcState *calc, CalcMode mode);
void calc_set_base(CalcState *calc, int base);

/* Read current display value (for inspection / tests) */
double calc_get_display(const CalcState *calc);
bool   calc_in_error(const CalcState *calc);

/* Operation / function discriminators.
 * Digits are passed as the literal 0..15 (programmer base up to 16).
 * ops/functions select among these: */
typedef enum {
    CALC_OP_NONE = 0,
    CALC_OP_ADD,        /* +   */
    CALC_OP_SUB,        /* -   */
    CALC_OP_MUL,        /* *   */
    CALC_OP_DIV,        /* /   */
    CALC_OP_POW,        /* ^   (scientific) */
    CALC_OP_EQ          /* =   (evaluate pending) */
} CalcOp;

typedef enum {
    CALC_FUNC_NONE = 0,
    CALC_FUNC_SQRT,     /* sqrt */
    CALC_FUNC_NEG,      /* +/- */
    CALC_FUNC_RECIP,    /* 1/x */
    CALC_FUNC_SIN,      /* sin (scientific, radians) */
    CALC_FUNC_COS,      /* cos */
    CALC_FUNC_TAN,      /* tan */
    CALC_FUNC_LN,       /* ln */
    CALC_FUNC_LOG,      /* log10 */
    CALC_FUNC_EXP,      /* e^x */
    CALC_FUNC_CLEAR,    /* C / CE */
    CALC_FUNC_PERCENT   /* % */
} CalcFunc;

#endif