/*
 * wubu_verifier_bytropix.h -- integration contract: bytropix as the INDEPENDENT
 * verifier / agent model for the Mega-OS self-improvement loop (Phase C, DA-3).
 *
 * GROUNDED REALITY CHECK (2026-07-25):
 *   bytropix (github.com/waefrebeorn/bytropix) is a from-scratch C/CUDA multi-model
 *   inference engine (DiffusionGemma-26B / Gemma 4 12B QAT / Qwen3.6-35B, MIT,
 *   ~35k LOC, fully local on WSL2/RTX 5050, NO framework deps). It is NOT an OS
 *   kernel. It is the on-device model backend that plays TWO roles in our loop:
 *     1. the AGENT brain (the thing being improved), and
 *     2. the INDEPENDENT VERIFIER (a DIFFERENT model/run than the agent, so it
 *        cannot grade its own work -- answers DA-3 "verifier != generator").
 *   Because it runs locally with no network, it also satisfies DA-1 (no trace
 *   exfiltration) and DA-2 (no external dependency in the observation path).
 *
 * This header defines the CONTRACT only. The operator layer (wubu_selfimprove.c)
 * already accepts any `wubu_verifier_fn`; this file shows how to bind bytropix to
 * it. The actual subprocess/model-handle wiring is filled in when bytropix's
 * `gen_text` binary (or a linked `libwubu_model`) is available on the host.
 *
 * Opaque, C11, minimal includes.
 */
#ifndef WUBU_VERIFIER_BYTROPIX_H
#define WUBU_VERIFIER_BYTROPIX_H

#include "wubu_selfimprove.h"   /* wubu_verifier_fn, wubu_trace_span_t */

/* How to invoke bytropix. Keep it a subprocess boundary so the verifier is a
 * SEPARATE process/model from the agent (defence-in-depth for DA-3). */
typedef enum {
    BYTROPIX_VIA_SUBPROCESS = 0,  /* spawn bytropix gen_text (default; clean isolation) */
    BYTROPIX_VIA_LIBHANDLE      /* link libwubu_model and call forward directly */
} wubu_bytropix_mode_t;

typedef struct {
    wubu_bytropix_mode_t mode;
    char gen_text_path[512];    /* path to bytropix gen_text binary */
    char model[64];             /* "qwen3.6-35b" | "gemma4-12b" | "diffusiongemma-26b" */
    int  max_ctx;
    /* Verifier prompt template: fed the span payload, must return
     * "PASS" / "FAIL" + a 0..1 score. Kept inline so it is auditable. */
    char prompt_template[1024];
} wubu_bytropix_verifier_t;

/* Build a verifier bound to bytropix. Returns an object whose
 * `score_span()` maps onto `wubu_verifier_fn` for wubu_selfimprove_set_verifier().
 * Returns NULL on bad config. The caller wires it:
 *     wubu_selfimprove_set_verifier(si, wubu_bytropix_score, cfg); */
wubu_bytropix_verifier_t *wubu_bytropix_verifier_create(const char *gen_text_path,
                                                          const char *model);
void wubu_bytropix_verifier_destroy(wubu_bytropix_verifier_t *v);

/* The wubu_verifier_fn entry point: scores ONE span using bytropix as the
 * independent verifier. `ud` must be the wubu_bytropix_verifier_t*. Sets *passed
 * (true = promote). Never the agent under test (separate model/process). */
float wubu_bytropix_score(const wubu_trace_span_t *span, void *ud, bool *passed);

#endif /* WUBU_VERIFIER_BYTROPIX_H */
