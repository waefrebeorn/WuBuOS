/*
 * dosgui_wm_holyc_term.c  --  HolyC Terminal subsystem for dosgui_wm
 *
 * Provides a REPL terminal window for HolyC script evaluation.
 * Extracted from dosgui_wm.c for modularity.
 *
 * NOTE: g_holyc_terms[] is defined as a static global in dosgui_wm.c.
 * This module provides the helper functions that operate on it.
 *
 * Evaluation is dependency-injected (see dosgui_wm_holyc_term.h). The default
 * evaluator is a direct, self-contained JIT compile+run via the public HolyC
 * compiler API. The hosted binary injects the richer wubu_holyc_agi path
 * (holyd daemon + EDR disclosure) at the composition root, so this module
 * stays decoupled from the runtime AGI/EDR layer.
 */

#include "dosgui_wm_internal.h"
#include "dosgui_wm_holyc_term.h"

#include "holyc_codegen.h"   /* public compiler API: hc_eval */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- Injected evaluator (DI) -------------------------------------- */

static int holyc_eval_default(const char *src, char *out, size_t out_size) {
    if (!src || !out || out_size == 0) return -1;
    int64_t r = hc_eval(src);
    snprintf(out, out_size, "%lld", (long long)r);
    return 0;
}

static holyc_term_eval_fn g_eval = holyc_eval_default;

void holyc_term_set_eval(holyc_term_eval_fn fn) {
    g_eval = fn ? fn : holyc_eval_default;
}

/* -- HolyC Terminal Implementation (matching original code) ------- */

void holyc_term_init_compiler(HolycTerm *term) {
    if (!term || term->initialized) return;
    term->buffer[0][0] = '\0';
    term->input[0] = '\0';
    term->cursor_pos = 0;
    term->initialized = true;
}

void holyc_term_add_line(HolycTerm *term, const char *line) {
    if (!term || !line) return;
    /* Scroll buffer up */
    for (int i = 31; i > 0; i--)
        snprintf(term->buffer[i], sizeof(term->buffer[i]), "%s", term->buffer[i - 1]);
    snprintf(term->buffer[0], sizeof(term->buffer[0]), "%s", line);
}

void holyc_term_add_history(HolycTerm *term, const char *cmd) {
    if (!term || !cmd) return;
    if (term->hist_count < 16)
        snprintf(term->history[term->hist_count++], sizeof(term->history[0]), "%s", cmd);
    else {
        for (int i = 0; i < 15; i++)
            snprintf(term->history[i], sizeof(term->history[i]), "%s", term->history[i + 1]);
        snprintf(term->history[15], sizeof(term->history[15]), "%s", cmd);
    }
    term->hist_pos = term->hist_count;
}

void holyc_term_eval(HolycTerm *term, const char *cmd) {
    if (!term || !cmd) return;

    if (strcmp(cmd, "exit") == 0) {
        holyc_term_add_line(term, "Goodbye.");
        return;
    }
    if (strcmp(cmd, "clear") == 0) {
        memset(term->buffer, 0, sizeof(term->buffer));
        term->input[0] = '\0';
        term->cursor_pos = 0;
        holyc_term_add_line(term, "WuBuOS HolyC Terminal v0.1");
        return;
    }
    if (strcmp(cmd, "help") == 0) {
        holyc_term_add_line(term, "HolyC Terminal -- live ring-0 compiler.");
        holyc_term_add_line(term, "  Type HolyC expressions/statements; they compile + run now.");
        holyc_term_add_line(term, "  e.g. 1+2+3   { I64 x=5; x*x; }   I64 sq(I64 n){return n*n;} sq(9);");
        holyc_term_add_line(term, "  State (vars/functions) persists across lines. exit/clear/help builtins.");
        return;
    }

    /* Real compile + execute via the injected evaluator. */
    char result[1024];
    int ret = g_eval(cmd, result, sizeof(result));
    char line[1280];
    if (ret == 0)
        snprintf(line, sizeof(line), "» %s", result);
    else
        snprintf(line, sizeof(line), "⚠ %s", result);
    holyc_term_add_line(term, line);
}

void holyc_term_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    (void)fb_w;
    (void)fb_h;
    HolycTerm *term = (HolycTerm*)win->user_data;
    if (!term || !win) return;

    const int tbh = title_bar_height();
    int cx = win->x + 2;
    int cy = win->y + tbh + 2;
    int cw = win->w - 4;
    int ch = win->h - tbh - taskbar_height_dynamic() - 6;

    int line_h = 16;
    int lines = ch / line_h;
    int start = term->scroll_y;

    for (int i = 0; i < lines && i < 32; i++) {
        int idx = (start + i) % 32;
        if (term->buffer[idx][0])
            vbe_draw_text(cx + 4, cy + i * line_h, term->buffer[idx], 0xFF00FF00, 1);
    }

    char input_line[272];
    snprintf(input_line, sizeof(input_line), "> %s", term->input);
    vbe_draw_text(cx + 4, cy + lines * line_h - line_h, input_line, 0xFF00FF00, 1);
}

void holyc_term_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    HolycTerm *term = (HolycTerm*)win->user_data;
    if (!term) return;

    if (key == '\n' || key == '\r') {
        holyc_term_add_history(term, term->input);
        holyc_term_eval(term, term->input);
        memset(term->input, 0, sizeof(term->input));
        term->cursor_pos = 0;
        return;
    }

    if (key == 0x08 || key == 0x7F) { /* Backspace */
        if (term->cursor_pos > 0)
            term->input[--term->cursor_pos] = '\0';
        return;
    }

    if (key == 0x1B) { /* Escape */
        memset(term->input, 0, sizeof(term->input));
        term->cursor_pos = 0;
        return;
    }

    if (key >= 32 && key <= 126 && term->cursor_pos < 255) {
        term->input[term->cursor_pos++] = (char)key;
        term->input[term->cursor_pos] = '\0';
    }
}

/* -- Public API (called from dosgui_wm.c) -------------------------- */

DosGuiWindow *dosgui_wm_spawn_holyc_term(int x, int y, int w, int h) {
    DosGuiWindow *win = dosgui_wm_create(x, y, w, h, "HolyC Terminal");
    if (!win) return NULL;

    /* Find index */
    int idx = -1;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) { idx = i; break; }
    }
    if (idx < 0) return NULL;

    /* Set up callbacks */
    win->on_draw = holyc_term_draw;
    win->on_key = holyc_term_on_key;
    win->user_data = calloc(1, sizeof(HolycTerm));
    if (win->user_data) {
        HolycTerm *term = (HolycTerm*)win->user_data;
        term->initialized = true;
        holyc_term_add_line(term, "WuBuOS HolyC Terminal v0.1");
    }

    return win;
}
