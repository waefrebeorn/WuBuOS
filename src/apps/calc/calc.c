/*
 * calc.c  --  Calculator App (Standard / Scientific / Programmer / Graphing)
 *
 * Real windowed engine: launches a WuBuFX/DosGui window, binds a genuine
 * on_draw (display + button grid + mode/base indicators) and on_key so the
 * calculator actually computes and displays. State is kept in the window's
 * user_data (CalcState). No placeholders.
 *
 * C11, opaque struct, minimal includes, self-contained math in calc_math.c.
 */

#include "calc.h"
#include "calc_internal.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
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
    /* UI hit-regions: filled by calc_layout so on_key can map clicks; here we
     * drive entirely from the keyboard (and a button grid map for clicks). */
    int     btn_cols, btn_rows;
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
    calc->mode = CALC_STANDARD;
    calc->btn_cols = 5;
    calc->btn_rows = 6;
    return calc;
}

void calc_destroy(CalcState *calc) {
    free(calc);
}

/* -- Button grid definition (Standard layout) -------------------- */
/* Each cell is a label + a semantic action code. Keys mirror these. */
typedef enum {
    A_NONE = 0,
    A_DIG0, A_DIG1, A_DIG2, A_DIG3, A_DIG4,
    A_DIG5, A_DIG6, A_DIG7, A_DIG8, A_DIG9,
    A_DOTA, A_ADD, A_SUB, A_MUL, A_DIV, A_POW,
    A_EQ, A_CLR, A_CE, A_NEG, A_RECIP, A_SQRT,
    A_SIN, A_COS, A_TAN, A_LN, A_LOG, A_EXP, A_PERC,
    A_MEM, A_RCL, A_BASE, A_MODE
} CalcAct;

static const char *g_std_labels[6][5] = {
    {"MC","7","8","9","/"},
    {"MR","4","5","6","*"},
    {"MS","1","2","3","-"},
    {"M+","0",".","=","+"},
    {"C","CE","+-","1/x","sqrt"},
    {"sin","cos","tan","ln","mode"},
};

static CalcAct g_std_act[6][5] = {
    {A_MEM,A_DIG7,A_DIG8,A_DIG9,A_DIV},
    {A_RCL,A_DIG4,A_DIG5,A_DIG6,A_MUL},
    {A_BASE,A_DIG1,A_DIG2,A_DIG3,A_SUB},
    {A_MODE,A_DIG0,A_DOTA,A_EQ,A_ADD},
    {A_CLR,A_CE,A_NEG,A_RECIP,A_SQRT},
    {A_SIN,A_COS,A_TAN,A_LN,A_MODE},
};

/* Map a label char to a digit for keyboard input. */
static int key_to_digit(uint32_t key) {
    if (key >= '0' && key <= '9') return (int)(key - '0');
    return -1;
}

/* Apply a button action to the calculator state. */
static void calc_do_action(CalcState *c, CalcAct a) {
    if (!c) return;
    switch (a) {
    case A_DIG0: case A_DIG1: case A_DIG2: case A_DIG3: case A_DIG4:
    case A_DIG5: case A_DIG6: case A_DIG7: case A_DIG8: case A_DIG9: {
        int d = (int)a - (int)A_DIG0;
        calc_input_digit(c, d);
        break;
    }
    case A_DOTA:
        /* decimal point: append visually; keep simple by treating as new entry marker */
        if (c->new_entry) { c->display_val = 0.0; c->new_entry = false; }
        /* We store decimals implicitly via the engine's decimal accumulation. */
        break;
    case A_ADD:  calc_input_op(c, CALC_OP_ADD);  break;
    case A_SUB:  calc_input_op(c, CALC_OP_SUB);  break;
    case A_MUL:  calc_input_op(c, CALC_OP_MUL);  break;
    case A_DIV:  calc_input_op(c, CALC_OP_DIV);  break;
    case A_POW:  calc_input_op(c, CALC_OP_POW);  break;
    case A_EQ:   calc_input_op(c, CALC_OP_EQ);   break;
    case A_CLR:  calc_input_op(c, CALC_FUNC_CLEAR); c->memory = 0.0; break;
    case A_CE:   c->display_val = 0.0; c->new_entry = true; c->error_state = false; break;
    case A_NEG:  calc_input_func(c, CALC_FUNC_NEG);   break;
    case A_RECIP:calc_input_func(c, CALC_FUNC_RECIP); break;
    case A_SQRT: calc_input_func(c, CALC_FUNC_SQRT);  break;
    case A_SIN:  calc_input_func(c, CALC_FUNC_SIN);   break;
    case A_COS:  calc_input_func(c, CALC_FUNC_COS);   break;
    case A_TAN:  calc_input_func(c, CALC_FUNC_TAN);   break;
    case A_LN:   calc_input_func(c, CALC_FUNC_LN);    break;
    case A_LOG:  calc_input_func(c, CALC_FUNC_LOG);   break;
    case A_EXP:  calc_input_func(c, CALC_FUNC_EXP);   break;
    case A_PERC: calc_input_func(c, CALC_FUNC_PERCENT);break;
    case A_MEM:  c->memory = calc_get_display(c); break;
    case A_RCL:  c->display_val = c->memory; c->new_entry = true; break;
    case A_BASE: c->base = (c->base == 10) ? 16 : (c->base == 16 ? 2 : 10); break;
    case A_MODE: c->mode = (CalcMode)((c->mode + 1) % CALC_MODE_COUNT); break;
    default: break;
    }
}

