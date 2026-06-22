/*
 * repl.c  --  My Seed HolyC JIT REPL (runs inside GUI window)
 * Uses the HolyC compiler (hc_eval) for evaluation
 * Updated to use DosGui WM API (Cell 400)
 */
#include "repl.h"
#include "holyc.h"
#include "../gui/dosgui_wm.h"
#include "../gui/gui_dbuf.h"
#include "../kernel/vbe.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define REPL_MAX_LINES  1000
#define REPL_LINE_LEN   256
#define REPL_PROMPT     "$ "

struct REPLState {
    char    lines[REPL_MAX_LINES][REPL_LINE_LEN];
    int     line_count;
    char    input[REPL_LINE_LEN];
    int     input_pos;
    int     scroll_offset;
    gui_dbuf_t *db;  /* Double-buffer for rendering */
};

static struct REPLState g_repl = {0};

/* Draw a string at position using the 8x8 font */
static void repl_draw_string(gui_dbuf_t *db, int x, int y, const char *str, uint32_t color) {
    int cx = x;
    for (int i = 0; str[i] && cx < db->width - 8; i++) {
        if (str[i] == '\n') {
            cx = x;
            y += 10; /* 8px font + 2px spacing */
        } else {
            gui_dbuf_draw_char(db, cx, y, str[i], color);
            cx += 8; /* 8px wide glyph */
        }
    }
}

static void repl_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;

    if (!g_repl.db) return;

    gui_dbuf_clear(g_repl.db, 0x00000000);

    int x = 4;
    int y = 4;
    int line_height = 10; /* 8px font + 2px spacing */
    int max_visible = (win->h - DOSGUI_TITLE_H - 8) / line_height;

    /* Draw output lines (most recent at bottom) */
    int start = g_repl.line_count - max_visible;
    if (start < 0) start = 0;

    for (int i = start; i < g_repl.line_count; i++) {
        repl_draw_string(g_repl.db, x, y, g_repl.lines[i], 0x00FFFFFF);
        y += line_height;
    }

    /* Draw input line */
    char prompt_line[REPL_LINE_LEN + 8];
    snprintf(prompt_line, sizeof(prompt_line), "%s%s", REPL_PROMPT, g_repl.input);
    repl_draw_string(g_repl.db, x, y, prompt_line, 0x00FFFF00);
    y += line_height;

    /* Draw cursor */
    int cursor_x = x + (2 + g_repl.input_pos) * 8; /* 2 for "$ " */
    gui_dbuf_vline(g_repl.db, cursor_x, y - line_height, y - 1, 0x00FFFF00);
}

static void repl_handle_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win; (void)mods;
    if (key == '\n' || key == '\r') {
        /* Execute the input line via HolyC compiler */
        if (g_repl.input[0]) {
            /* Store input as output line */
            if (g_repl.line_count < REPL_MAX_LINES) {
                snprintf(g_repl.lines[g_repl.line_count], REPL_LINE_LEN,
                         "%s%s", REPL_PROMPT, g_repl.input);
                g_repl.line_count++;
            }

            /* Evaluate via HolyC compiler */
            int64_t result = hc_eval(g_repl.input);

            /* Store result as output line */
            if (g_repl.line_count < REPL_MAX_LINES) {
                snprintf(g_repl.lines[g_repl.line_count], REPL_LINE_LEN,
                         "= %ld", (long)result);
                g_repl.line_count++;
            }
        }
        g_repl.input[0] = '\0';
        g_repl.input_pos = 0;
    } else if (key == 8 && g_repl.input_pos > 0) { /* Backspace */
        g_repl.input[--g_repl.input_pos] = '\0';
    } else if (key >= 32 && key < 127 && g_repl.input_pos < REPL_LINE_LEN - 1) {
        g_repl.input[g_repl.input_pos++] = key;
        g_repl.input[g_repl.input_pos] = '\0';
    }
}

void repl_start(int fb_w, int fb_h) {
    g_repl.line_count = 0;
    g_repl.input_pos = 0;
    g_repl.scroll_offset = 0;

    /* Initialize double-buffer for REPL rendering */
    g_repl.db = (gui_dbuf_t *)malloc(sizeof(gui_dbuf_t));
    if (g_repl.db) {
        gui_dbuf_init(g_repl.db, 640, 480);
    }

    DosGuiWindow *win = dosgui_wm_create(100, 100, 400, 400, "HolyC REPL");
    if (win) {
        win->on_draw  = repl_draw;
        win->on_key   = repl_handle_key;
        snprintf(win->title, sizeof(win->title), "HolyC REPL");
    }
}
