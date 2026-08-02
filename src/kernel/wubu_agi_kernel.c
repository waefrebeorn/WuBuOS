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
#include "wubu_attest.h"
#include "wubu_bonzi.h"
#include "wubu_console.h"
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

    /* Firmware root of trust (WuBuFW attestation, consumed at init). */
    bool             attest_valid;
    uint32_t         kernel_size;
    uint8_t          kernel_digest[WUBU_AGI_PCR_SZ];

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

    /* Firmware root of trust: consume the attestation snapshot the WuBuFW
     * loader handed over (metal_main calls wubu_attest_load_scratch() first).
     * If the firmware's measurement chain is not live, the supervisor will
     * REFUSE to promote any self-improvement change (see cycle()). */
    g_agi.attest_valid = wubu_attest_valid();
    g_agi.kernel_size  = wubu_attest_kernel_size();
    wubu_attest_kernel_digest(g_agi.kernel_digest);

    /* Record the boot's root-of-trust state as an immutable trace span
     * (code-as-data: the supervisor's own start is observable). */
    {
        static const char hexd[] = "0123456789ABCDEF";
        char sbuf[WUBU_AGI_SPAN_DATA];
        if (g_agi.attest_valid) {
            uint8_t p4[WUBU_AGI_PCR_SZ];
            char p4hex[2 * WUBU_AGI_PCR_SZ + 1];
            char kh[2 * WUBU_AGI_PCR_SZ + 1];
            char *o = p4hex;
            if (wubu_attest_pcr4_digest(p4) == 0) {
                for (int i = 0; i < WUBU_AGI_PCR_SZ; i++) {
                    *o++ = hexd[p4[i] >> 4]; *o++ = hexd[p4[i] & 15];
                }
            } else {
                *o++ = '?';
            }
            *o = 0;
            o = kh;
            for (int i = 0; i < WUBU_AGI_PCR_SZ; i++) {
                *o++ = hexd[g_agi.kernel_digest[i] >> 4];
                *o++ = hexd[g_agi.kernel_digest[i] & 15];
            }
            *o = 0;
            snprintf(sbuf, sizeof(sbuf),
                     "attest: valid sb=%u setup=%u boot=%u pcr4=%s kern=%s sz=%u",
                     wubu_attest_sb_enabled() ? 1u : 0u,
                     wubu_attest_setup_mode() ? 1u : 0u,
                     wubu_attest_boot_counter(), p4hex, kh, g_agi.kernel_size);
        } else {
            snprintf(sbuf, sizeof(sbuf),
                     "attest: ABSENT -- self-improve promotion disabled");
        }
        agi_ring_push(&g_agi, WUBU_AGI_SPAN_SUPER, 0, sbuf);
    }

    if (klog_printf) {
        klog_printf("WuBuOS AGI: GAAD viewport %dx%d -> %d regions\n",
                    g_agi.fb_w, g_agi.fb_h, g_agi.decomp.n_regions);
        klog_printf("WuBuOS AGI: firmware attestation %s\n",
                    g_agi.attest_valid ? "VALID (root of trust live)" :
                                         "ABSENT (promotion disabled)");
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
    /* Spawn the Bonzi Buddy human interface task: draws the gorilla +
     * console on the framebuffer, polls the PS/2 keyboard, dispatches real
     * ring-0 actions, and feeds every human interaction into the trace. */
    if (task_create("bonzi", wubu_bonzi_task, k, 128 * 1024, PRIO_NORMAL)) {
        if (klog_printf)
            klog_printf("WuBuOS AGI: Bonzi Buddy task spawned\n");
    }
    /* Spawn the live console task: COM1 REPL -- the TempleOS-style live
     * development surface of the OS (commands + HolyC in ring 0). */
    if (task_create("console", wubu_console_task, k, 64 * 1024, PRIO_NORMAL)) {
        if (klog_printf)
            klog_printf("WuBuOS AGI: live console task spawned\n");
    }
    /* The cooperative loop is driven by the PIT timer (wubu_agi_kernel_tick).
     * On bare metal we yield to the scheduler; the timer interrupt ticks us.
     * We do NOT busy-HLT -- the agent task + supervisor run cooperatively. */
    for (;;) {
        task_yield();
    }
}

static void agi_theme_step(wubu_agi_kernel_t *k);

