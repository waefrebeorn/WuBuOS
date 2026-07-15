/*
 * wubu_shell_complete.c -- WuBuOS shell tab completion (BATTLESHIP gap 229)
 *
 * Self-contained. Completes a partial token against the builtin + program
 * command table. Returns the longest common prefix (the canonical readline
 * behaviour); on ambiguity the caller can enumerate candidates.
 *
 * The command table is intentionally local to this module (no god header):
 * it is the single source of truth for shell-known command names.
 */

#include "wubu_shell_internal.h"

#include <string.h>

/* Local table of shell-known command names (builtins + common host tools). */
static const char *kCmdTable[] = {
    /* builtins */
    "exit", "quit", "history", "cd", "pwd", "echo", "help",
    /* host tools the shell can launch */
    "ls", "cat", "grep", "wc", "head", "tail", "sort", "uniq",
    "sed", "awk", "tr", "cut", "date", "uname", "env", "which",
    NULL
};

static int cmd_matches(const char *name, const char *partial) {
    return strncmp(name, partial, strlen(partial)) == 0;
}

int shell_complete_list(const char *partial,
                        char out[][SHELL_MAX_LINE], int maxn) {
    int found = 0;
    if (!partial) partial = "";
    for (int i = 0; kCmdTable[i] && found < maxn; i++) {
        if (cmd_matches(kCmdTable[i], partial)) {
            strncpy(out[found], kCmdTable[i], SHELL_MAX_LINE - 1);
            out[found][SHELL_MAX_LINE - 1] = '\0';
            found++;
        }
    }
    return found;
}

int shell_complete(ShellState *st, const char *partial,
                   char *buf, size_t bufsz) {
    (void)st;
    char cands[SHELL_HIST_MAX][SHELL_MAX_LINE];
    int n = shell_complete_list(partial ? partial : "", cands, SHELL_HIST_MAX);
    if (n == 0 || bufsz == 0) {
        if (buf && bufsz) buf[0] = '\0';
        return 0;
    }

    /* Seed with the first candidate, then narrow to the longest common prefix. */
    size_t lcp = strlen(cands[0]);
    for (int i = 1; i < n; i++) {
        size_t j = 0;
        while (j < lcp && j < strlen(cands[i]) &&
               cands[0][j] == cands[i][j]) j++;
        lcp = j;
    }
    size_t copy = lcp < bufsz ? lcp : bufsz - 1;
    memcpy(buf, cands[0], copy);
    buf[copy] = '\0';
    return n;                           /* 1 = exact, >1 = ambiguous */
}
