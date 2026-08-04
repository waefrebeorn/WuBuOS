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

/* W3: enumerate the live spaces (the broker / -spaces view).
 * cb(space, user) returns 0 to continue, nonzero to stop. */
void wubu_runtime_list(const wubu_runtime_t *rt,
                       int (*cb)(const wubu_rt_space_t *, void *),
                       void *user)
{
    if (!rt || !cb) return;
    wubu_hive_iter_t it;
    void *e;
    for (e = wubu_hive_first(rt->hive, &it); e; e = wubu_hive_next(rt->hive, &it)) {
        const rt_slot_t *s = (const rt_slot_t *)e;
        if (cb(&s->space, user)) return;
    }
}

/* ====================================================================
 * W4/W5 -- PERSISTENCE (the "nothing left in the dust" guarantee)
 *
 * A flat, versioned binary format: magic + version + count, then one
 * fixed-size record per space (the struct fields + the personality
 * name). No external deps, self-contained C11.
 * ==================================================================== */

#define WUBU_RT_FILE_MAGIC 0x57554252544E5350ull  /* "WUBRTNSP" */
#define WUBU_RT_FILE_VER   1

/* the on-disk record: fixed-size, so the file is a plain array */
typedef struct {
    uint64_t id;
    uint64_t seq;
    char name[64];
    char language[48];
    char compiler_ver[32];
    char language_ver[32];
    char abi_snapshot[96];
    char created[32];
    char namespace_path[64];
    uint64_t heap_cap;
    uint64_t heap_used;
    int32_t state;
    char personality[16];
} rt_file_rec_t;

typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t count;
} rt_file_hdr_t;

int wubu_runtime_save(const wubu_runtime_t *rt, const char *path)
{
    if (!rt || !path) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    rt_file_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = WUBU_RT_FILE_MAGIC;
    hdr.version = WUBU_RT_FILE_VER;
    hdr.count = (uint32_t)wubu_runtime_count(rt);
    if (fwrite(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return -1; }

    wubu_hive_iter_t it;
    void *e;
    for (e = wubu_hive_first(rt->hive, &it); e; e = wubu_hive_next(rt->hive, &it)) {
        const rt_slot_t *s = (const rt_slot_t *)e;
        rt_file_rec_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.id = s->space.id;
        rec.seq = s->seq;
        snprintf(rec.name, sizeof(rec.name), "%s", s->space.name);
        snprintf(rec.language, sizeof(rec.language), "%s", s->space.language);
        snprintf(rec.compiler_ver, sizeof(rec.compiler_ver), "%s",
                 s->space.compiler_ver);
        snprintf(rec.language_ver, sizeof(rec.language_ver), "%s",
                 s->space.language_ver);
        snprintf(rec.abi_snapshot, sizeof(rec.abi_snapshot), "%s",
                 s->space.abi_snapshot);
        snprintf(rec.created, sizeof(rec.created), "%s", s->space.created);
        snprintf(rec.namespace_path, sizeof(rec.namespace_path), "%s",
                 s->space.namespace_path);
        rec.heap_cap = s->space.heap_cap;
        rec.heap_used = s->space.heap_used;
        rec.state = (int32_t)s->space.state;
        snprintf(rec.personality, sizeof(rec.personality), "%s",
                 s->space.personality ? s->space.personality->name : "");
        if (fwrite(&rec, sizeof(rec), 1, f) != 1) { fclose(f); return -1; }
    }
    fclose(f);
    return 0;
}

int wubu_runtime_load(wubu_runtime_t *rt, const char *path)
{
    if (!rt || !path) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    rt_file_hdr_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1 ||
        hdr.magic != WUBU_RT_FILE_MAGIC || hdr.version != WUBU_RT_FILE_VER) {
        fclose(f);
        return -1;               /* not ours / corrupt */
    }

    for (uint32_t i = 0; i < hdr.count; i++) {
        rt_file_rec_t rec;
        if (fread(&rec, sizeof(rec), 1, f) != 1) { fclose(f); return -1; }
        if (rt->n_live >= rt->cfg.max_spaces) {
            /* ring discipline: at cap, skip extra records */
            continue;
        }
        rt_slot_t *s = (rt_slot_t *)calloc(1, sizeof(*s));
        if (!s) { fclose(f); return -1; }
        s->seq = rec.seq;
        s->space.id = rec.id;
        snprintf(s->space.name, sizeof(s->space.name), "%s", rec.name);
        snprintf(s->space.language, sizeof(s->space.language), "%s", rec.language);
        snprintf(s->space.compiler_ver, sizeof(s->space.compiler_ver), "%s",
                 rec.compiler_ver);
        snprintf(s->space.language_ver, sizeof(s->space.language_ver), "%s",
                 rec.language_ver);
        snprintf(s->space.abi_snapshot, sizeof(s->space.abi_snapshot), "%s",
                 rec.abi_snapshot);
        snprintf(s->space.created, sizeof(s->space.created), "%s", rec.created);
        snprintf(s->space.namespace_path, sizeof(s->space.namespace_path), "%s",
                 rec.namespace_path);
        s->space.heap_cap = rec.heap_cap;
        s->space.heap_used = rec.heap_used;
        if (s->space.id >= rt->next_id) rt->next_id = s->space.id + 1;
        if (wubu_hive_insert(rt->hive, s) != s) { free(s); continue; }
        rt->n_live++;
        /* personality + state AFTER the insert (set_personality finds
         * the slot by id — it must already be in the hive) */
        if (rec.personality[0])
            wubu_runtime_set_personality(rt, rec.id, rec.personality);
        wubu_runtime_find(rt, rec.id)->state = (wubu_rt_state_t)rec.state;
    }
    fclose(f);
    return 0;
}
