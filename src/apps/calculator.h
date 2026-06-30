/*
 * calculator.h  --  WuBuOS Calculator (Standard/Scientific/Programmer/Graphing)
 *
 * Opaque struct + C11 only. No god headers.
 */

#ifndef WUBU_CALCULATOR_H
#define WUBU_CALCULATOR_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct DosGuiWindow DosGuiWindow;
typedef struct CalcState CalcState;

/* Calculator modes */
typedef enum {
    CALC_STANDARD = 0,
    CALC_SCIENTIFIC,
    CALC_PROGRAMMER,
    CALC_GRAPHING,
    CALC_MODE_COUNT
} CalcMode;

/* Public API */
CalcState* calc_state_get(void);
DosGuiWindow* dosgui_calc_launch(void);
void dosgui_calc_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);

/* State accessors for testing */
double calc_get_display(CalcState *state);
void calc_set_display(CalcState *state, double val);
double calc_get_memory(CalcState *state);
void calc_set_memory(CalcState *state, double val);
CalcMode calc_get_mode(CalcState *state);
void calc_set_mode(CalcState *state, CalcMode mode);
int calc_get_base(CalcState *state);
void calc_set_base(CalcState *state, int base);

#endif /* WUBU_CALCULATOR_H */