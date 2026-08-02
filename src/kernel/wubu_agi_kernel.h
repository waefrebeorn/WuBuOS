/*
 * wubu_agi_kernel.h -- WuBuOS Bare-Metal AGI Kernel Supervisor (ring-0).
 *
 * This is the "AGI kernel" that actually runs ON METAL (not just the hosted
 * binary). It is the convergence of three existing WuBuOS concepts, ported to
 * freestanding C11 (NO malloc, NO pthreads, NO hosted APIs):
 *
 *   1. wubu_realm  (REALM_AGENT personality)  -> an in-process agent task
 *   2. wubu_trace  (append-only immutable span store) -> static ring buffer
 *   3. wubu_selfimprove (independent-verifier promotion gate, DA-3) -> tick loop
 *   4. wubu_gaad   (golden-ratio viewport decomposition) -> static decomp
 *
 * Why freestanding: the bare-metal kernel has no libc heap or pthread. The
 * supervisor is single-CPU cooperative: the PIT timer ticks it, and the agent
 * realm is a kernel `tasking` thread. No microVM (metal has no KVM).
 *
 * DA-3 (alignment): promotion requires an INDEPENDENT verifier. Same-agent
 * grading is a rubber stamp. Default verifier REJECTS (safe); the operator
 * wires a real overseer. The loop is freezable (user switch).
 *
 * Opaque struct, C11, minimal includes.
 */
#ifndef WUBU_AGI_KERNEL_H
#define WUBU_AGI_KERNEL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Trace capacity (static ring, no heap). */
#define WUBU_AGI_TRACE_CAP    256
#define WUBU_AGI_SPAN_DATA    192   /* payload bytes per span */

/* Independent verifier signature (DA-3). Must NOT be the agent under test.
 * Returns score in [0,1] (1 = good) and sets *passed (true = promote). */
typedef float (*wubu_agi_verifier_fn)(const char *payload, uint64_t ts_ms,
                                      void *ud, bool *passed);

/* Gap G5: the AGI's long-term-memory hook (metal: the hive). Invoked
 * with op "put" on every promotion (the span's payload + id) so the
 * promoted knowledge is retained beyond the boot. Returns 0 on success. */
typedef int (*wubu_agi_memory_fn)(const char *op, uint64_t span_id,
                                  const char *payload, void *ud);

typedef enum {
    WUBU_AGI_SPAN_AGENT    = 0,   /* agent reasoning/tool-call step */
    WUBU_AGI_SPAN_SELFMOD  = 1,   /* a self-improvement change (gated) */
    WUBU_AGI_SPAN_SUPER    = 2,   /* supervisor/operator event */
    WUBU_AGI_SPAN_COUNT
} wubu_agi_span_kind_t;

/* A single immutable span. Written once, never rewritten by the agent. */
typedef struct {
    uint64_t id;
    uint64_t parent;
    uint64_t ts_ms;
    wubu_agi_span_kind_t kind;
    bool     consumed;     /* already scored by a self-improve cycle */
    char     data[WUBU_AGI_SPAN_DATA];
} wubu_agi_span_t;

typedef struct wubu_agi_kernel wubu_agi_kernel_t;

/* ---- Lifecycle (called from metal_main) ----------------------------- */

/* Initialize the supervisor. Decomposes the framebuffer via GAAD (static
 * WubuGaadDecomp), sets up the static trace ring, spawns the agent realm
 * task. fb_w/fb_h are the Limine framebuffer dimensions. */
wubu_agi_kernel_t *wubu_agi_kernel_init(int fb_w, int fb_h);

/* Cooperative run loop. Spawns the agent task and enters the supervisor tick
 * loop. Driven by the PIT timer (wubu_agi_kernel_tick). Does NOT return until
 * the user freezes the loop via wubu_agi_kernel_freeze(). On bare metal this
 * replaces the old `for(;;) HLT();` shell. */
void wubu_agi_kernel_run(wubu_agi_kernel_t *k);

/* PIT timer tick: ingest one queued agent span, run a bounded self-improve
 * cycle step, emit a supervisor heartbeat. Cooperative (no blocking). */
void wubu_agi_kernel_tick(wubu_agi_kernel_t *k);

/* Freeze/unfreeze the self-improve loop (user switch). Frozen => cycle() does
 * nothing, agent keeps emitting but changes are never promoted. */
void wubu_agi_kernel_freeze(wubu_agi_kernel_t *k, bool frozen);
bool wubu_agi_kernel_is_frozen(const wubu_agi_kernel_t *k);

/* Wire the independent verifier (DA-3). NULL => loop refuses to promote. */
void wubu_agi_kernel_set_verifier(wubu_agi_kernel_t *k,
                                  wubu_agi_verifier_fn fn, void *ud);

/* Gap G5: wire the long-term-memory hook. */
void wubu_agi_kernel_set_memory(wubu_agi_kernel_t *k,
                                wubu_agi_memory_fn fn, void *ud);

/* Gap G2: the self-test suite needs to know whether the memory hook is
 * armed (the long-term hive survived the boot). */
int wubu_agi_kernel_has_memory(const wubu_agi_kernel_t *k);

/* ---- Agent realm API (REALM_AGENT, in-process tasking thread) -------- */

/* Emit an AGENT trace span from the agent realm. Append-only, immutable.
 * Returns 0 on success, -1 on ring overflow (oldest dropped). */
int wubu_agi_kernel_agent_emit(wubu_agi_kernel_t *k, uint64_t parent,
                               const char *payload);

/* Run one self-improve cycle. For each unconsumed AGENT/SELFMOD span it scores
 * via the INDEPENDENT verifier; promoted changes are recorded (gated). Returns
 * #promoted this cycle. If frozen or no verifier, returns 0 and changes
 * nothing. This is the port of wubu_selfimprove_cycle to freestanding C11. */
int wubu_agi_kernel_cycle(wubu_agi_kernel_t *k);

/* Returns the singleton kernel instance (set by wubu_agi_kernel_init).
 * Used by the PIT timer handler to tick the supervisor without a global in
 * every translation unit. NULL before init. */
wubu_agi_kernel_t *wubu_agi_kernel_global(void);
int    wubu_agi_kernel_trace_count(const wubu_agi_kernel_t *k);
int    wubu_agi_kernel_promoted_total(const wubu_agi_kernel_t *k);
int    wubu_agi_kernel_region_count(const wubu_agi_kernel_t *k);
uint64_t wubu_agi_kernel_uptime_ms(const wubu_agi_kernel_t *k);

/* Gap D7: the tick of the last successful promotion (the supervisor
 * watchdog's heartbeat -- the bonzi alerts when this goes stale). */
uint64_t wubu_agi_kernel_last_promote_tick(const wubu_agi_kernel_t *k);

/* Firmware root-of-trust state captured at init (wubu_attest consumed). */
bool   wubu_agi_kernel_attest_valid(const wubu_agi_kernel_t *k);

/* Copy the data of the idx-th trace span (oldest-first) into out.
 * Returns 0 on success, -1 if out of range. */
int    wubu_agi_kernel_span_data(const wubu_agi_kernel_t *k, int idx,
                                 char *out, size_t outsz);

#endif /* WUBU_AGI_KERNEL_H */
