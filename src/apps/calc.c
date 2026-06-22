/*
 * calc.c  --  WuBuOS Calculator (Win98-style)
 *
 * Modes: Standard, Scientific, Programmer
 * Win98 visual: 3D buttons, memory indicators, history tape
 */

#include "../gui/wm.h"
#include "../kernel/vbe.h"
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
#define CALC_MAX_HIST   100

typedef enum {
    CALC_STANDARD = 0,
    CALC_SCIENTIFIC,
    CALC_PROGRAMMER,
    CALC_MODE_COUNT
} CalcMode;

typedef struct {
    double  display_val;
    double  memory;
    double  pending_val;
    int     pending_op;
    bool    new_entry;
    bool    error_state;
    CalcMode mode;
    int     base;  // 10, 16, 8, 2 for programmer
    char    history[CALC_MAX_HIST][64];
    int     hist_count;
    int     hist_scroll;
} CalcState;

static CalcState g_calc = {0};

#define BTN_DIGIT_BASE  100
#define BTN_OP_BASE     200
#define BTN_FUNC_BASE   300
#define BTN_MEM_BASE    400
#define BTN_MODE_BASE   500

static struct {
    int id;
    int x, y;
    int w, h;
    const char *label;
    int type;  // 0=digit, 1=op, 2=func, 3=mem, 4=mode, 5=special
    CalcMode modes;  // bitmask of modes where visible
} g_buttons[] = {
    /* Standard mode - Row 1: MC MR MS M+ M- */
    {BTN_MEM_BASE+0, 0, 0, CALC_BTN_W, CALC_BTN_H, "MC", 3, 1<<CALC_STANDARD},
    {BTN_MEM_BASE+1, 0, 0, CALC_BTN_W, CALC_BTN_H, "MR", 3, 1<<CALC_STANDARD},
    {BTN_MEM_BASE+2, 0, 0, CALC_BTN_W, CALC_BTN_H, "MS", 3, 1<<CALC_STANDARD},
    {BTN_MEM_BASE+3, 0, 0, CALC_BTN_W, CALC_BTN_H, "M+", 3, 1<<CALC_STANDARD},
    {BTN_MEM_BASE+4, 0, 0, CALC_BTN_W, CALC_BTN_H, "M-", 3, 1<<CALC_STANDARD},

    /* Standard - Row 2: Backspace CE C +- sqrt */
    {BTN_FUNC_BASE+0, 0, 0, CALC_BTN_W, CALC_BTN_H, "\x08", 2, 1<<CALC_STANDARD},  // backspace
    {BTN_FUNC_BASE+1, 0, 0, CALC_BTN_W, CALC_BTN_H, "CE", 2, 1<<CALC_STANDARD},
    {BTN_FUNC_BASE+2, 0, 0, CALC_BTN_W, CALC_BTN_H, "C", 2, 1<<CALC_STANDARD},
    {BTN_OP_BASE+0,   0, 0, CALC_BTN_W, CALC_BTN_H, "+/-", 1, 1<<CALC_STANDARD},
    {BTN_FUNC_BASE+3, 0, 0, CALC_BTN_W, CALC_BTN_H, "sqrt", 2, 1<<CALC_STANDARD},

    /* Standard - Row 3: 7 8 9 / % */
    {BTN_DIGIT_BASE+7, 0, 0, CALC_BTN_W, CALC_BTN_H, "7", 0, 1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+8, 0, 0, CALC_BTN_W, CALC_BTN_H, "8", 0, 1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+9, 0, 0, CALC_BTN_W, CALC_BTN_H, "9", 0, 1<<CALC_STANDARD},
    {BTN_OP_BASE+1,    0, 0, CALC_BTN_W, CALC_BTN_H, "/", 1, 1<<CALC_STANDARD},
    {BTN_OP_BASE+2,    0, 0, CALC_BTN_W, CALC_BTN_H, "%", 1, 1<<CALC_STANDARD},

    /* Standard - Row 4: 4 5 6 * 1/x */
    {BTN_DIGIT_BASE+4, 0, 0, CALC_BTN_W, CALC_BTN_H, "4", 0, 1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+5, 0, 0, CALC_BTN_W, CALC_BTN_H, "5", 0, 1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+6, 0, 0, CALC_BTN_W, CALC_BTN_H, "6", 0, 1<<CALC_STANDARD},
    {BTN_OP_BASE+3,    0, 0, CALC_BTN_W, CALC_BTN_H, "*", 1, 1<<CALC_STANDARD},
    {BTN_FUNC_BASE+4,  0, 0, CALC_BTN_W, CALC_BTN_H, "1/x", 2, 1<<CALC_STANDARD},

    /* Standard - Row 5: 1 2 3 - = */
    {BTN_DIGIT_BASE+1, 0, 0, CALC_BTN_W, CALC_BTN_H, "1", 0, 1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+2, 0, 0, CALC_BTN_W, CALC_BTN_H, "2", 0, 1<<CALC_STANDARD},
    {BTN_DIGIT_BASE+3, 0, 0, CALC_BTN_W, CALC_BTN_H, "3", 0, 1<<CALC_STANDARD},
    {BTN_OP_BASE+4,    0, 0, CALC_BTN_W, CALC_BTN_H, "-", 1, 1<<CALC_STANDARD},
    {BTN_OP_BASE+5,    0, 0, CALC_BTN_W*2+CALC_GAP, CALC_BTN_H, "=", 1, 1<<CALC_STANDARD},

    /* Standard - Row 6: 0 . + */
    {BTN_DIGIT_BASE+0, 0, 0, CALC_BTN_W*2+CALC_GAP, CALC_BTN_H, "0", 0, 1<<CALC_STANDARD},
    {BTN_FUNC_BASE+5,  0, 0, CALC_BTN_W, CALC_BTN_H, ".", 0, 1<<CALC_STANDARD},

    /* Scientific extra buttons (compact grid) */
    {BTN_FUNC_BASE+6,  0, 0, CALC_BTN_W, CALC_BTN_H, "sin", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+7,  0, 0, CALC_BTN_W, CALC_BTN_H, "cos", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+8,  0, 0, CALC_BTN_W, CALC_BTN_H, "tan", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+9,  0, 0, CALC_BTN_W, CALC_BTN_H, "asin", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+10, 0, 0, CALC_BTN_W, CALC_BTN_H, "acos", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+11, 0, 0, CALC_BTN_W, CALC_BTN_H, "atan", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+12, 0, 0, CALC_BTN_W, CALC_BTN_H, "log", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+13, 0, 0, CALC_BTN_W, CALC_BTN_H, "ln", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+14, 0, 0, CALC_BTN_W, CALC_BTN_H, "x^y", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+15, 0, 0, CALC_BTN_W, CALC_BTN_H, "x^2", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+16, 0, 0, CALC_BTN_W, CALC_BTN_H, "x^3", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+17, 0, 0, CALC_BTN_W, CALC_BTN_H, "10^x", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+18, 0, 0, CALC_BTN_W, CALC_BTN_H, "e^x", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+19, 0, 0, CALC_BTN_W, CALC_BTN_H, "pi", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+20, 0, 0, CALC_BTN_W, CALC_BTN_H, "deg", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+21, 0, 0, CALC_BTN_W, CALC_BTN_H, "rad", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+22, 0, 0, CALC_BTN_W, CALC_BTN_H, "n!", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+23, 0, 0, CALC_BTN_W, CALC_BTN_H, "(", 2, 1<<CALC_SCIENTIFIC},
    {BTN_FUNC_BASE+24, 0, 0, CALC_BTN_W, CALC_BTN_H, ")", 2, 1<<CALC_SCIENTIFIC},

    /* Programmer extra: Hex Dec Oct Bin, And Or Xor Not, Lsh Rsh */
    {BTN_MODE_BASE+0,  0, 0, CALC_BTN_W, CALC_BTN_H, "Hex", 4, 1<<CALC_PROGRAMMER},
    {BTN_MODE_BASE+1,  0, 0, CALC_BTN_W, CALC_BTN_H, "Dec", 4, 1<<CALC_PROGRAMMER},
    {BTN_MODE_BASE+2,  0, 0, CALC_BTN_W, CALC_BTN_H, "Oct", 4, 1<<CALC_PROGRAMMER},
    {BTN_MODE_BASE+3,  0, 0, CALC_BTN_W, CALC_BTN_H, "Bin", 4, 1<<CALC_PROGRAMMER},
    {BTN_FUNC_BASE+25, 0, 0, CALC_BTN_W, CALC_BTN_H, "And", 2, 1<<CALC_PROGRAMMER},
    {BTN_FUNC_BASE+26, 0, 0, CALC_BTN_W, CALC_BTN_H, "Or", 2, 1<<CALC_PROGRAMMER},
    {BTN_FUNC_BASE+27, 0, 0, CALC_BTN_W, CALC_BTN_H, "Xor", 2, 1<<CALC_PROGRAMMER},
    {BTN_FUNC_BASE+28, 0, 0, CALC_BTN_W, CALC_BTN_H, "Not", 2, 1<<CALC_PROGRAMMER},
    {BTN_FUNC_BASE+29, 0, 0, CALC_BTN_W, CALC_BTN_H, "Lsh", 2, 1<<CALC_PROGRAMMER},
    {BTN_FUNC_BASE+30, 0, 0, CALC_BTN_W, CALC_BTN_H, "Rsh", 2, 1<<CALC_PROGRAMMER},
    {BTN_DIGIT_BASE+10, 0,0, CALC_BTN_W, CALC_BTN_H, "A", 0, 1<<CALC_PROGRAMMER},
    {BTN_DIGIT_BASE+11, 0,0, CALC_BTN_W, CALC_BTN_H, "B", 0, 1<<CALC_PROGRAMMER},
    {BTN_DIGIT_BASE+12, 0,0, CALC_BTN_W, CALC_BTN_H, "C", 0, 1<<CALC_PROGRAMMER},
    {BTN_DIGIT_BASE+13, 0,0, CALC_BTN_W, CALC_BTN_H, "D", 0, 1<<CALC_PROGRAMMER},
    {BTN_DIGIT_BASE+14, 0,0, CALC_BTN_W, CALC_BTN_H, "E", 0, 1<<CALC_PROGRAMMER},
    {BTN_DIGIT_BASE+15, 0,0, CALC_BTN_W, CALC_BTN_H, "F", 0, 1<<CALC_PROGRAMMER},

    /* Mode switchers (always visible) */
    {BTN_MODE_BASE+4,  0, 0, 60, 20, "Standard", 4, (1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)},
    {BTN_MODE_BASE+5,  0, 0, 60, 20, "Scientific", 4, (1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)},
    {BTN_MODE_BASE+6,  0, 0, 60, 20, "Programmer", 4, (1<<CALC_STANDARD)|(1<<CALC_SCIENTIFIC)|(1<<CALC_PROGRAMMER)},
};

