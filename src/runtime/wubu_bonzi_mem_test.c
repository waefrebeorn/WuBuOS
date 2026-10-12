/* src/runtime/wubu_bonzi_mem_test.c
 *
 * The companion persistent-memory test. Verifies save/load round-trips the
 * agentic-mem vector index to a 9P-backed file, and that AD01 gates access:
 *   1. save writes a loadable file; load restores embeddings/actions/timestamps
 *   2. a caller whose subtree doesn't own the path is refused (AD01)
 *   3. a corrupt/wrong-magic file is rejected
 */
#include "wubu_bonzi_mem.h"
#include "wubu_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MEMDIR  "/tmp/bonzi_mem_test.d"
#define MEMPATH MEMDIR "/mem.wbmem"

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

/* Build a small dimensioned index (the vecsearch module has no init, so the
 * caller allocates -- mirrors how test_vecsearch.c does it). */
static void mem_init(wubu_agentic_mem_t *m, int dim, int max)
{
    m->dim = dim; m->max = max; m->n = 0;
    m->embeddings = calloc((size_t)dim * max, sizeof(float));
    m->actions    = calloc((size_t)max, sizeof(char *));
    for (int i = 0; i < max; i++) m->actions[i] = calloc(64, 1);
    m->timestamps = calloc((size_t)max, sizeof(uint64_t));
    m->rewards    = calloc((size_t)max, sizeof(double));
}

static void mem_free(wubu_agentic_mem_t *m)
{
    for (int i = 0; i < m->max; i++) free(m->actions[i]);
    free(m->actions); free(m->embeddings);
    free(m->timestamps); free(m->rewards);
}

static void mem_put(wubu_agentic_mem_t *m, int idx, const float *emb,
                    const char *action, uint64_t ts, double reward)
{
    memcpy(&m->embeddings[idx * m->dim], emb, m->dim * sizeof(float));
    strncpy(m->actions[idx], action, 63);
    m->timestamps[idx] = ts;
    m->rewards[idx] = reward;
    if (idx + 1 > m->n) m->n = idx + 1;
}

int main(void)
{
    printf("=== wubu_bonzi_mem_test (companion persistent memory) ===\n");
    mkdir(MEMDIR, 0755);
    remove(MEMPATH);
    /* AD01 gates on path-under-subtree; point the subtree at our temp dir. */
    wubu_bonzi_mem_set_subtree(MEMDIR);

    /* 1. build a 3-entry index, save, wipe, reload, compare */
    wubu_agentic_mem_t a;
    mem_init(&a, 4, 8);
    float e0[4] = {1,0,0,0}, e1[4] = {0,1,0,0}, e2[4] = {0,0,1,0};
    mem_put(&a, 0, e0, "greet",   100, 0.5);
    mem_put(&a, 1, e1, "explore", 200, 0.9);
    mem_put(&a, 2, e2, "remember",300, 0.1);

    CHECK(wubu_bonzi_mem_save(&a, MEMPATH, MEMDIR) == 0,
          "save the index to the 9P-backed file");

    wubu_agentic_mem_t b;
    mem_init(&b, 4, 8);
    CHECK(wubu_bonzi_mem_load(&b, MEMPATH, MEMDIR) == 0,
          "load the index back");
    CHECK(b.n == 3, "reloaded n == 3");
    CHECK(b.embeddings[0] == 1.0f && b.embeddings[1] == 0.0f,
          "reloaded embedding[0] matches");
    CHECK(b.embeddings[4*2 + 2] == 1.0f,
          "reloaded embedding[2] matches (row-major)");
    CHECK(strcmp(b.actions[1], "explore") == 0, "reloaded action[1] == explore");
    CHECK(b.timestamps[2] == 300, "reloaded timestamp[2] == 300");
    CHECK(b.rewards[0] == 0.5, "reloaded reward[0] == 0.5");
    mem_free(&a); mem_free(&b);

    /* 2. AD01: a caller whose subtree does NOT own the path is refused */
    wubu_agentic_mem_t c;
    mem_init(&c, 4, 8);
    CHECK(wubu_bonzi_mem_save(&c, MEMPATH, "/n/other") == -1,
          "AD01 refuses save from an unowned subtree");
    CHECK(wubu_bonzi_mem_load(&c, MEMPATH, "/n/other") == -1,
          "AD01 refuses load from an unowned subtree");
    mem_free(&c);

    /* 3. corrupt file (wrong magic) is rejected */
    FILE *f = fopen(MEMPATH, "wb");
    fputs("NOTWBMEM", f);
    fclose(f);
    wubu_agentic_mem_t d;
    mem_init(&d, 4, 8);
    CHECK(wubu_bonzi_mem_load(&d, MEMPATH, MEMDIR) == -1,
          "wrong-magic file is rejected");
    mem_free(&d);

    remove(MEMPATH);
    rmdir(MEMDIR);
    printf("=== wubu_bonzi_mem_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
