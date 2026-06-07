/*
 * repl.c — My Seed HolyC JIT REPL (runs inside GUI window)
 */
#include "repl.h"
#include "../jit/jit.h"
#include "../gui/wm.h"
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
    JITContext *jit;
    int     scroll_offset;
};

static struct REPLState g_repl = {0};

static void repl_draw(Window *win, void *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    /* Draw REPL background */
    int x = win->x + 4, y = win->y + WM_TITLE_HEIGHT + 4;
    vbe_fill_rect(x, y, win->w - 8, win->h - WM_TITLE_HEIGHT - 8, 0x00000000);
    
    /* Draw output lines */
    int max_visible = (win->h - WM_TITLE_HEIGHT - 20) / 16;
    int start = g_repl.line_count - max_visible;
    if (start < 0) start = 0;
    
    /* Draw input line */
    vbe_fill_rect(x, y + max_visible * 16, win->w - 8, 16, 0x00101010);
}

static void repl_handle_key(Window *win, uint32_t key, uint32_t mods) {
    (void)win; (void)mods;
    if (key == '\n' || key == '\r') {
        /* Execute the input line via JIT */
        if (g_repl.jit && g_repl.input[0]) {
            JITFunc fn;
            JITResult r = jit_compile(g_repl.jit, g_repl.input, JIT_LANG_C,
                                       "__repl_expr", &fn);
            if (r == JIT_OK) {
                int64_t result = jit_call0(&fn);
                /* Store result as output line */
                if (g_repl.line_count < REPL_MAX_LINES) {
                    snprintf(g_repl.lines[g_repl.line_count], REPL_LINE_LEN,
                             "= %ld", result);
                    g_repl.line_count++;
                }
                jit_func_free(&fn);
            } else {
                if (g_repl.line_count < REPL_MAX_LINES) {
                    strncpy(g_repl.lines[g_repl.line_count], "ERROR", REPL_LINE_LEN);
                    g_repl.line_count++;
                }
            }
        }
        g_repl.input[0] = '\0';
        g_repl.input_pos = 0;
    } else if (key == 8 && g_repl.input_pos > 0) {
        g_repl.input[--g_repl.input_pos] = '\0';
    } else if (key >= 32 && key < 127 && g_repl.input_pos < REPL_LINE_LEN - 1) {
        g_repl.input[g_repl.input_pos++] = key;
        g_repl.input[g_repl.input_pos] = '\0';
    }
}

void repl_start(JITContext *jit_ctx) {
    g_repl.jit = jit_ctx;
    g_repl.line_count = 0;
    g_repl.input_pos = 0;
    
    Window *win = wm_create_window(640, 100, 380, 500, "Temple REPL");
    if (win) {
        win->on_draw  = repl_draw;
        win->on_key   = repl_handle_key;
        win->title_color = 0x00404080; /* Temple green-black */
    }
}