#define NUM_BUTTONS (sizeof(g_buttons)/sizeof(g_buttons[0]))

static void calc_layout(void) {
    int start_y = CALC_DISPLAY_H + 8;
    int row_h = CALC_BTN_H + CALC_GAP;
    int col_w = CALC_BTN_W + CALC_GAP;
    int idx = 0;

    if (g_calc.mode == CALC_STANDARD) {
        /* Memory row */
        for (int c = 0; c < 5; c++) {
            g_buttons[idx].x = 4 + c * col_w;
            g_buttons[idx].y = start_y;
            idx++;
        }
        start_y += row_h;

        /* Backspace CE C +- sqrt */
        for (int c = 0; c < 5; c++) {
            g_buttons[idx].x = 4 + c * col_w;
            g_buttons[idx].y = start_y;
            idx++;
        }
        start_y += row_h;

        /* 7 8 9 / % */
        for (int c = 0; c < 5; c++) {
            g_buttons[idx].x = 4 + c * col_w;
            g_buttons[idx].y = start_y;
            idx++;
        }
        start_y += row_h;

        /* 4 5 6 * 1/x */
        for (int c = 0; c < 5; c++) {
            g_buttons[idx].x = 4 + c * col_w;
            g_buttons[idx].y = start_y;
            idx++;
        }
        start_y += row_h;

        /* 1 2 3 - */
        for (int c = 0; c < 4; c++) {
            g_buttons[idx].x = 4 + c * col_w;
            g_buttons[idx].y = start_y;
            idx++;
        }
        /* = spans 2 */
        g_buttons[idx].x = 4 + 4 * col_w;
        g_buttons[idx].y = start_y;
        g_buttons[idx].w = CALC_BTN_W * 2 + CALC_GAP;
        idx++;
        start_y += row_h;

        /* 0 . + */
        g_buttons[idx].x = 4;
        g_buttons[idx].y = start_y;
        g_buttons[idx].w = CALC_BTN_W * 2 + CALC_GAP;
        idx++;
        g_buttons[idx].x = 4 + 3 * col_w;
        g_buttons[idx].y = start_y;
        idx++;

    } else if (g_calc.mode == CALC_SCIENTIFIC) {
        /* Scientific: standard digits on right, functions on left */
        /* Mode tabs at top */
        start_y += 28;

        /* Left: function buttons in 4x6 grid */
        int func_start = 6;  // first scientific func
        for (int r = 0; r < 6; r++) {
            for (int c = 0; c < 4; c++) {
                if (func_start < NUM_BUTTONS && (g_buttons[func_start].modes & (1<<CALC_SCIENTIFIC))) {
                    g_buttons[func_start].x = 4 + c * col_w;
                    g_buttons[func_start].y = start_y + r * row_h;
                    func_start++;
                }
            }
        }

        /* Right: standard digit pad */
        int std_idx = 5;  // after memory row
        int std_rows = 5;
        for (int r = 0; r < std_rows; r++) {
            for (int c = 0; c < 5; c++) {
                if (std_idx < NUM_BUTTONS && (g_buttons[std_idx].modes & (1<<CALC_STANDARD))) {
                    g_buttons[std_idx].x = 4 + 4 * col_w + c * col_w;
                    g_buttons[std_idx].y = start_y + r * row_h;
                    std_idx++;
                }
            }
        }

    } else if (g_calc.mode == CALC_PROGRAMMER) {
        /* Programmer: hex digits, bitwise ops, base switchers */
        start_y += 28;

        /* Base switchers row */
        for (int c = 0; c < 4; c++) {
            g_buttons[BTN_MODE_BASE+c-BTN_MODE_BASE].x = 4 + c * col_w;
            g_buttons[BTN_MODE_BASE+c-BTN_MODE_BASE].y = start_y;
        }
        start_y += row_h;

        /* Hex digits A-F */
        for (int r = 0; r < 2; r++) {
            for (int c = 0; c < 3; c++) {
                int btn_idx = BTN_DIGIT_BASE+10 + r*3 + c;
                if (btn_idx < NUM_BUTTONS) {
                    g_buttons[btn_idx].x = 4 + c * col_w;
                    g_buttons[btn_idx].y = start_y + r * row_h;
                }
            }
        }

        /* Bitwise ops */
        int bit_idx = BTN_FUNC_BASE+25;
        for (int r = 0; r < 2; r++) {
            for (int c = 0; c < 3; c++) {
                if (bit_idx < NUM_BUTTONS) {
                    g_buttons[bit_idx].x = 4 + 4 * col_w + c * col_w;
                    g_buttons[bit_idx].y = start_y + r * row_h;
                    bit_idx++;
                }
            }
        }

        /* Standard digits 0-9 on right */
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 3; c++) {
                int d = (3-r)*3 + c + 1;
                if (d == 10) d = 0;
                int btn_idx = BTN_DIGIT_BASE + d;
                if (btn_idx < NUM_BUTTONS) {
                    g_buttons[btn_idx].x = 4 + 7 * col_w + c * col_w;
                    g_buttons[btn_idx].y = start_y + r * row_h;
                }
            }
        }
        /* 0 at bottom */
        g_buttons[BTN_DIGIT_BASE+0].x = 4 + 7 * col_w;
        g_buttons[BTN_DIGIT_BASE+0].y = start_y + 4 * row_h;
        g_buttons[BTN_DIGIT_BASE+0].w = CALC_BTN_W * 2 + CALC_GAP;
    }

    /* Mode tabs at very top */
    int tab_y = CALC_DISPLAY_H + 4;
    g_buttons[BTN_MODE_BASE+4-BTN_MODE_BASE].x = 4;
    g_buttons[BTN_MODE_BASE+4-BTN_MODE_BASE].y = tab_y;
    g_buttons[BTN_MODE_BASE+5-BTN_MODE_BASE].x = 68;
    g_buttons[BTN_MODE_BASE+5-BTN_MODE_BASE].y = tab_y;
    g_buttons[BTN_MODE_BASE+6-BTN_MODE_BASE].x = 132;
    g_buttons[BTN_MODE_BASE+6-BTN_MODE_BASE].y = tab_y;
}

