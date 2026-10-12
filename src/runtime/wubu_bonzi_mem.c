/*
 * wubu_bonzi_mem.c -- the Bonzi companion's PERSISTENT memory.
 *
 * The agentic-mem vector index (wubu_agentic_mem_t in wubu_vecsearch.h) is a
 * pure in-RAM index: embeddings, actions, timestamps, rewards. Its own doc
 * claims "persistent vector index survives" (AV03) but there is NO save/load
 * -- a restart wipes the companion's memory. This module closes that gap: it
 * serializes the index to a 9P-backed file (/n/bonzi/mem) and reloads it, so
 * the companion remembers across sessions (JE26 companion continuity).
 *
 * Format (little-endian, self-describing, append-friendly):
 *   magic "WBMEM001\0" (8 bytes)
 *   u32 dim, u32 max, u32 n
 *   n * dim      f32 embeddings
 *   n * 64       i8  action strings (fixed 64-byte slots, NUL-padded)
 *   n            u64 timestamps
 *   n            f64 rewards
 *
 * Access is gated by AD01 (wubu_9p_cap_allowed): only a caller whose subtree
 * owns /n/bonzi/mem may read or write it. The companion's own subtree passes;
 * an unrelated agent is refused. C11, minimal includes; links the Body's
 * src/brain vecsearch for the index type only.
 */

#include "wubu_bonzi_mem.h"
#include "wubu_agentic_os.h"   /* AD01: wubu_9p_cap_allowed */
#include "../gui/wubu_bonzi_greet.h"  /* Track B3: greeting */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WBMEM_MAGIC "WBMEM001"
#define WBMEM_MAGIC_LEN 8
#define WBMEM_ACTION_SLOT 64

/* The companion's capability subtree: it owns /n/bonzi/mem. */
static const char *g_mem_subtree = "/n/bonzi/mem";

void wubu_bonzi_mem_set_subtree(const char *subtree) {
    if (subtree) g_mem_subtree = subtree;
}

/* Serialize the index to path. Returns 0 on success, -1 on any failure
 * (including an AD01 refusal: caller's subtree doesn't own the file). */
int wubu_bonzi_mem_save(const wubu_agentic_mem_t *m, const char *path,
                        const char *caller_subtree)
{
    if (!m || !path) return -1;
    /* AD01 gate: the caller must own this path. */
    if (!wubu_9p_cap_allowed(caller_subtree ? caller_subtree : g_mem_subtree,
                             path))
        return -1;

    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    int ok = 1;
    ok = ok && fwrite(WBMEM_MAGIC, 1, WBMEM_MAGIC_LEN, f) == WBMEM_MAGIC_LEN;
    uint32_t dim = (uint32_t)m->dim, max = (uint32_t)m->max, n = (uint32_t)m->n;
    ok = ok && fwrite(&dim, 4, 1, f) == 1;
    ok = ok && fwrite(&max, 4, 1, f) == 1;
    ok = ok && fwrite(&n,   4, 1, f) == 1;
    if (ok && n > 0) {
        ok = ok && fwrite(m->embeddings, sizeof(float), (size_t)dim * n, f)
                  == (size_t)dim * n;
        /* actions: fixed 64-byte slots */
        for (uint32_t i = 0; i < n && ok; i++) {
            char slot[WBMEM_ACTION_SLOT];
            memset(slot, 0, sizeof slot);
            strncpy(slot, m->actions[i], WBMEM_ACTION_SLOT - 1);
            ok = ok && fwrite(slot, 1, WBMEM_ACTION_SLOT, f) == WBMEM_ACTION_SLOT;
        }
        ok = ok && fwrite(m->timestamps, sizeof(uint64_t), n, f) == n;
        ok = ok && fwrite(m->rewards, sizeof(double), n, f) == n;
    }
    fclose(f);
    return ok ? 0 : -1;
}

/* Load an index from path into a caller-allocated wubu_agentic_mem_t that is
 * ALREADY dimensioned (dim/max/arrays allocated by wubu_agentic_mem_init).
 * Returns 0 on success, -1 on failure (bad magic, AD01 refusal, capacity
 * mismatch, short read). n is set to the stored count. */
