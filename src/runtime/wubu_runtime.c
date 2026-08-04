/*
 * wubu_runtime.c -- WUBURUNTIME: the compilation-space registry.
 *
 * Every OO runtime gets its own compilation space (research/063):
 * named, versioned, hive-backed. The snapshot fields are recorded at
 * creation so nothing is left in the dust.
 *
 * C11, self-contained (hive-backed slots). Uses the wubuos hive
 * (src/kernel/wubu_hive.h — heap hive, first/next iteration).
 */
#define _POSIX_C_SOURCE 200809L   /* localtime_r under -std=c11 */
#include "wubu_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

struct wubu_runtime {
    wubu_hive_t *hive;
    wubu_rt_cfg_t cfg;
    size_t n_live;              /* live spaces (ring-bounded) */
    uint64_t next_id;           /* monotonic id source */
};

/* the hive payload: the space + the ring key */
typedef struct { wubu_rt_space_t space; uint64_t seq; } rt_slot_t;

static void today(char *buf, size_t cap)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(buf, cap, "%04d-%02d-%02d", tm.tm_year + 1900,
             tm.tm_mon + 1, tm.tm_mday);
}

wubu_runtime_t *wubu_runtime_init(wubu_hive_t *hive,
                                  const wubu_rt_cfg_t *cfg)
{
    if (!hive) return NULL;
    wubu_runtime_t *rt = (wubu_runtime_t *)calloc(1, sizeof(*rt));
    if (!rt) return NULL;
    rt->hive = hive;
    if (cfg) rt->cfg = *cfg;
    if (rt->cfg.max_spaces == 0) rt->cfg.max_spaces = 16;
    if (rt->cfg.default_heap_cap == 0) rt->cfg.default_heap_cap = 1ull << 30;
    rt->next_id = 1;
    return rt;
}

void wubu_runtime_free(wubu_runtime_t *rt)
{
    if (!rt) return;
    /* free every slot payload (the hive itself is caller-owned) */
    wubu_hive_iter_t it;
    void *e;
    for (e = wubu_hive_first(rt->hive, &it); e; e = wubu_hive_next(rt->hive, &it))
        free(e);
    wubu_hive_clear(rt->hive);
    free(rt);
}

uint64_t wubu_runtime_create(wubu_runtime_t *rt,
                             const char *name,
                             const char *language,
                             const char *compiler_ver,
                             const char *language_ver,
                             const char *abi_snapshot,
                             const char *namespace_path)
{
    if (!rt || !name || !language) return 0;

    /* ring discipline: at cap, recycle the oldest (the amoeba membrane) */
    if (rt->n_live >= rt->cfg.max_spaces) {
        rt_slot_t *oldest = NULL;
        uint64_t min_seq = 0;
        wubu_hive_iter_t it;
        void *e;
        for (e = wubu_hive_first(rt->hive, &it); e; e = wubu_hive_next(rt->hive, &it)) {
            rt_slot_t *s = (rt_slot_t *)e;
            if (!oldest || s->seq < min_seq) { oldest = s; min_seq = s->seq; }
        }
        if (oldest) {
            wubu_hive_erase(rt->hive, oldest);
            free(oldest);
            if (rt->n_live > 0) rt->n_live--;
        }
    }

    rt_slot_t *s = (rt_slot_t *)calloc(1, sizeof(*s));
    if (!s) return 0;
    s->seq = rt->next_id;
    s->space.id = rt->next_id;
    rt->next_id++;

    snprintf(s->space.name, sizeof(s->space.name), "%s", name);
    snprintf(s->space.language, sizeof(s->space.language), "%s", language);
    /* THE SNAPSHOT: compiler + language versions + the date, recorded
     * at creation — nothing is left in the dust. */
    snprintf(s->space.compiler_ver, sizeof(s->space.compiler_ver),
             "%s", compiler_ver ? compiler_ver : "holyc-0.1.0");
    snprintf(s->space.language_ver, sizeof(s->space.language_ver),
             "%s", language_ver ? language_ver : "unknown");
    snprintf(s->space.abi_snapshot, sizeof(s->space.abi_snapshot),
             "%s", abi_snapshot ? abi_snapshot : "wubu-abi-v1");
    today(s->space.created, sizeof(s->space.created));
    snprintf(s->space.namespace_path, sizeof(s->space.namespace_path),
             "%s", namespace_path ? namespace_path : "/n/");
    s->space.heap_cap = rt->cfg.default_heap_cap;
    s->space.heap_used = 0;
    s->space.state = WUBU_RT_COLD;

    if (wubu_hive_insert(rt->hive, s) != s) { free(s); return 0; }
    rt->n_live++;
    return s->space.id;
}

wubu_rt_space_t *wubu_runtime_find(wubu_runtime_t *rt, uint64_t id)
{
    if (!rt) return NULL;
    wubu_hive_iter_t it;
    void *e;
    for (e = wubu_hive_first(rt->hive, &it); e; e = wubu_hive_next(rt->hive, &it)) {
        rt_slot_t *s = (rt_slot_t *)e;
        if (s->space.id == id) return &s->space;
    }
    return NULL;
}

wubu_rt_space_t *wubu_runtime_find_name(wubu_runtime_t *rt,
                                        const char *name)
{
    if (!rt || !name) return NULL;
    wubu_hive_iter_t it;
    void *e;
    for (e = wubu_hive_first(rt->hive, &it); e; e = wubu_hive_next(rt->hive, &it)) {
        rt_slot_t *s = (rt_slot_t *)e;
        if (!strcmp(s->space.name, name)) return &s->space;
    }
    return NULL;
}

int wubu_runtime_set_state(wubu_runtime_t *rt, uint64_t id,
                           wubu_rt_state_t state)
{
    wubu_rt_space_t *sp = wubu_runtime_find(rt, id);
    if (!sp) return -1;
    sp->state = state;
    return 0;
}

int wubu_runtime_touch_heap(wubu_runtime_t *rt, uint64_t id,
                            int64_t delta)
{
    wubu_rt_space_t *sp = wubu_runtime_find(rt, id);
    if (!sp) return -1;
    if (delta >= 0 &&
        sp->heap_used + (uint64_t)delta > sp->heap_cap)
        return -1;              /* over the ring cap: refuse */
    if ((int64_t)sp->heap_used + delta < 0)
        sp->heap_used = 0;
    else
        sp->heap_used += (uint64_t)delta;
    return 0;
}

int wubu_runtime_destroy(wubu_runtime_t *rt, uint64_t id)
{
    if (!rt) return -1;
    wubu_hive_iter_t it;
    void *e;
    for (e = wubu_hive_first(rt->hive, &it); e; e = wubu_hive_next(rt->hive, &it)) {
        rt_slot_t *s = (rt_slot_t *)e;
        if (s->space.id == id) {
            wubu_hive_erase(rt->hive, s);
            free(s);
            if (rt->n_live > 0) rt->n_live--;
            return 0;
        }
    }
    return -1;
}

size_t wubu_runtime_count(const wubu_runtime_t *rt)
{
    return rt ? rt->n_live : 0;
}
