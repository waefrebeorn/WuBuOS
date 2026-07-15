/*
 * wubu_shell_history.c -- WuBuOS shell command history (BATTLESHIP gap 228)
 *
 * Self-contained ring buffer of past command lines. Up/down navigation is
 * exposed via shell_history_prev()/shell_history_next(); the REPL driver
 * records history with shell_history_push(). No dependency on anything but
 * the shell internal state.
 */

#include "wubu_shell_internal.h"

#include <string.h>

void shell_history_push(ShellState *st, const char *line) {
    if (!st || !line || line[0] == '\0') return;

    /* De-dupe against the most recent entry (don't stack repeats). */
    if (st->hist_count > 0) {
        const char *last = st->hist_buf[(st->hist_count - 1) % SHELL_HIST_MAX];
        if (strcmp(last, line) == 0) {
            st->hist_cur = -1;            /* reset navigation cursor */
            return;
        }
    }

    int idx = st->hist_count % SHELL_HIST_MAX;
    strncpy(st->hist_buf[idx], line, SHELL_MAX_LINE - 1);
    st->hist_buf[idx][SHELL_MAX_LINE - 1] = '\0';
    st->hist_count++;
    st->hist_cur = -1;                    /* reset navigation cursor */
}

const char *shell_history_prev(ShellState *st) {
    if (!st || st->hist_count == 0) return NULL;
    int total = st->hist_count < SHELL_HIST_MAX ? st->hist_count : SHELL_HIST_MAX;
    if (st->hist_cur == -1)
        st->hist_cur = total - 1;          /* start at newest */
    else if (st->hist_cur > 0)
        st->hist_cur--;
    return st->hist_buf[st->hist_cur];
}

const char *shell_history_next(ShellState *st) {
    if (!st || st->hist_count == 0) return NULL;
    if (st->hist_cur == -1) return NULL;  /* already at the prompt */
    if (st->hist_cur + 1 >= (st->hist_count < SHELL_HIST_MAX
                                 ? st->hist_count : SHELL_HIST_MAX)) {
        st->hist_cur = -1;                 /* fell off the end -> empty line */
        return NULL;
    }
    st->hist_cur++;
    return st->hist_buf[st->hist_cur];
}

int shell_history_count(const ShellState *st) {
    if (!st) return 0;
    return st->hist_count < SHELL_HIST_MAX ? st->hist_count : SHELL_HIST_MAX;
}
