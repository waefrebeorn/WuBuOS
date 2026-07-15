/*
 * wubu_shell_internal.h -- shell-internal opaque state + cross-module API
 *
 * Self-contained, no god headers: includes only the public shell API and
 * the bare C stdint/stddef types. The full ShellState layout lives here so
 * every split module (main / history / completion / exec) can see it, while
 * the public header keeps it opaque.
 */

#ifndef WUBU_SHELL_INTERNAL_H
#define WUBU_SHELL_INTERNAL_H

#include "wubu_shell.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>   /* FILE* used by the shell output sink */

#define SHELL_MAX_LINE  1024   /* max command line length            */
#define SHELL_MAX_ARGS  64     /* max argv entries per pipeline stage */
#define SHELL_HIST_MAX  64     /* ring size for command history       */

/*
 * Opaque-to-public shell runtime state. All fields are private to the shell
 * implementation; only this translation unit group may touch them.
 */
struct ShellState {
    int    running;          /* loop flag, cleared by `exit` builtin */
    int    argc;
    char **argv;
    FILE  *out;              /* output sink (defaults to stdout)     */

    /* command history ring (gaps 228: shell_history) */
    char   hist_buf[SHELL_HIST_MAX][SHELL_MAX_LINE];
    int    hist_count;       /* total entries (<= SHELL_HIST_MAX)    */
    int    hist_cur;         /* navigational cursor for up/down     */
};

typedef struct ShellState ShellState;

/* ---- main / REPL driver (wubu_shell.c) ---- */
ShellState *shell_state_new(void);
void        shell_state_free(ShellState *st);
void        shell_run(ShellState *st, int argc, char **argv);
int         shell_dispatch(ShellState *st, const char *line); /* run one line */
void        shell_print(ShellState *st, const char *msg);     /* write to sink */

/* ---- history (wubu_shell_history.c) : gap 228 ---- */
void        shell_history_push(ShellState *st, const char *line);
const char *shell_history_prev(ShellState *st);   /* walk back; NULL past start */
const char *shell_history_next(ShellState *st);   /* walk forward; NULL past end */
int         shell_history_count(const ShellState *st);

/* ---- completion (wubu_shell_complete.c) : gap 229 ---- */
/* Fill buf with the longest-common-prefix completion of `partial` against the
 * registered command table. Returns match count: 0 none, 1 exact, >1 ambiguous.
 * On >1 the caller may list candidates via shell_complete_list(). */
int  shell_complete(ShellState *st, const char *partial,
                    char *buf, size_t bufsz);
/* Enumerate up to maxn candidate command names for `partial` into out[][].
 * Returns the number written. */
int  shell_complete_list(const char *partial,
                         char out[][SHELL_MAX_LINE], int maxn);

/* ---- pipeline / redirect engine (wubu_shell_exec.c) : gaps 230/231 ---- */
/* Parse and execute a possibly-piped, possibly-redirected command line.
 * Returns the last stage's process exit status (0 = success). */
int  shell_exec_pipeline(ShellState *st, const char *line);

#endif /* WUBU_SHELL_INTERNAL_H */
