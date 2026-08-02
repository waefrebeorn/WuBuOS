/*
 * wubu_agi_kernel.c -- WuBuOS Bare-Metal AGI Kernel Supervisor (ring-0).
 *
 * Freestanding C11: NO malloc, NO pthreads, NO hosted APIs. Runs ON METAL.
 * See wubu_agi_kernel.h for the design (converges realm + trace + selfimprove
 * + gaad into a single ring-0 cooperative supervisor).
 *
 * The agent realm is an in-process kernel `tasking` thread (REALM_AGENT
 * personality, co-resident -- metal has no KVM for a microVM). The supervisor
 * is single-CPU cooperative: the PIT timer calls wubu_agi_kernel_tick().
 */
#include "wubu_agi_kernel.h"
#include "wubu_gaad.h"
#include "tasking.h"
#include "klog.h"
#include "vbe.h"

#include <string.h>

/* ==================================================================
 * Static state (no heap)
 * ================================================================= */

struct wubu_agi_kernel {
    /* GAAD viewport decomposition (static, computed once at init). */
    WubuGaadDecomp   decomp;
    int              fb_w, fb_h;

    /* Append-only trace ring (no malloc). */
    wubu_agi_span_t  ring[WUBU_AGI_TRACE_CAP];
    int              ring_head;      /* next write slot */
    int              ring_count;     /* occupied slots */
    uint64_t         span_id;        /* monotonic id */

    /* Self-improve bookkeeping. */
    wubu_agi_verifier_fn verifier;
    void            *verifier_ud;
    bool             frozen;
    int              promoted_total;

    /* Uptime (ms), advanced by tick(). */
    uint64_t         uptime_ms;

    /* Agent realm task handle (co-resident tasking thread). */
    CTask           *agent_task;
    bool             agent_alive;
};

/* One fixed instance (kernel is single-instance). */
static wubu_agi_kernel_t g_agi;

/* ==================================================================
 * Trace ring (append-only, immutable)
 * ================================================================= */

static int agi_ring_push(wubu_agi_kernel_t *k, wubu_agi_span_kind_t kind,
                          uint64_t parent, const char *payload)
{
    if (!k) return -1;
    wubu_agi_span_t *s = &k->ring[k->ring_head];
    memset(s, 0, sizeof(*s));
    s->id       = ++k->span_id;
    s->parent   = parent;
    s->ts_ms    = k->uptime_ms;
    s->kind     = kind;
    s->consumed = false;
    if (payload) {
        size_t n = strlen(payload);
        if (n >= WUBU_AGI_SPAN_DATA) n = WUBU_AGI_SPAN_DATA - 1;
        memcpy(s->data, payload, n);
        s->data[n] = '\0';
    }
    k->ring_head = (k->ring_head + 1) % WUBU_AGI_TRACE_CAP;
    if (k->ring_count < WUBU_AGI_TRACE_CAP) k->ring_count++;
    else {
        /* Ring full: oldest was overwritten; count stays capped.
         * The overwritten span is genuinely lost (immutable eviction). */
    }
    return 0;
}

/* ==================================================================
 * Agent realm task (REALM_AGENT, in-process)
 *
 * The agent emits AGENT trace spans on a cadence. This is a real, running
 * ring-0 task -- not a stub. On bare metal it has no network/LLM; it runs a
 * deterministic local policy (the "cog" loop): observe GAAD viewport, emit a
 * step span, yield. The supervisor scores these via the independent verifier.
 * ================================================================= */

static void agi_agent_task(void *arg)
{
    wubu_agi_kernel_t *k = (wubu_agi_kernel_t *)arg;
    if (!k) return;
    k->agent_alive = true;

    uint64_t step = 0;
    char buf[WUBU_AGI_SPAN_DATA];
    while (k->agent_alive) {
        /* Local policy step: decompose-aware action on the φ viewport.
         * Real work: report the current GAAD region count as the agent's
         * observable state. The supervisor learns from these spans. */
        int regions = k->decomp.n_regions;
        snprintf(buf, sizeof(buf),
                 "agent.step=%llu regions=%d viewport=%dx%d",
                 (unsigned long long)step, regions, k->fb_w, k->fb_h);
        wubu_agi_kernel_agent_emit(k, 0, buf);

        /* Emit a periodic self-mod proposal (gated event). The supervisor
         * only promotes it if the independent verifier signs off. */
        if ((step % 8) == 7) {
            snprintf(buf, sizeof(buf),
                     "agent.propose: tune agent cadence to regions=%d",
                     regions);
            agi_ring_push(k, WUBU_AGI_SPAN_SELFMOD, k->span_id, buf);
        }

        step++;
        task_yield();   /* cooperative: let the supervisor tick */
    }
}

/* ==================================================================
 * Public API
 * ================================================================= */