int wubu_bonzi_mem_load(wubu_agentic_mem_t *m, const char *path,
                        const char *caller_subtree)
{
    if (!m || !path) return -1;
    if (!wubu_9p_cap_allowed(caller_subtree ? caller_subtree : g_mem_subtree,
                             path))
        return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    char magic[WBMEM_MAGIC_LEN];
    uint32_t dim, max, n;
    int ok = 1;
    ok = ok && fread(magic, 1, WBMEM_MAGIC_LEN, f) == WBMEM_MAGIC_LEN;
    ok = ok && memcmp(magic, WBMEM_MAGIC, WBMEM_MAGIC_LEN) == 0;
    ok = ok && fread(&dim, 4, 1, f) == 1;
    ok = ok && fread(&max, 4, 1, f) == 1;
    ok = ok && fread(&n,   4, 1, f) == 1;
    /* capacity + dimension must match the pre-allocated index */
    ok = ok && (int)dim == m->dim && (int)max == m->max && (int)n <= m->max;
    if (ok && n > 0) {
        ok = ok && fread(m->embeddings, sizeof(float), (size_t)dim * n, f)
                  == (size_t)dim * n;
        for (uint32_t i = 0; i < n && ok; i++) {
            char slot[WBMEM_ACTION_SLOT];
            ok = ok && fread(slot, 1, WBMEM_ACTION_SLOT, f) == WBMEM_ACTION_SLOT;
            if (ok) {
                slot[WBMEM_ACTION_SLOT - 1] = '\0';
                strncpy(m->actions[i], slot, sizeof(m->actions[0]) - 1);
                m->actions[i][sizeof(m->actions[0]) - 1] = '\0';
            }
        }
        ok = ok && fread(m->timestamps, sizeof(uint64_t), n, f) == n;
        ok = ok && fread(m->rewards, sizeof(double), n, f) == n;
    }
    fclose(f);
    if (ok) m->n = (int)n;
    return ok ? 0 : -1;
}


/* Track B3: recall the companion's memory and format a summon greeting.
 * Loads the persisted index at the default path (AD01-gated to the companion
 * subtree), then hands the count + recent actions to the greeting formatter. */
int wubu_bonzi_mem_recall(char *buf, int cap)
{
    if (!buf || cap <= 0) return -1;
    const char *path = "/n/bonzi/mem";
    /* Allocate a modest index to load into. */
    enum { DIM = 8, MAX = 256 };
    wubu_agentic_mem_t m;
    m.dim = DIM; m.max = MAX; m.n = 0;
    m.embeddings = calloc((size_t)DIM * MAX, sizeof(float));
    m.actions    = calloc((size_t)MAX, sizeof(char *));
    for (int i = 0; i < MAX; i++) m.actions[i] = calloc(64, 1);
    m.timestamps = calloc((size_t)MAX, sizeof(uint64_t));
    m.rewards    = calloc((size_t)MAX, sizeof(double));
    if (!m.embeddings || !m.actions || !m.timestamps || !m.rewards) {
        for (int i = 0; i < MAX; i++) free(m.actions ? m.actions[i] : NULL);
        free(m.actions); free(m.embeddings); free(m.timestamps); free(m.rewards);
        return -1;
    }
    int rc = wubu_bonzi_mem_load(&m, path, "/n/bonzi");
    if (rc != 0 || m.n <= 0) {
        for (int i = 0; i < MAX; i++) free(m.actions[i]);
        free(m.actions); free(m.embeddings); free(m.timestamps); free(m.rewards);
        return -1;
    }
    /* The two most-recent actions (highest stored index = most recent). */
    const char *recent[2] = { NULL, NULL };
    recent[0] = m.actions[m.n - 1];
    if (m.n > 1) recent[1] = m.actions[m.n - 2];
    wubu_bonzi_greet_format(buf, cap, m.n, recent, 2);
    for (int i = 0; i < MAX; i++) free(m.actions[i]);
    free(m.actions); free(m.embeddings); free(m.timestamps); free(m.rewards);
    return 0;
}
