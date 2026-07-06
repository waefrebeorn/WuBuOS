/*
 * dosgui_term_internal.h  --  WuBuOS Terminal internal API
 *
 * Shared between dosgui_term.c and dosgui_term_ansi.c.
 * Holds the polymorphic screen interface used by the (deduplicated)
 * VT100/ANSI parser.
 *
 * BOTH TermPtySession and TermContainerSession have rows/cols/screen/
 * attrs/cursor_x/cursor_y/cur_attr/cur_fg/cur_bg/saved_cursor_x/y
 * fields. TermScreen is a flat-pointer view into one of those structs
 * so the shared parser can be called on either.
 */

#ifndef WUBU_DOSGUI_TERM_INTERNAL_H
#define WUBU_DOSGUI_TERM_INTERNAL_H

#include "dosgui_term.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* -- Polymorphic screen view -------------------------------------- */

/*
 * The parser operates on this view rather than directly on
 * TermPtySession / TermContainerSession — keeps it polymorphic so
 * both backing stores can be used without duplicating the parser.
 *
 * Pointers are into a live TermPtySession or TermContainerSession.
 * Do not free the backing storage while a TermScreen is in scope.
 */
typedef struct {
    int    *cols;
    int    *rows;
    int    *cursor_x;
    int    *cursor_y;
    int    *saved_cursor_x;
    int    *saved_cursor_y;
    uint8_t *cur_attr;
    uint8_t *cur_fg;
    uint8_t *cur_bg;

    /* Flat pointer to screen[row * MAX_COLS + col]. */
    char    *screen;
    /* Flat pointer to attrs[row * MAX_COLS + col]. */
    uint8_t *attrs;

    /* Buffer dimensions (compile-time constants from the public header). */
    int      max_cols;
    int      max_rows;
} TermScreen;

/* -- Screen binders ----------------------------------------------- */

/* Bind a TermScreen to a PTY session (PTY doesn't track dirty rows here
 * because the parser operates on flat screen/attrs; the renderer is
 * responsible for marking dirty rows). */
void term_screen_bind_pty(TermScreen *out, TermPtySession *pty);

/* Same, for a container session. */
void term_screen_bind_container(TermScreen *out, TermContainerSession *container);

/* -- Shared VT100/ANSI parser ------------------------------------- */

/*
 * Drive the VT100/ANSI state machine over a byte buffer.
 * Implements CSIs, SGR, cursor movement, scroll, erase in display/line,
 * 256-color foreground/background, DECSC/DECRC, save/restore cursor.
 * Decoded bytes update `scr` in place.
 *
 * State (NORMAL/ESC/CSI, parameter buffer, accumulator) is bound to
 * `scr` so callers do not need to track it between invocations.
 *
 * This is the canonical implementation that used to live inline in
 * term_process_pty_output and term_process_container_output.
 */
void term_ansi_parse(TermScreen *scr, const char *buf, int n);

/*
 * Convenience: read from fd (non-blocking) and dispatch into the parser.
 * Used by term_process_pty_output and term_process_container_output.
 * Returns 1 if any bytes were processed, 0 otherwise.
 */
int  term_ansi_drain_fd(int fd, TermScreen *scr);

/* -- PTY/Container session management ------------------------------- */

/* PTY session lifecycle */
int  term_pty_spawn(const char *shell, const char *cwd, TermPtySession *pty);
void term_pty_cleanup(TermPtySession *pty);
void term_update_pty_size(TermPtySession *pty, int cols, int rows);
void term_reset_pty_screen(TermPtySession *pty);
void term_pty_push_line(TermPtySession *pty, const char *line);

/* PTY I/O and input handling */
void term_process_pty_output(TermPtySession *pty);
void term_handle_key_pty(TermState *term, uint32_t key, uint32_t mods);
void term_pty_put_char(TermPtySession *pty, char c, uint8_t attr);
void term_pty_cursor_move(TermPtySession *pty, int x, int y);

/* Container session lifecycle */
int  term_container_spawn(TermContainerSession *container, const char *container_name);
void term_container_cleanup(TermContainerSession *container);
void term_update_container_size(TermContainerSession *container, int cols, int rows);

/* Container I/O and input handling */
void term_process_container_output(TermContainerSession *container);
void term_handle_key_container(TermState *term, uint32_t key, uint32_t mods);
void term_handle_key_holyc(TermState *term, uint32_t key, uint32_t mods);

/* Render helpers (shared with dosgui_term.c) */
void term_render_pty_session(TermPtySession *pty, uint32_t *fb, int x, int y, int w, int h);
void term_render_container_session(TermContainerSession *container, uint32_t *fb, int x, int y, int w, int h);

#endif /* WUBU_DOSGUI_TERM_INTERNAL_H */