static void calc_add_history(const char *expr, double result) {
    if (g_calc.hist_count < CALC_MAX_HIST) {
        snprintf(g_calc.history[g_calc.hist_count], sizeof(g_calc.history[0]),
                 "%s = %g", expr, result);
        g_calc.hist_count++;
    } else {
        memmove(g_calc.history, g_calc.history+1, sizeof(g_calc.history)-sizeof(g_calc.history[0]));
        snprintf(g_calc.history[CALC_MAX_HIST-1], sizeof(g_calc.history[0]),
                 "%s = %g", expr, result);
    }
}

static void calc_clear_entry(void) {
    g_calc.display_val = 0;
    g_calc.new_entry = true;
    g_calc.error_state = false;
}

static void calc_clear_all(void) {
    calc_clear_entry();
    g_calc.pending_val = 0;
    g_calc.pending_op = 0;
}

static void calc_do_pending(void) {
    if (!g_calc.pending_op) return;
    double a = g_calc.pending_val;
    double b = g_calc.display_val;
    double r = 0;
    bool err = false;

    switch (g_calc.pending_op) {
        case '+': r = a + b; break;
        case '-': r = a - b; break;
        case '*': r = a * b; break;
        case '/': r = (b != 0) ? a / b : (err = true, 0); break;
        case '%': r = (b != 0) ? fmod(a, b) : (err = true, 0); break;
        case '^': r = pow(a, b); break;
        case '&': r = (long long)a & (long long)b; break;
        case '|': r = (long long)a | (long long)b; break;
        case '~': r = (long long)a ^ (long long)b; break;  // Xor
        case '<': r = (long long)a << (long long)b; break; // Lsh
        case '>': r = (long long)a >> (long long)b; break; // Rsh
    }

    if (err) {
        g_calc.error_state = true;
        g_calc.display_val = 0;
    } else {
        char expr[64];
        snprintf(expr, sizeof(expr), "%g %c %g", a, g_calc.pending_op, b);
        calc_add_history(expr, r);
        g_calc.display_val = r;
    }
    g_calc.pending_op = 0;
    g_calc.new_entry = true;
}

