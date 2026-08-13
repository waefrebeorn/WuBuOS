/*
 * wubu_density_plan.h -- WuBuOS AGF (Adaptive Gradient Feedback) Density
 * Planner.
 *
 * The density planner is the AGI/AGF training metabolism. It runs an
 * absorb/keep/prune cycle over "token clusters" drawn from the world-state:
 *
 *   - ABSORB  : a token cluster (coherence score + n_tokens) is recorded.
 *               User interactions (a /theme write) and GPU render frames feed
 *               the metabolism -- "user interactions = live training".
 *
 *   - KEEP / PRUNE : density = coherence / n_tokens drives the decision.
 *               Clusters whose density rises above the planner's threshold are
 *               KEPT (written into the KV-cache-as-FS training store); clusters
 *               that fall below are PRUNED (evicted from the live KV tensor).
 *
 * KV-cache-as-FS IS the training metabolism: the KV namespace (wubu_kvfs) is
 * both the allocator's working store and the 9P surface the AGI reads /n/kv/*
 * over. The planner is linked to the wubu_drv_gpu path because a theme change
 * (a user interaction) re-renders the desktop through the GPU driver, emitting
 * render frames that feed absorb().
 *
 * Freestanding-safe (C11, opaque struct, no heap of its own). A hosted test
 * links the planner with the real wubu_theme node set or with a stub metabolism.
 */
#ifndef WUBU_DENSITY_PLAN_H
#define WUBU_DENSITY_PLAN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Max live token clusters the planner tracks. Fixed table (no malloc). */
#define WUBU_DENSITY_PLAN_MAX 64

/* Max length of a KV-cache path stored per entry. */
#define WUBU_DENSITY_PLAN_PATH 48

typedef struct {
    uint64_t id;               /* monotonic cluster id */
    char     kv_path[WUBU_DENSITY_PLAN_PATH];  /* KV-cache-as-FS path */
    float    coherence;        /* [0.0, 1.0] coherence of the cluster */
    uint64_t n_tokens;         /* tokens absorbed at this cluster */
    uint64_t frames;           /* GPU render frames referencing it */
    bool     alive;            /* kept (true) vs pruned (false) */
} WubuDensityEntry;

/* A cluster queued for the next absorb/keep/prune cycle. The planner runs the
 * absorb phase inline (cheap, O(1) append) and defers the heavier keep/prune
 * sweep to cycle(), so theme writes are responsive. */
typedef struct {
    char     kv_path[WUBU_DENSITY_PLAN_PATH];
    float    coherence;
    uint64_t n_tokens;
} WubuDensityPending;

/* KV-cache-as-FS metabolism hook: persist a cluster's tokens into the live
 * KV tensor (the 9P training store). Returns 0 on success. The kernel wires
 * this to wubu_kvfs_route_write at init; a hosted test may supply a stub. */
typedef int (*wubu_density_kv_write_fn)(const char *kv_path,
                                        const void *data, size_t len,
                                        void *ud);

struct wubu_density_plan;
typedef struct wubu_density_plan wubu_density_plan_t;

struct wubu_density_plan {
    WubuDensityEntry  entries[WUBU_DENSITY_PLAN_MAX];
    int               n_entries;    /* live (alive) clusters */

    WubuDensityPending pending[WUBU_DENSITY_PLAN_MAX];
    int               n_pending;    /* clusters queued for the next cycle */

    float             total_coherence;  /* sum of alive clusters' coherence */
    uint64_t          total_tokens;     /* sum of alive clusters' n_tokens */

    /* Accounting (the cycle's absorb/keep/prune bookkeeping the test audits). */
    uint64_t          cycles;        /* absorb/keep/prune cycles run */
    uint64_t          absorbed;      /* tokens absorbed this boot */
    uint64_t          kept;          /* clusters kept (promoted to alive) */
    uint64_t          pruned;        /* clusters pruned (evicted) */
    uint64_t          theme_writes;  /* /theme write-throughs observed */

    float             density_threshold;  /* density < this -> prune */

    /* KV-cache-as-FS metabolism (the training store). NULL = in-memory only. */
    wubu_density_kv_write_fn kv_write;
    void                      *kv_write_ud;

    /* wubu_drv_gpu connection: cumulative render frames fed in. */
    uint64_t          gpu_frames;
};

/* ---- Lifecycle -------------------------------------------------- */

/* Initialise a planner. Density threshold defaults to the keep/prune gate
 * (clusters with coherence/n_tokens below WUBU_DENSITY_KEEP below this are
 * pruned). */
void wubu_density_plan_init(wubu_density_plan_t *p);

/* Wire the planner to the KV-cache-as-FS training metabolism. Called by the
 * kernel after wubu_kvfs_kernel_init(); the hook persists a cluster's tokens
 * into the live KV tensor via wubu_kvfs_route_write. */
void wubu_density_plan_set_kvfs(wubu_density_plan_t *p,
                                wubu_density_kv_write_fn kv_write,
                                void *ud);

/* ---- The absorb/keep/prune metabolism -------------------------- */

/* ABSORB: record a token cluster from a user interaction (a /theme write) or
 * a GPU render frame. Appends to the pending buffer (deduped by kv_path,
 * merging n_tokens). Returns 0 on success, -1 if full. */
int wubu_density_plan_absorb(wubu_density_plan_t *p, const char *kv_path,
                             float coherence, uint64_t n_tokens);

/* KEEP/PRUNE (the cycle): promote pending clusters to the live set when their
 * density = coherence/n_tokens >= threshold (KEPT, written into the KV-cache
 * metabolism); evict clusters whose density falls below (PRUNED). Recomputes
 * the global density over the live set. Returns the net live cluster count. */
int wubu_density_plan_cycle(wubu_density_plan_t *p);

/* ---- wubu_drv_gpu connection ------------------------------------- */

/* A render frame completed through the GPU driver. Feeds the metabolism:
 * each frame on a theme-driven re-render injects tokens so the planner can
 * re-score coherence. */
void wubu_density_plan_gpu_frame(wubu_density_plan_t *p, uint64_t n_tokens);

/* ---- /theme write-through -------------------------------------- */

/* The /theme Styx/9P node write-through observer. A kernel theme change is
 * a user interaction (= live training): absorb a cluster at /kv/theme/<path>
 * and run a cycle so the changed node is kept/pruned by density. */
void wubu_density_plan_theme_write(wubu_density_plan_t *p,
                                   const char *theme_path,
                                   uint32_t value);

/* ---- Accounting / queries (audited by the hosted test) --------- */

uint64_t wubu_density_plan_cycles(const wubu_density_plan_t *p);
uint64_t wubu_density_plan_absorbed(const wubu_density_plan_t *p);
uint64_t wubu_density_plan_kept(const wubu_density_plan_t *p);
uint64_t wubu_density_plan_pruned(const wubu_density_plan_t *p);
uint64_t wubu_density_plan_theme_writes(const wubu_density_plan_t *p);
uint64_t wubu_density_plan_gpu_frames(const wubu_density_plan_t *p);
int      wubu_density_plan_alive_count(const wubu_density_plan_t *p);

/* Global density = total_coherence / total_tokens over the LIVE set. */
float wubu_density_plan_density(const wubu_density_plan_t *p);

/* Density of a single cluster (0.0 if empty). */
float wubu_density_cluster_density(const WubuDensityEntry *e);

#endif /* WUBU_DENSITY_PLAN_H */