void wubu_agi_kernel_tick(wubu_agi_kernel_t *k)
{
    if (!k) return;
    k->uptime_ms += 10;   /* PIT ticks at ~100 Hz -> 10 ms */
    /* Run a bounded self-improve cycle step each tick. */
    wubu_agi_kernel_cycle(k);
    /* The Colonel re-skins the desktop from its own state: the /theme
     * namespace is AGI-writable, so health + mood show up live. */
    agi_theme_step(k);
}

/* AGI-writable graphic set: derive theme nodes from the supervisor state.
 * Writes only on CHANGE (the EDR write counter = real changes). The
 * desktop is the Colonel's vitals: attestation failure turns the title
 * bar red; growth (promotions) warms the gorilla's fur toward gold;
 * frozen mutes the desktop. */

/* 0..100 RGB lerp helper (no float, integer math). */
static uint32_t lerp_rgb(uint32_t a, uint32_t b, int t)
{
    int r = (int)((a >> 16) & 0xFF) + (((int)((b >> 16) & 0xFF) -
              (int)((a >> 16) & 0xFF)) * t) / 100;
    int g = (int)((a >> 8) & 0xFF) + (((int)((b >> 8) & 0xFF) -
              (int)((a >> 8) & 0xFF)) * t) / 100;
    int bl = (int)(a & 0xFF) + (((int)(b & 0xFF) - (int)(a & 0xFF)) * t) / 100;
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (bl < 0) bl = 0; else if (bl > 255) bl = 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

static void agi_theme_step(wubu_agi_kernel_t *k)
{
    extern int  wubu_theme_node_set(const char *, uint32_t);
    extern void wubu_theme_apply(void);
    if (!k) return;

    /* title: attestation is the root of trust */
    uint32_t title = k->attest_valid ? 0x00006040u : 0x00A00000u;
    uint32_t cur = 0;
    extern int wubu_theme_node_get(const char *, uint32_t *);
    if (wubu_theme_node_get("/theme/win/title_active", &cur) != 0 ||
        cur != title) {
        wubu_theme_node_set("/theme/win/title_active", title);
    }

    /* mood -> gorilla fur: green (calm) -> gold (growing) */
    int mood = (int)k->promoted_total;
    if (mood > 20) mood = 20;                 /* cap the growth ramp */
    if (!k->attest_valid) mood = 0;           /* no trust, no warmth */
    uint32_t fur = lerp_rgb(0x00806040u, 0x00E0A000u, mood * 5);
    if (wubu_theme_node_get("/theme/gorilla/fur", &cur) != 0 ||
        cur != fur) {
        wubu_theme_node_set("/theme/gorilla/fur", fur);
    }

    /* frozen -> muted desktop */
    uint32_t bg = k->frozen ? 0x00404040u : 0x00003020u;
    if (wubu_theme_node_get("/theme/desktop/bg", &cur) != 0 || cur != bg) {
        wubu_theme_node_set("/theme/desktop/bg", bg);
    }

    wubu_theme_apply();
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
    /* DA-3 safe default + firmware root-of-trust gate: no promotion without
     * (a) a running loop, (b) an INDEPENDENT verifier, (c) a LIVE firmware
     * attestation. A self-modification chain is only trusted while the
     * WuBuFW measurement chain (PCR0-7 + AuthentiCode) is present. */
    if (!k || k->frozen || !k->verifier || !k->attest_valid) return 0;

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
bool     wubu_agi_kernel_attest_valid(const wubu_agi_kernel_t *k)
           { return k ? k->attest_valid : false; }

/* Copy the data of the idx-th trace span (oldest-first) into out.
 * Returns 0 on success, -1 if out of range. */
int wubu_agi_kernel_span_data(const wubu_agi_kernel_t *k, int idx,
                              char *out, size_t outsz)
{
    if (!k || !out || outsz == 0) return -1;
    if (idx < 0 || idx >= k->ring_count) return -1;
    int slot = (k->ring_head - k->ring_count + idx);
    if (slot < 0) slot += WUBU_AGI_TRACE_CAP;
    size_t n = strlen(k->ring[slot].data);
    if (n >= outsz) n = outsz - 1;
    memcpy(out, k->ring[slot].data, n);
    out[n] = '\0';
    return 0;
}

wubu_agi_kernel_t *wubu_agi_kernel_global(void) { return &g_agi; }