static void calc_button_press(int btn_id) {
    if (g_calc.error_state && btn_id != BTN_FUNC_BASE+1 && btn_id != BTN_FUNC_BASE+2) return;

    if (btn_id >= BTN_DIGIT_BASE && btn_id < BTN_DIGIT_BASE+16) {
        int digit = btn_id - BTN_DIGIT_BASE;
        if (g_calc.new_entry) {
            g_calc.display_val = digit;
            g_calc.new_entry = false;
        } else {
            if (g_calc.mode == CALC_PROGRAMMER && g_calc.base == 16 && digit >= 10) {
                /* Hex digit - handle as string */
            } else {
                g_calc.display_val = g_calc.display_val * g_calc.base + digit;
            }
        }
    } else if (btn_id >= BTN_OP_BASE && btn_id < BTN_OP_BASE+10) {
        int op_idx = btn_id - BTN_OP_BASE;
        char ops[] = "+-*/%^&=|<>";
        char op = ops[op_idx];
        if (op == '=') {
            calc_do_pending();
        } else {
            if (g_calc.pending_op && !g_calc.new_entry) calc_do_pending();
            g_calc.pending_val = g_calc.display_val;
            g_calc.pending_op = op;
            g_calc.new_entry = true;
        }
    } else if (btn_id >= BTN_FUNC_BASE && btn_id < BTN_FUNC_BASE+40) {
        int func = btn_id - BTN_FUNC_BASE;
        double x = g_calc.display_val;
        double r = 0;
        bool err = false;
        char expr[64];

        switch (func) {
            case 1: /* CE */ calc_clear_entry(); break;
            case 2: /* C  */ calc_clear_all(); break;
            case 3: /* +/- */ g_calc.display_val = -g_calc.display_val; break;
            case 4: /* sqrt */ r = (x >= 0) ? sqrt(x) : (err=true,0); break;
            case 5: /* 1/x */ r = (x != 0) ? 1/x : (err=true,0); break;
            case 6: /* sin */ r = sin(x * M_PI/180); break;
            case 7: /* cos */ r = cos(x * M_PI/180); break;
            case 8: /* tan */ r = tan(x * M_PI/180); break;
            case 9: /* asin */ r = (x>=-1&&x<=1) ? asin(x)*180/M_PI : (err=true,0); break;
            case 10: /* acos */ r = (x>=-1&&x<=1) ? acos(x)*180/M_PI : (err=true,0); break;
            case 11: /* atan */ r = atan(x)*180/M_PI; break;
            case 12: /* log */ r = (x > 0) ? log10(x) : (err=true,0); break;
            case 13: /* ln */ r = (x > 0) ? log(x) : (err=true,0); break;
            case 14: /* x^y */ g_calc.pending_val = x; g_calc.pending_op = '^'; g_calc.new_entry = true; return;
            case 15: /* x^2 */ r = x*x; break;
            case 16: /* x^3 */ r = x*x*x; break;
            case 17: /* 10^x */ r = pow(10, x); break;
            case 18: /* e^x */ r = exp(x); break;
            case 19: /* pi */ g_calc.display_val = M_PI; g_calc.new_entry = true; return;
            case 20: /* deg */ break;  // mode toggle
            case 21: /* rad */ break;  // mode toggle
            case 22: /* n! */ r = (x >= 0 && x == (int)x && x <= 170) ? tgamma(x+1) : (err=true,0); break;
            case 23: /* ( */ break;
            case 24: /* ) */ break;
            case 25: /* And */ g_calc.pending_val = x; g_calc.pending_op = '&'; g_calc.new_entry = true; return;
            case 26: /* Or */ g_calc.pending_val = x; g_calc.pending_op = '|'; g_calc.new_entry = true; return;
            case 27: /* Xor */ g_calc.pending_val = x; g_calc.pending_op = '^'; g_calc.new_entry = true; return;
            case 28: /* Not */ r = ~(long long)x; break;
            case 29: /* Lsh */ g_calc.pending_val = x; g_calc.pending_op = '<'; g_calc.new_entry = true; return;
            case 30: /* Rsh */ g_calc.pending_val = x; g_calc.pending_op = '>'; g_calc.new_entry = true; return;
            default: return;
        }
        if (err) {
            g_calc.error_state = true;
            g_calc.display_val = 0;
        } else {
            snprintf(expr, sizeof(expr), "%s(%g)", g_buttons[btn_id].label, x);
            calc_add_history(expr, r);
            g_calc.display_val = r;
            g_calc.new_entry = true;
        }
    } else if (btn_id >= BTN_MEM_BASE && btn_id < BTN_MEM_BASE+5) {
        int mem = btn_id - BTN_MEM_BASE;
        switch (mem) {
            case 0: g_calc.memory = 0; break;  // MC
            case 1: g_calc.display_val = g_calc.memory; g_calc.new_entry = true; break;  // MR
            case 2: g_calc.memory = g_calc.display_val; g_calc.new_entry = true; break;  // MS
            case 3: g_calc.memory += g_calc.display_val; break;  // M+
            case 4: g_calc.memory -= g_calc.display_val; break;  // M-
        }
    } else if (btn_id >= BTN_MODE_BASE+4 && btn_id < BTN_MODE_BASE+7) {
        g_calc.mode = (CalcMode)(btn_id - (BTN_MODE_BASE+4));
        if (g_calc.mode == CALC_PROGRAMMER) g_calc.base = 16;
        else g_calc.base = 10;
        calc_layout();
    }
}