wubu_agi_kernel_t *wubu_agi_kernel_init(int fb_w, int fb_h)
{
    memset(&g_agi, 0, sizeof(g_agi));
    g_agi.fb_w = fb_w > 0 ? fb_w : 640;
    g_agi.fb_h = fb_h > 0 ? fb_h : 480;

    /* Real GAAD work: decompose the framebuffer into golden-ratio regions.
     * This is the resolution-independent viewport the agent operates on. */
    wubu_gaad_decompose(g_agi.fb_w, g_agi.fb_h, WUBU_GAAD_MAX_DEPTH,
                        &g_agi.decomp);
    wubu_gaad_add_spirals(&g_agi.decomp, 4, 8);   /* phi-spiral sector points */

    /* Default: no verifier => safe (refuses to promote). */
    g_agi.verifier  = NULL;
    g_agi.frozen    = false;
    g_agi.ring_head = 0;
    g_agi.ring_count = 0;
    g_agi.span_id   = 0;
    g_agi.promoted_total = 0;
    g_agi.uptime_ms = 0;
    g_agi.agent_alive = false;

    if (klog_printf) {
        klog_printf("WuBuOS AGI: GAAD viewport %dx%d -> %d regions\n",
                    g_agi.fb_w, g_agi.fb_h, g_agi.decomp.n_regions);
    }
    return &g_agi;
}

void wubu_agi_kernel_run(wubu_agi_kernel_t *k)
{
    if (!k) return;
    /* Spawn the agent realm as a co-resident kernel task (REALM_AGENT). */
    k->agent_task = task_create("agi-agent", agi_agent_task, k,
                                 256 * 1024, PRIO_NORMAL);
    if (klog_printf)
        klog_printf("WuBuOS AGI: agent realm task spawned (%s)\n",
                    k->agent_task ? "ok" : "FAIL");
    /* The cooperative loop is driven by the PIT timer (wubu_agi_kernel_tick).
     * On bare metal we yield to the scheduler; the timer interrupt ticks us.
     * We do NOT busy-HLT -- the agent task + supervisor run cooperatively. */
    for (;;) {
        task_yield();
    }
}

void wubu_agi_kernel_tick(wubu_agi_kernel_t *k)
{
    if (!k) return;
    k->uptime_ms += 10;   /* PIT ticks at ~100 Hz -> 10 ms */
    /* Run a bounded self-improve cycle step each tick. */
    wubu_agi_kernel_cycle(k);
}

void wubu_agi_kernel_freeze(wubu_agi_kernel_t *k, bool frozen)
{
    if (k) k->frozen = frozen;
}

bool wubu_agi_kernel_is_frozen(const wubu_agi_kernel_t *k)
{
    return k ? k->frozen : true;
}

void wubu_agi_kernel_set_verifier(wubu_agi_kernel_t *k,
                                  wubu_agi_verifier_fn fn, void *ud)
{
    if (!k) return;
    k->verifier = fn;
    k->verifier_ud = ud;
}

int wubu_agi_kernel_agent_emit(wubu_agi_kernel_t *k, uint64_t parent,
                               const char *payload)
{
    return agi_ring_push(k, WUBU_AGI_SPAN_AGENT, parent, payload);
}

int wubu_agi_kernel_cycle(wubu_agi_kernel_t *k)
{
    if (!k || k->frozen || !k->verifier) return 0;  /* DA-3 safe default */

    int promoted = 0;
    /* Scan the ring for unconsumed spans; score each via the INDEPENDENT
     * verifier. Promoted changes are recorded (gated). Oldest-first. */
    for (int i = 0; i < k->ring_count; i++) {
        /* oldest slot is at (head - count + i) mod CAP */
        int idx = (k->ring_head - k->ring_count + i);
        if (idx < 0) idx += WUBU_AGI_TRACE_CAP;
        wubu_agi_span_t *s = &k->ring[idx];
        if (s->consumed) continue;

        bool passed = false;
        float score = k->verifier(s->data, s->ts_ms, k->verifier_ud, &passed);
        (void)score;  /* score kept for future telemetry; gate is `passed` */
        s->consumed = true;
        if (passed) {
            promoted++;
            k->promoted_total++;
            if (klog_printf)
                klog_printf("WuBuOS AGI: PROMOTED span %llu (%s)\n",
                            (unsigned long long)s->id, s->data);
        }
    }
    return promoted;
}

/* ---- Inspection ---- */
int      wubu_agi_kernel_trace_count(const wubu_agi_kernel_t *k)
           { return k ? k->ring_count : 0; }
int      wubu_agi_kernel_promoted_total(const wubu_agi_kernel_t *k)
           { return k ? k->promoted_total : 0; }
int      wubu_agi_kernel_region_count(const wubu_agi_kernel_t *k)
           { return k ? k->decomp.n_regions : 0; }
uint64_t wubu_agi_kernel_uptime_ms(const wubu_agi_kernel_t *k)
           { return k ? k->uptime_ms : 0; }

wubu_agi_kernel_t *wubu_agi_kernel_global(void) { return &g_agi; }
