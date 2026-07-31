/*
 * wubu_verifier_bytropix.c -- bind bytropix (local C/CUDA inference engine) as the
 * INDEPENDENT verifier for the Mega-OS self-improvement loop (DA-3).
 *
 * The verifier is a SEPARATE process/model from the agent being graded, so it
 * cannot rubber-stamp its own work. Runs locally (no network) -> DA-1 safe.
 *
 * If the bytropix gen_text binary is absent, wubu_bytropix_score returns
 * "needs human" (passed=false, score=0) so the promotion gate stays CLOSED
 * rather than guessing. This is fail-closed, matching the DA-3 design.
 */
#include "wubu_verifier_bytropix.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <spawn.h>
#include <signal.h>

/* NOTE: the struct layout lives in wubu_verifier_bytropix.h (wubu_bytropix_verifier_t).
 * We do NOT redefine it here -- redefining it with a different body is UB and
 * corrupts the calloc/strncpy sizing (this is what caused the earlier segfault). */

wubu_bytropix_verifier_t *wubu_bytropix_verifier_create(const char *gen_text_path,
                                                          const char *model) {
    if (!gen_text_path || !model) return NULL;
    wubu_bytropix_verifier_t *v = calloc(1, sizeof(*v));
    if (!v) return NULL;
    v->mode = BYTROPIX_VIA_SUBPROCESS;
    strncpy(v->gen_text_path, gen_text_path, sizeof(v->gen_text_path) - 1);
    strncpy(v->model, model, sizeof(v->model) - 1);
    v->max_ctx = 4096;
    /* Default prompt: ask for a verdict + 0..1 quality score on the trace. */
    strncpy(v->prompt_template,
            "You are an independent verifier. Given this OS trace span, "
            "reply with 'PASS' or 'FAIL' then a 0..1 score. Span: ",
            sizeof(v->prompt_template) - 1);
    return v;
}

void wubu_bytropix_verifier_destroy(wubu_bytropix_verifier_t *v) { free(v); }

float wubu_bytropix_score(const wubu_trace_span_t *span, void *ud, bool *passed) {
    wubu_bytropix_verifier_t *v = (wubu_bytropix_verifier_t *)ud;
    if (passed) *passed = false;
    if (!v || !span) return 0.0f;

    /* Fail-closed: if the bytropix binary is not present, do NOT auto-promote.
     * The human/overseer gate stays the only path. */
    if (access(v->gen_text_path, X_OK) != 0) {
        if (passed) *passed = false;
        return 0.0f;
    }

    /* Build the prompt: template + span payload (grepable one-liner). */
    char prompt[1400];
    snprintf(prompt, sizeof(prompt), "%s%s", v->prompt_template, span->data);

    /* Invoke bytropix as a SEPARATE process (subprocess boundary = isolation
     * from the agent under test). Use posix_spawn + pipe for IPC — no popen,
     * no system, no fork (wubu dependency rule). Pipe the prompt in, read verdict.
     * Falls fail-closed if bytropix binary is unavailable. */
    int p_stdin[2];  /* parent -> child stdin */
    int p_stdout[2]; /* child -> parent stdout */
    if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0) {
        if (passed) *passed = false;
        return 0.0f;
    }

    char *argv[] = { v->gen_text_path, "--model", v->model,
                     "--max_ctx", "4096", NULL };
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, p_stdin[0],  STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&fa, p_stdout[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&fa, p_stdin[1]);
    posix_spawn_file_actions_addclose(&fa, p_stdout[0]);

    pid_t pid;
    int spawn_err = posix_spawn(&pid, v->gen_text_path, &fa, NULL, argv, NULL);
    posix_spawn_file_actions_destroy(&fa);
    close(p_stdin[0]);
    close(p_stdout[1]);

    if (spawn_err != 0) {
        close(p_stdin[1]); close(p_stdout[0]);
        if (passed) *passed = false;
        return 0.0f;
    }

    /* Write prompt to child stdin, then close. */
    size_t plen = strlen(prompt);
    ssize_t wn = 0;
    while ((size_t)wn < plen) {
        ssize_t w = write(p_stdin[1], prompt + wn, plen - (size_t)wn);
        if (w <= 0) break;
        wn += w;
    }
    close(p_stdin[1]);

    /* Read verdict from child stdout. */
    char out[1024] = {0};
    size_t total = 0;
    for (;;) {
        ssize_t r = read(p_stdout[0], out + total, sizeof(out) - 1 - total);
        if (r <= 0) break;
        total += (size_t)r;
    }
    out[total] = '\0';
    close(p_stdout[0]);

    /* Reap the child. */
    int status = 0;
    waitpid(pid, &status, 0);

    /* Parse: PASS -> promote; FAIL -> not. Score from a "0.xx" if present. */
    bool is_pass = (strstr(out, "PASS") != NULL) && (strstr(out, "FAIL") == NULL);
    float score = 0.5f;
    float s = 0.0f;
    if (sscanf(out, "%*[^0-9.]%f", &s) == 1 || sscanf(out, "%f", &s) == 1) score = s;
    if (passed) *passed = is_pass;
    return is_pass ? (score > 0.0f ? score : 0.9f) : 0.1f;
}
