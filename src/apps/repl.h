/*
 * repl.h  --  My Seed HolyC REPL
 *
 * The REPL evaluates HolyC through the compiler's hd_eval().  This header is
 * the public face: the window entry point, the live instance (so a test can
 * inspect the buffer keystrokes actually reached), and the input primitives
 * the window's on_key slot forwards to.
 */
#ifndef MYSEED_REPL_H
#define MYSEED_REPL_H

#include <stdint.h>

#define REPL_MAX_LINES  1000
#define REPL_LINE_LEN   256

typedef struct {
    char  lines[REPL_MAX_LINES][REPL_LINE_LEN];
    int   line_count;
    char  input[REPL_LINE_LEN];
    int   input_pos;
    int   scroll_offset;
    void *db;   /* gui_dbuf_t* -- opaque here so this header stays light */
} REPLState;

/* Start the REPL as a GUI window (framebuffer dimensions for the backbuffer). */
void repl_start(int fb_w, int fb_h);

/* The live REPL instance. */
REPLState *repl_state(void);

/* Input primitives: printable char, backspace, and submit-and-evaluate.
 * Exposed so the window slot and the test drive exactly the same code. */
void repl_input_char(REPLState *r, char c);
void repl_input_backspace(REPLState *r);
void repl_submit(REPLState *r);

#endif /* MYSEED_REPL_H */
