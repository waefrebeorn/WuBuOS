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

#endif