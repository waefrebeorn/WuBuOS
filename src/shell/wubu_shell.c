/*
 * wubu_shell.c -- WuBuOS shell: state, REPL driver, main entry
 *
 * Self-contained. Owns the ShellState lifecycle and the interactive read
 * loop (line editing with history recall + tab completion). The heavy
 * sub-concerns are delegated to focused modules behind the internal header:
 *   wubu_shell_history.c   -- command history ring        (gap 228)
 *   wubu_shell_complete.c  -- tab completion of commands  (gap 229)
 *   wubu_shell_exec.c      -- pipeline + redirection exec (gaps 230/231)
 */

#include "wubu_shell_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>   /* chdir */

/* ---- state lifecycle ---- */

ShellState *shell_state_new(void) {
    ShellState *st = (ShellState *)calloc(1, sizeof(ShellState));
    if (!st) return NULL;
    st->running = 1;
    st->out = stdout;
    st->hist_cur = -1;
    return st;
}

void shell_state_free(ShellState *st) {
    if (!st) return;
    free(st);
}

void shell_print(ShellState *st, const char *msg) {
    if (!st || !msg) return;
    FILE *out = st->out ? st->out : stdout;
    fputs(msg, out);
    fflush(out);
}

/* ---- command dispatch (single line) ---- */

int shell_dispatch(ShellState *st, const char *line) {
    if (!st || !line) return -1;

    /* Trim leading/trailing whitespace. */
    while (*line && isspace((unsigned char)*line)) line++;
    char buf[SHELL_MAX_LINE];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    size_t n = strlen(buf);
    while (n > 0 && isspace((unsigned char)buf[n - 1])) buf[--n] = '\0';

    if (buf[0] == '\0') return 0;          /* empty line: no-op */

    /* Record every executed, non-empty line in history (gap 228). */
    shell_history_push(st, buf);

    /* Builtins evaluated before the pipeline engine. */
    if (strcmp(buf, "exit") == 0 || strcmp(buf, "quit") == 0) {
        st->running = 0;
        return 0;
    }
    if (strcmp(buf, "history") == 0) {
        for (int i = 0; i < shell_history_count(st); i++) {
            fprintf(st->out ? st->out : stdout, "  %2d  %s\n",
                    i + 1, st->hist_buf[i]);
        }
        return 0;
    }
    if (strcmp(buf, "help") == 0) {
        FILE *o = st->out ? st->out : stdout;
        fputs("WuBuOS shell builtins:\n"
              "  exit, quit   leave the shell\n"
              "  history      show command history\n"
              "  help         this message\n"
              "  cd <dir>     change directory\n"
              "Supports pipelines (|), redirection (>, >>, <, 2>, 2>&1),\n"
              "command lists (;), and tab completion of known commands.\n", o);
        fflush(o);
        return 0;
    }
    if (strncmp(buf, "cd ", 3) == 0) {
        const char *dir = buf + 3;
        while (*dir && isspace((unsigned char)*dir)) dir++;
        if (chdir(dir) != 0) {
            fprintf(st->out ? st->out : stdout,
                    "wubu-shell: cd: %s: %s\n", dir, strerror(errno));
            return 1;
        }
        return 0;
    }

    /* Otherwise run through the real pipeline/redirection engine. */
    int rc = shell_exec_pipeline(st, buf);
    return rc;
}

/* ---- REPL driver ---- */

void shell_run(ShellState *st, int argc, char **argv) {
    if (!st) return;
    st->argc = argc;
    st->argv = argv;

    FILE *in = stdin;
    char line[SHELL_MAX_LINE];

    shell_print(st, "WuBuOS shell v1.0 -- type 'exit' to quit\n");

    while (st->running) {
        fprintf(st->out ? st->out : stdout, "$ ");
        fflush(st->out ? st->out : stdout);

        if (!fgets(line, sizeof(line), in))
            break;                 /* EOF (Ctrl-D) */

        /* strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        shell_dispatch(st, line);
    }
    shell_print(st, "Bye.\n");
}

/* ---- task entry point (resolves the weak hook in wubu_metal.c) ---- */

void wubu_shell_run(void *arg) {
    (void)arg;
    ShellState *st = shell_state_new();
    if (!st) return;
    shell_run(st, 0, NULL);
    shell_state_free(st);
}

#ifndef WUBU_SHELL_NO_MAIN
/* Standalone shell binary entry (used by `make shell`). */
int main(int argc, char **argv) {
    ShellState *st = shell_state_new();
    if (!st) return 1;
    shell_run(st, argc, argv);
    shell_state_free(st);
    return 0;
}
#endif