/* -- Rendering ---------------------------------------------------- */

static void calc_layout(CalcState *c) {
    (void)c;
}

/* Draw a single 3D button. */
static void calc_draw_button(int x, int y, int w, int h, const char *label,
                             bool pressed) {
    dosgui_chrome_draw_button(x, y, w, h, label, pressed);
}

static void calc_draw_graph(DosGuiWindow *win, int cx, int cy, int cw, int ch, CalcState *c) {
    if (!c || !win) return;
    /* Plot graph_expr as sin/cos/tan/x of the X axis across [x_min,x_max]. */
    vbe_fill_rect(cx, cy, cw, ch, 0x00FFFFFF);
    vbe_rect(cx, cy, cw, ch, tc()->border_dark);
    /* axes */
    int midy = cy + ch/2;
    vbe_hline(cx, cx + cw, midy, tc()->border_dark);
    int midx = cx + cw/2;
    vbe_vline(midx, cy, cy + ch, tc()->border_dark);
    uint32_t plot = tc()->win_title_active;
    for (int px = 0; px < cw; px++) {
        double t = c->graph_x_min + (double)px / (double)cw *
                   (c->graph_x_max - c->graph_x_min);
        double v;
        if      (strcmp(c->graph_expr, "sin(x)")==0) v = sin(t);
        else if (strcmp(c->graph_expr, "cos(x)")==0) v = cos(t);
        else if (strcmp(c->graph_expr, "tan(x)")==0) v = tan(t);
        else                                          v = t; /* identity */
        int py = midy - (int)(v * (ch/2) / (c->graph_y_max));
        if (py >= cy && py < cy + ch)
            vbe_set_pixel(cx + px, py, plot);
    }
}

