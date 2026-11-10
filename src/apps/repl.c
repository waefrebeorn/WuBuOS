/*
 * repl.c  --  My Seed HolyC JIT REPL (runs inside GUI window)
 * Uses the HolyC compiler (hd_eval) for evaluation
 * Updated to use DosGui WM API (Cell 400)
 */
#include "repl.h"
#include "holyd.h"
#include "../gui/wm.h"
#include "../gui/compositor/gui_dbuf.h"
#include "../gui/shell/window_chrome.h"
#include "../kernel/vbe.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define REPL_PROMPT     "$ "

static REPLState g_repl = {0};

REPLState *repl_state(void) {
    return &g_repl;
}

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

static gui_dbuf_t *repl_dbuf(void) {
    return (gui_dbuf_t *)g_repl.db;
}

static void repl_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;

    gui_dbuf_t *db = repl_dbuf();
    if (!db) return;

    gui_dbuf_clear(db, 0x00000000);

    /* Centralized window chrome: draw frame + title bar, get content rect.
     * ADR-001: apps draw ONLY within the chrome-provided content rect. */
    ChromeContentRect content = chrome_draw_window(win, fb, fb_w, fb_h);
    int x = content.x + 4;
    int y = content.y + 4;
    int line_height = 10;
    int max_visible = (content.h - 8) / line_height;

    /* Draw output lines (most recent at bottom) */
    int start = g_repl.line_count - max_visible;
    if (start < 0) start = 0;

    for (int i = start; i < g_repl.line_count; i++) {
        repl_draw_string(db, x, y, g_repl.lines[i], 0x00FFFFFF);
        y += line_height;
    }

    /* Draw input line */
    char prompt_line[REPL_LINE_LEN + 8];
    snprintf(prompt_line, sizeof(prompt_line), "%s%s", REPL_PROMPT, g_repl.input);
    repl_draw_string(db, x, y, prompt_line, 0x00FFFF00);
    y += line_height;

    /* Draw cursor */
    int cursor_x = x + (2 + g_repl.input_pos) * 8; /* 2 for "$ " */
    gui_dbuf_vline(db, cursor_x, y - line_height, y - 1, 0x00FFFF00);

    /* Surface effects, if a stack was attached to this buffer.  Applied here,
     * between drawing and presenting -- never inside gui_dbuf_flip, because a
     * pass is not idempotent and a flip can happen more than once per frame. */
    gui_dbuf_effect_apply(db);
}

/* -- Input primitives ---------------------------------------------
 * Extracted from the window's on_key slot so a test drives exactly the code
 * a keystroke does. */

void repl_input_char(REPLState *r, char c) {
    if (!r || r->input_pos >= REPL_LINE_LEN - 1) return;
    r->input[r->input_pos++] = c;
    r->input[r->input_pos] = '\0';
}

void repl_input_backspace(REPLState *r) {
    if (!r || r->input_pos <= 0) return;
    r->input[--r->input_pos] = '\0';
}

void repl_submit(REPLState *r) {
    if (!r) return;
    if (r->input[0]) {
        if (r->line_count < REPL_MAX_LINES) {
            snprintf(r->lines[r->line_count], REPL_LINE_LEN,
                     "%s%s", REPL_PROMPT, r->input);
            r->line_count++;
        }

        int64_t result = hd_eval(r->input);

        if (r->line_count < REPL_MAX_LINES) {
            snprintf(r->lines[r->line_count], REPL_LINE_LEN,
                     "= %ld", (long)result);
            r->line_count++;
        }
    }
    r->input[0] = '\0';
    r->input_pos = 0;
}

static void repl_handle_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win; (void)mods;
    REPLState *r = &g_repl;

    if (key == '\n' || key == '\r') {
        repl_submit(r);
    } else if (key == 0x08 || key == 0x7F) {   /* Backspace / Delete */
        repl_input_backspace(r);
    } else if (key == 0x1B) {                    /* Escape: clear the line */
        r->input[0] = '\0';
        r->input_pos = 0;
    } else if (key >= 32 && key <= 126) {
        repl_input_char(r, (char)key);
    }
}

void repl_start(int fb_w, int fb_h) {
    g_repl.line_count = 0;
    g_repl.input_pos = 0;
    g_repl.scroll_offset = 0;

    /* Initialize double-buffer for REPL rendering */
    if (!g_repl.db) {
        g_repl.db = malloc(sizeof(gui_dbuf_t));
        if (g_repl.db) gui_dbuf_init((gui_dbuf_t *)g_repl.db, 640, 480);
    }

    DosGuiWindow *win = wm_create(100, 100, 400, 400, "HolyC REPL");
    if (win) {
        win->on_draw  = repl_draw;
        win->on_key   = repl_handle_key;
        snprintf(win->title, sizeof(win->title), "HolyC REPL");
    }
}
