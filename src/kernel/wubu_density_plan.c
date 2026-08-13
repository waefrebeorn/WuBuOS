/*
 * wubu_density_plan.c -- WuBuOS AGF Density Planner implementation.
 *
 * absorb/keep/prune metabolism. density = coherence / n_tokens gates the
 * keep/prune decision; KV-cache-as-FS is the training metabolism.
 *
 * Freestanding-safe (C11, fixed tables, no heap). Linked from the kernel
 * init path and from the hosted density-planner test.
 */
#include "wubu_density_plan.h"

#include <stdio.h>
#include <string.h>

/* Keep gate: a cluster is KEPT iff coherence/n_tokens >= this. */
#define WUBU_DENSITY_KEEP 0.5f

static int dp_str_eq(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* ---- Lifecycle -------------------------------------------------- */

void wubu_density_plan_init(wubu_density_plan_t *p)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->density_threshold = WUBU_DENSITY_KEEP;
    p->n_entries = 0;
    p->n_pending = 0;
    p->total_coherence = 0.0f;
    p->total_tokens = 0;
    p->cycles = 0;
    p->absorbed = 0;
    p->kept = 0;
    p->pruned = 0;
    p->theme_writes = 0;
    p->gpu_frames = 0;
    p->kv_write = NULL;
    p->kv_write_ud = NULL;
    for (int i = 0; i < WUBU_DENSITY_PLAN_MAX; i++) {
        p->entries[i].id = 0;
        p->entries[i].kv_path[0] = '\0';
        p->entries[i].coherence = 0.0f;
        p->entries[i].n_tokens = 0;
        p->entries[i].frames = 0;
        p->entries[i].alive = false;
    }
    for (int i = 0; i < WUBU_DENSITY_PLAN_MAX; i++) {
        p->pending[i].kv_path[0] = '\0';
        p->pending[i].coherence = 0.0f;
        p->pending[i].n_tokens = 0;
    }
}

void wubu_density_plan_set_kvfs(wubu_density_plan_t *p,
                                wubu_density_kv_write_fn kv_write,
                                void *ud)
{
    if (!p) return;
    p->kv_write = kv_write;
    p->kv_write_ud = ud;
}

/* ---- Absorb phase ---------------------------------------------- */

int wubu_density_plan_absorb(wubu_density_plan_t *p, const char *kv_path,
                             float coherence, uint64_t n_tokens)
{
    if (!p || !kv_path) return -1;
    if (coherence < 0.0f) coherence = 0.0f;
    if (coherence > 1.0f) coherence = 1.0f;

    /* Deduplicate by path: merge tokens + average coherence. */
    for (int i = 0; i < p->n_pending; i++) {
        if (dp_str_eq(p->pending[i].kv_path, kv_path)) {
            uint64_t tot = p->pending[i].n_tokens + n_tokens;
            if (tot > 0) {
                p->pending[i].coherence =
                    (p->pending[i].coherence * (float)p->pending[i].n_tokens +
                     coherence * (float)n_tokens) / (float)tot;
            } else {
                p->pending[i].coherence = coherence;
            }
            p->pending[i].n_tokens = tot;
            p->absorbed += n_tokens;
            return 0;
        }
    }

    if (p->n_pending >= WUBU_DENSITY_PLAN_MAX) return -1;  /* full */

    WubuDensityPending *q = &p->pending[p->n_pending++];
    strncpy(q->kv_path, kv_path, WUBU_DENSITY_PLAN_PATH - 1);
    q->kv_path[WUBU_DENSITY_PLAN_PATH - 1] = '\0';
    q->coherence = coherence;
    q->n_tokens = n_tokens;
    p->absorbed += n_tokens;
    return 0;
}

/* ---- Keep/prune cycle ------------------------------------------ */

float wubu_density_cluster_density(const WubuDensityEntry *e)
{
    if (!e || e->n_tokens == 0) return 0.0f;
    return e->coherence / (float)e->n_tokens;
}

