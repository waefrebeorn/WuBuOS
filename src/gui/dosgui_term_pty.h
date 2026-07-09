/*
 * dosgui_term_pty.h  --  WuBuOS Terminal PTY Backend API
 *
 * Internal header for PTY session management (spawn, I/O, resize, key handling).
 * Separated from dosgui_term.c to break up the monolithic terminal file.
 */

#ifndef WUBU_DOSGUI_TERM_PTY_H
#define WUBU_DOSGUI_TERM_PTY_H

#include "dosgui_term.h"
#include <sys/types.h>

/* -- PTY Session Lifecycle ---------------------------------------- */

int  term_pty_spawn(const char *shell, const char *cwd, TermPtySession *pty,
                   const char *const *extra_argv);
void term_pty_cleanup(TermPtySession *pty);

/* -- PTY Size Management ------------------------------------------ */

void term_update_pty_size(TermPtySession *pty, int cols, int rows);
void term_reset_pty_screen(TermPtySession *pty);

/* -- PTY I/O ------------------------------------------------------ */

void term_pty_push_line(TermPtySession *pty, const char *line);
void term_process_pty_output(TermPtySession *pty);  /* Implemented in dosgui_term_ansi.c */

/* -- PTY Input Handling ------------------------------------------- */

void term_handle_key_pty(TermState *term, uint32_t key, uint32_t mods);
void term_pty_put_char(TermPtySession *pty, char c, uint8_t attr);
void term_pty_cursor_move(TermPtySession *pty, int x, int y);

/* HolyC REPL key handling */
void term_handle_key_holyc(TermState *term, uint32_t key, uint32_t mods);

/* -- Container PTY (shares PTY infrastructure) -------------------- */

int  term_container_spawn(TermContainerSession *container, const char *container_name);
void term_container_cleanup(TermContainerSession *container);
void term_update_container_size(TermContainerSession *container, int cols, int rows);
void term_process_container_output(TermContainerSession *container);  /* In dosgui_term_ansi.c */
void term_handle_key_container(TermState *term, uint32_t key, uint32_t mods);

#endif /* WUBU_DOSGUI_TERM_PTY_H */