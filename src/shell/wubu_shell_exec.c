/*
 * wubu_shell_exec.c -- WuBuOS shell pipeline + redirection engine
 *                     (BATTLESHIP gaps 230 shell_pipe / 231 shell_redirect)
 *
 * Self-contained. Splits a command line on ';' into independent pipelines.
 * Each pipeline is parsed into STAGES, each stage carrying its own argv and
 * redirection list. Redirections supported:
 *   cmd > file        stdout -> file (truncate)          (fd 1)
 *   cmd >> file       stdout -> file (append)            (fd 1)
 *   cmd < file        stdin  <- file                     (fd 0)
 *   cmd 2> file       fd 2   -> file
 *   cmd 2>> file      fd 2   -> file (append)
 *   cmd 2>&1          fd 2   -> dup of fd 1              (fd duplication)
 *   cmd 1>&2          fd 1   -> dup of fd 2
 * Stages are wired with pipes and run via fork()/execvp(). Returns the last
 * stage's child status. This is the genuine implementation that previously
 * did not exist (form-not-function).
 */

#include "wubu_shell_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

/* ---- redirection + stage model ---- */

typedef enum { R_OUT, R_OUT_APPEND, R_IN, R_DUP } RedirKind;

typedef struct {
    RedirKind kind;
    int       src_fd;          /* fd being redirected (1=out,0=in,2=err) */
    int       dst_fd;          /* for R_DUP: target fd                  */
    char      file[SHELL_MAX_LINE];
} Redir;

#define STAGE_MAX_REDIR 8

typedef struct {
    char  argv_buf[SHELL_MAX_ARGS][SHELL_MAX_LINE];  /* string storage   */
    char *argv[SHELL_MAX_ARGS];                       /* NULL-terminated  */
    int   argc;
    Redir redirs[STAGE_MAX_REDIR];
    int   nredir;
} Stage;

/* Returns 1 if s is a bare non-negative integer (an fd literal prefix). */
static int is_fd_literal(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

/*
 * Parse a single pipeline (no ';') into up to max_stages Stage structs.
 * Returns stage count.
 */
static int parse_pipeline(const char *line, Stage *stages, int max_stages) {
    for (int i = 0; i < max_stages; i++) {
        stages[i].argc = 0;
        stages[i].nredir = 0;
        memset(stages[i].argv, 0, sizeof(stages[i].argv));
    }
    if (max_stages < 1) return 0;

    int nstage = 1;
    Stage *cur = &stages[0];

    char *work = strdup(line);
    if (!work) return 0;
    char *p = work;

    #define PUSH_WORD(src, len) do { \
        if (cur->argc < SHELL_MAX_ARGS - 1) { \
            size_t c = (len) < SHELL_MAX_LINE - 1 ? (len) : SHELL_MAX_LINE - 1; \
            memcpy(cur->argv_buf[cur->argc], (src), c); \
            cur->argv_buf[cur->argc][c] = '\0'; \
            cur->argv[cur->argc] = cur->argv_buf[cur->argc]; \
            cur->argc++; \
        } \
    } while (0)

    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        if (*p == '|') {                       /* new pipeline stage */
            if (nstage < max_stages) { cur = &stages[nstage]; nstage++; }
            p++;
            continue;
        }

        if (*p == '>' || *p == '<') {          /* redirection */
            int is_out = (*p == '>');
            int append = 0;
            if (is_out && p[1] == '>') { append = 1; p++; }
            p++;                                /* consume op char */

            /* The immediately preceding bare word may be the source fd
             * (e.g. "2>" -> src_fd 2). Pop it if it is all digits. */
            int src_fd = is_out ? 1 : 0;
            if (cur->argc > 0) {
                char *last = cur->argv[cur->argc - 1];
                if (is_fd_literal(last)) {
                    src_fd = atoi(last);
                    cur->argc--;                /* consume the fd word */
                    cur->argv[cur->argc] = NULL; /* keep argv NULL-terminated */
                }
            }

            /* The target may directly follow the operator (e.g. "2>&1",
             * "2>/tmp/x") with no intervening space. */
            while (*p && isspace((unsigned char)*p)) p++;
            char *start = p;
            while (*p && !isspace((unsigned char)*p) && *p != '|' && *p != ';')
                p++;
            size_t olen = (size_t)(p - start);
            if (*p) { *p = '\0'; p++; }

            if (cur->nredir < STAGE_MAX_REDIR) {
                Redir *r = &cur->redirs[cur->nredir++];
                r->src_fd = src_fd;
                if (olen > 0 && start[0] == '&') {       /* fd duplication */
                    r->kind = R_DUP;
                    r->dst_fd = atoi(start + 1);
                } else if (is_out) {
                    r->kind = append ? R_OUT_APPEND : R_OUT;
                    memcpy(r->file, start, olen < SHELL_MAX_LINE-1 ? olen : SHELL_MAX_LINE-1);
                    r->file[SHELL_MAX_LINE - 1] = '\0';
                } else {
                    r->kind = R_IN;
                    memcpy(r->file, start, olen < SHELL_MAX_LINE-1 ? olen : SHELL_MAX_LINE-1);
                    r->file[SHELL_MAX_LINE - 1] = '\0';
                }
            }
            continue;
        }

        /* ordinary word (stops at whitespace or shell metacharacters) */
        char *start = p;
        while (*p && !isspace((unsigned char)*p) && *p != '|' &&
               *p != '<' && *p != '>' && *p != ';') p++;
        size_t wlen = (size_t)(p - start);
        PUSH_WORD(start, wlen);
        if (*p && isspace((unsigned char)*p)) p++;
    }

    #undef PUSH_WORD
    free(work);
    return nstage;
}