static void calc_draw(WmWindow *win, void *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    uint32_t *pixels = (uint32_t*)fb;
    const WubuThemeColors *tc = wubu_theme_colors();

    /* Background */
    for (int y = 0; y < win->h; y++) {
        for (int x = 0; x < win->w; x++) {
            pixels[y * fb_w + (win->x + x)] = tc->win_face;
        }
    }

    /* Display area */
    int dx = win->x + 4;
    int dy = win->y + WM_TITLE_HEIGHT + 4;
    int dw = win->w - 8;
    int dh = CALC_DISPLAY_H;
    vbe_fill_rect(dx, dy, dw, dh, 0x00FFFFFF);
    vbe_3d_sunken_colors(dx, dy, dw, dh,
                          tc->border_light, tc->border_face,
                          tc->border_dark, tc->border_darkest);

    /* Display text (simple - show value) */
    char buf[64];
    if (g_calc.error_state) {
        snprintf(buf, sizeof(buf), "Error");
    } else if (g_calc.mode == CALC_PROGRAMMER) {
        if (g_calc.base == 16) snprintf(buf, sizeof(buf), "0x%lX", (long long)g_calc.display_val);
        else if (g_calc.base == 8) snprintf(buf, sizeof(buf), "0%llo", (long long)g_calc.display_val);
        else if (g_calc.base == 2) {
            /* Binary display */
            unsigned long long v = (unsigned long long)g_calc.display_val;
            char *p = buf + sizeof(buf) - 1;
            *p = '\0';
            do { *--p = (v & 1) ? '1' : '0'; v >>= 1; } while (v && p > buf);
        } else snprintf(buf, sizeof(buf), "%lld", (long long)g_calc.display_val);
    } else {
        snprintf(buf, sizeof(buf), "%g", g_calc.display_val);
    }

    /* Memory indicator */
    if (g_calc.memory != 0) {
        char mbuf[8];
        snprintf(mbuf, sizeof(mbuf), " M ");
        /* Draw at top-left of display */
    }

    /* Draw buttons */
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (!(g_buttons[i].modes & (1<<g_calc.mode))) continue;
        if (strlen(g_buttons[i].label) == 0) continue;

        int bx = win->x + g_buttons[i].x;
        int by = win->y + WM_TITLE_HEIGHT + g_buttons[i].y;
        int bw = g_buttons[i].w;
        int bh = g_buttons[i].h;

        int btn_id = g_buttons[i].id;

        /* Skip mode tabs for non-current mode */
        if (btn_id >= BTN_MODE_BASE+4 && btn_id < BTN_MODE_BASE+7) {
            int m = btn_id - (BTN_MODE_BASE+4);
            if (m == g_calc.mode) continue;  // don't draw active tab? or draw pressed
        }

        vbe_fill_rect(bx, by, bw, bh, tc->btn_face);
        vbe_3d_raised_colors(bx, by, bw, bh,
                              tc->border_light, tc->border_face,
                              tc->border_dark, tc->border_darkest);

        /* Center text (simplified - just first char) */
    }
}