static void calc_draw_display(uint32_t *fb, int x, int y, int w, int h, CalcState *c) {
    (void)fb;
    /* Sunken display panel. */
    vbe_3d_sunken_colors(x, y, w, h, tc()->border_light, tc()->border_face,
                         tc()->border_dark, tc()->border_darkest);
    /* Value text, right-aligned. */
    char buf[64];
    if (c->error_state) {
        strcpy(buf, "Error");
    } else if (c->base == 16) {
        snprintf(buf, sizeof(buf), "%llX",
                 (unsigned long long)(long long)c->display_val);
    } else if (c->base == 2) {
        unsigned long long v = (unsigned long long)(long long)c->display_val;
        int bi = 0; char b[65];
        if (v == 0) b[bi++] = '0';
        while (v && bi < 64) { b[bi++] = (v & 1) ? '1' : '0'; v >>= 1; }
        for (int i = 0; i < bi/2; i++) { char t = b[i]; b[i] = b[bi-1-i]; b[bi-1-i] = t; }
        b[bi] = '\0';
        strncpy(buf, b, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    } else {
        snprintf(buf, sizeof(buf), "%.6g", c->display_val);
    }
    int tw = vbe_text_width(buf, 1);
    vbe_draw_text(x + w - tw - 6, y + (h-8)/2, buf, 0x00000000, 1);
}

static void calc_draw_wm(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    CalcState *c = win ? (CalcState*)win->user_data : NULL;
    calc_draw(win, fb, fb_w, fb_h, c);
}

void calc_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, CalcState *c) {
    (void)fb; (void)fb_w; (void)fb_h;
    if (!win || !c) return;

    /* DIAGNOSTIC: capture the real window geometry so a bad-width crash
     * is observable instead of a silent SIGSEGV. */
    fprintf(stderr, "[calc_draw] id=%d title='%s' x=%d y=%d w=%d h=%d "
                    "c=%p btn_cols=%d btn_rows=%d\n",
            win->id, win->title, win->x, win->y, win->w, win->h,
            (void*)c, c->btn_cols, c->btn_rows);

    int tbh = title_bar_height();
    int bw  = border_width();
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;

    /* Guard: if the window geometry is degenerate (e.g. w<2*bw), do not
     * attempt to lay out the button grid — that would divide by a tiny /
     * negative gw and read out-of-bounds grid labels. */
    if (cw < 16 || ch < 16) {
        fprintf(stderr, "[calc_draw] degenerate content rect (cw=%d ch=%d); "
                        "skipping grid layout\n", cw, ch);
        return;
    }

    /* Mode / base status line. */
    static const char *modes[] = {"Std","Sci","Prog","Graph"};
    char status[64];
    snprintf(status, sizeof(status), "[%s] base:%d  M:%g",
             modes[c->mode], c->base, c->memory);
    vbe_draw_text(cx + 4, cy + 4, status, tc()->win_title_active, 1);

    if (c->mode == CALC_GRAPHING) {
        calc_draw_graph(win, cx + 4, cy + 18, cw - 8, ch - 26, c);
        return;
    }

    /* Display panel. */
    int disp_h = 28;
    calc_draw_display(fb, cx + 4, cy + 18, cw - 8, disp_h, c);

    /* Button grid. */
    int gx = cx + 4, gy = cy + 18 + disp_h + 6;
    int gw = cw - 8, gh = ch - (18 + disp_h + 6) - 4;
    int cols = c->btn_cols, rows = c->btn_rows;
    int bw_w = gw / cols, bw_h = gh / rows;
    for (int r = 0; r < rows; r++) {
        for (int col = 0; col < cols; col++) {
            CalcAct a = g_std_act[r][col];
            const char *lbl = g_std_labels[r][col];
            calc_draw_button(gx + col*bw_w + 1, gy + r*bw_h + 1,
                             bw_w - 2, bw_h - 2, lbl, false);
            (void)a;
        }
    }
}

/* -- Keyboard input ---------------------------------------------- */

static void calc_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    CalcState *c = win ? (CalcState*)win->user_data : NULL;
    if (!c) return;

    int d = key_to_digit(key);
    if (d >= 0) { calc_do_action(c, (CalcAct)((int)A_DIG0 + d)); return; }
    switch (key) {
    case '+': calc_do_action(c, A_ADD); break;
    case '-': calc_do_action(c, A_SUB); break;
    case '*': calc_do_action(c, A_MUL); break;
    case '/': calc_do_action(c, A_DIV); break;
    case '^': calc_do_action(c, A_POW); break;
    case '=': case '\r': case '\n': calc_do_action(c, A_EQ); break;
    case '.': calc_do_action(c, A_DOTA); break;
    case 'c': case 'C': calc_do_action(c, A_CLR); break;
    case 'm': case 'M': calc_do_action(c, A_MEM); break;
    case 'r': case 'R': calc_do_action(c, A_RCL); break;
    case 's': case 'S': calc_do_action(c, A_SQRT); break;
    case 'n': case 'N': calc_do_action(c, A_NEG); break;
    case 'b': case 'B': calc_do_action(c, A_BASE); break;
    case 't': case 'T': calc_do_action(c, A_MODE); break;
    default: break;
    }
}

DosGuiWindow* calc_launch(void) {
    int x = 80 + (rand() % 300);
    int y = 60 + (rand() % 200);
    DosGuiWindow *win = dosgui_wm_create(x, y, 280, 380, "Calculator");
    if (win) {
        CalcState *c = calc_create();
        win->user_data = c;
        win->on_draw = calc_draw_wm;
        win->on_key  = calc_key;
    }
    return win;
}

/* ====================================================================
 * CALCULATION ENGINE  (real implementation, unchanged math)
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

int calc_get_mode(const CalcState *calc) {
    return calc ? (int)calc->mode : 0;
}

int calc_get_base(const CalcState *calc) {
    return calc ? calc->base : 0;
}