/* Apply a stage's redirections in the child process. Returns 0 on success. */
static int apply_redirs(Stage *st) {
    for (int i = 0; i < st->nredir; i++) {
        Redir *r = &st->redirs[i];
        if (r->kind == R_DUP) {
            if (dup2(r->dst_fd, r->src_fd) < 0) return -1;
        } else if (r->kind == R_IN) {
            int fd = open(r->file, O_RDONLY);
            if (fd < 0) { fprintf(stderr, "wubu-shell: %s: %s\n",
                                  r->file, strerror(errno)); return -1; }
            dup2(fd, r->src_fd);
            close(fd);
        } else {
            int flags = O_WRONLY | O_CREAT |
                        (r->kind == R_OUT_APPEND ? O_APPEND : O_TRUNC);
            int fd = open(r->file, flags, 0644);
            if (fd < 0) { fprintf(stderr, "wubu-shell: %s: %s\n",
                                  r->file, strerror(errno)); return -1; }
            dup2(fd, r->src_fd);
            close(fd);
        }
    }
    return 0;
}

/* Run a single ';'-free pipeline. Returns last stage status. */
static int run_one_pipeline(ShellState *st, const char *line) {
    Stage stages[SHELL_MAX_ARGS];
    int n = parse_pipeline(line, stages, SHELL_MAX_ARGS);
    if (n == 0) return 0;

    int prev_read = -1;        /* read end of previous stage's pipe  */
    int last_status = 0;

    for (int s = 0; s < n; s++) {
        Stage *stg = &stages[s];
        if (stg->argc == 0) {
            if (prev_read != -1) close(prev_read);
            prev_read = -1;
            continue;
        }

        int is_last = (s == n - 1);
        int pipefd[2] = {-1, -1};
        if (!is_last && pipe(pipefd) != 0) { last_status = 1; break; }

        pid_t pid = fork();
        if (pid == 0) {
            if (prev_read != -1) { dup2(prev_read, STDIN_FILENO); }
            if (!is_last) {
                dup2(pipefd[1], STDOUT_FILENO);
            }
            /* Apply redirections (opens files, dup2's onto 0/1/2). */
            if (apply_redirs(stg) != 0) _exit(1);
            /* Now nuke any other inherited fds (leftover pipe/redir ends from
             * earlier commands) so they cannot leak and corrupt a later
             * command's stdin/stdout wiring. 0/1/2 are already finalized. */
            for (int fd = 3; fd < 256; fd++) close(fd);
            execvp(stg->argv[0], stg->argv);
            fprintf(stderr, "wubu-shell: %s: command not found\n", stg->argv[0]);
            _exit(127);
        }

        /* parent: close local pipe ends; hand read end to next stage */
        if (pipefd[1] != -1) close(pipefd[1]);
        if (is_last) {
            if (prev_read != -1) close(prev_read);
            prev_read = -1;
        } else {
            if (prev_read != -1) close(prev_read);
            prev_read = pipefd[0];
        }

        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) last_status = WEXITSTATUS(status);
    }

    if (prev_read != -1) close(prev_read);
    (void)st;
    return last_status;
}

int shell_exec_pipeline(ShellState *st, const char *line) {
    if (!line || line[0] == '\0') return 0;

    /* Split the line on ';' into independent pipelines. */
    char *work = strdup(line);
    if (!work) return 0;
    int last_status = 0;
    char *seg = work;
    while (*seg) {
        char *end = seg;
        while (*end && *end != ';') end++;
        int was_sep = (*end == ';');
        *end = '\0';

        /* Trim surrounding whitespace of this segment. */
        char *s = seg;
        while (*s && isspace((unsigned char)*s)) s++;
        char *e = end;
        while (e > s && isspace((unsigned char)*(e - 1))) e--;
        *e = '\0';

        if (*s) last_status = run_one_pipeline(st, s);
        seg = was_sep ? end + 1 : end;
    }
    free(work);
    return last_status;
}