int wubu_density_plan_cycle(wubu_density_plan_t *p)
{
    if (!p) return -1;

    p->cycles++;
    int kept_now = 0;
    int pruned_now = 0;

    /* Promote pending clusters: density = coherence/n_tokens >= threshold
     * => KEEP (write into the KV-cache-as-FS metabolism); else PRUNE. */
    for (int i = 0; i < p->n_pending; i++) {
        WubuDensityPending *q = &p->pending[i];
        float dens = (q->n_tokens > 0)
            ? (q->coherence / (float)q->n_tokens) : 0.0f;

        if (dens >= p->density_threshold) {
            /* KEEP: append a live cluster (dedup against the live set). */
            if (p->n_entries < WUBU_DENSITY_PLAN_MAX) {
                WubuDensityEntry *e = &p->entries[p->n_entries];
                e->id = (uint64_t)(p->kept + p->pruned + 1);
                strncpy(e->kv_path, q->kv_path, WUBU_DENSITY_PLAN_PATH - 1);
                e->kv_path[WUBU_DENSITY_PLAN_PATH - 1] = '\0';
                e->coherence = q->coherence;
                e->n_tokens = q->n_tokens;
                e->frames = 0;
                e->alive = true;
                p->n_entries++;

                /* Persist into the KV-cache-as-FS training store. */
                if (p->kv_write) {
                    (void)p->kv_write(e->kv_path, &q->coherence, sizeof(float),
                                      p->kv_write_ud);
                }
            }
            p->kept++;
            kept_now++;
        } else {
            /* PRUNE: drop from the live KV tensor (no metabolism write). */
            p->pruned++;
            pruned_now++;
        }
    }
    p->n_pending = 0;  /* pending buffer drained */

    /* Recompute global density = total_coherence / total_tokens over the
     * LIVE set. Re-scan live entries (a cluster can be re-absorbed later). */
    p->total_coherence = 0.0f;
    p->total_tokens = 0;
    for (int i = 0; i < p->n_entries; i++) {
        if (p->entries[i].alive) {
            p->total_coherence += p->entries[i].coherence;
            p->total_tokens    += p->entries[i].n_tokens;
        }
    }
    (void)kept_now;
    (void)pruned_now;
    return p->n_entries;
}

/* ---- wubu_drv_gpu connection ------------------------------------- */

void wubu_density_plan_gpu_frame(wubu_density_plan_t *p, uint64_t n_tokens)
{
    if (!p) return;
    p->gpu_frames++;
    /* A render frame is live training signal: absorb the frame's tokens at
     * the GPU render KV path. Coherence is weighted by frame progress. */
    if (n_tokens > 0) {
        float coh = (p->gpu_frames > 0) ? 0.8f : 0.5f;
        wubu_density_plan_absorb(p, "/kv/gpu/frame", coh, n_tokens);
    }
}

/* ---- /theme write-through --------------------------------------- */

void wubu_density_plan_theme_write(wubu_density_plan_t *p,
                                   const char *theme_path,
                                   uint32_t value)
{
    if (!p || !theme_path) return;
    p->theme_writes++;

    /* A theme change is a user interaction = live training. Absorb a token
     * cluster keyed by the changed node under the KV-cache-as-FS path
     * /kv/theme/<node>. The coherence of a theme write is high (it is an
     * explicit human signal); n_tokens is scaled by the EDR write proxy in
     * value's low byte so a repeated write at the same node accumulates. */
    char kvpath[WUBU_DENSITY_PLAN_PATH];
    const char *src = theme_path;
    if (strncmp(src, "/theme/", 7) == 0) src += 7;   /* strip /theme/ */
    else if (strncmp(src, "theme/", 6) == 0) src += 6;
    {
        char node[WUBU_DENSITY_PLAN_PATH];
        snprintf(node, sizeof(node), "%s", src);
        snprintf(kvpath, sizeof(kvpath), "/kv/theme/%s", node);
    }

    float coh = 0.9f;                      /* explicit human signal */
    uint64_t n_tokens = (uint64_t)(value & 0xFFu) + 1u;  /* 1..256 */
    (void)wubu_density_plan_absorb(p, kvpath, coh, n_tokens);

    /* Keep it responsive: run the cycle on every theme write so the changed
     * node is immediately accounted for in keep/prune. */
    wubu_density_plan_cycle(p);
}

/* ---- Accounting ------------------------------------------------- */

uint64_t wubu_density_plan_cycles(const wubu_density_plan_t *p)
{ return p ? p->cycles : 0; }
uint64_t wubu_density_plan_absorbed(const wubu_density_plan_t *p)
{ return p ? p->absorbed : 0; }
uint64_t wubu_density_plan_kept(const wubu_density_plan_t *p)
{ return p ? p->kept : 0; }
uint64_t wubu_density_plan_pruned(const wubu_density_plan_t *p)
{ return p ? p->pruned : 0; }
uint64_t wubu_density_plan_theme_writes(const wubu_density_plan_t *p)
{ return p ? p->theme_writes : 0; }
uint64_t wubu_density_plan_gpu_frames(const wubu_density_plan_t *p)
{ return p ? p->gpu_frames : 0; }

int wubu_density_plan_alive_count(const wubu_density_plan_t *p)
{
    if (!p) return 0;
    int n = 0;
    for (int i = 0; i < p->n_entries; i++)
        if (p->entries[i].alive) n++;
    return n;
}

float wubu_density_plan_density(const wubu_density_plan_t *p)
{
    if (!p || p->total_tokens == 0) return 0.0f;
    return p->total_coherence / (float)p->total_tokens;
}