static void calc_handle_mouse(WmWindow *win, int x, int y, int btn, int kind) {
    if (kind != 1 || btn != 1) return;  // left click down

    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (!(g_buttons[i].modes & (1<<g_calc.mode))) continue;

        int bx = win->x + g_buttons[i].x;
        int by = win->y + WM_TITLE_HEIGHT + g_buttons[i].y;
        int bw = g_buttons[i].w;
        int bh = g_buttons[i].h;

        if (x >= bx && x < bx+bw && y >= by && y < by+bh) {
            calc_button_press(g_buttons[i].id);
            wm_invalidate(win);
            break;
        }
    }
}

static void calc_handle_key(WmWindow *win, uint32_t key, uint32_t mods) {
    (void)win; (void)mods;
    /* NumPad and keyboard support */
    if (key >= '0' && key <= '9') calc_button_press(BTN_DIGIT_BASE + (key - '0'));
    else if (key == '+') calc_button_press(BTN_OP_BASE + 0);
    else if (key == '-') calc_button_press(BTN_OP_BASE + 1);
    else if (key == '*') calc_button_press(BTN_OP_BASE + 3);
    else if (key == '/') calc_button_press(BTN_OP_BASE + 1);
    else if (key == '.') calc_button_press(BTN_FUNC_BASE + 5);
    else if (key == 0x1C) calc_button_press(BTN_OP_BASE + 5);  // Enter = =
    else if (key == 0x0E) calc_button_press(BTN_FUNC_BASE + 0);  // Backspace
    else if (key == 0x01) calc_button_press(BTN_FUNC_BASE + 2);  // Esc = C
}

void calc_open(void) {
    memset(&g_calc, 0, sizeof(g_calc));
    g_calc.mode = CALC_STANDARD;
    g_calc.base = 10;
    calc_layout();

    WmWindow *win = wm_create_window(200, 200, CALC_WIN_W, CALC_WIN_H, "Calculator");
    if (win) {
        win->on_draw = calc_draw;
        win->on_mouse = calc_handle_mouse;
        win->on_key = calc_handle_key;
    }
}

void calc_init(void) { memset(&g_calc, 0, sizeof(g_calc)); }
void calc_shutdown(void) { }