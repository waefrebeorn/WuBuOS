/*
 * wubu_bonzi_mem.h -- the Bonzi companion's PERSISTENT memory.
 *
 * Persists the agentic-mem vector index (wubu_agentic_mem_t) to a 9P-backed
 * file (/n/bonzi/mem) so the companion remembers across restarts (JE26).
 * Access is gated by AD01 (wubu_9p_cap_allowed): only the companion's own
 * subtree may read or write the store. See wubu_bonzi_mem.c.
 */
#ifndef WUBU_BONZI_MEM_H
#define WUBU_BONZI_MEM_H

#include <stdint.h>

/* Forward-declared: the index type lives in wubu_vecsearch.h. Save/load only
 * need the struct layout, not the vecsearch functions. */
typedef struct {
    float    *embeddings;
    char    **actions;
    uint64_t *timestamps;
    double   *rewards;
    int dim;
    int max;
    int n;
} wubu_agentic_mem_t;

/* Override the default capability subtree ("/n/bonzi/mem"). */
void wubu_bonzi_mem_set_subtree(const char *subtree);

/* Serialize the index to path. Returns 0 on success, -1 on failure
 * (including an AD01 refusal). */
int wubu_bonzi_mem_save(const wubu_agentic_mem_t *m, const char *path,
                        const char *caller_subtree);

/* Load an index from path into a caller-allocated, already-dimensioned
 * wubu_agentic_mem_t. Sets m->n to the stored count. Returns 0 on success,
 * -1 on failure (bad magic, AD01 refusal, capacity/dim mismatch). */
int wubu_bonzi_mem_load(wubu_agentic_mem_t *m, const char *path,
                        const char *caller_subtree);

/* Track B3: recall the companion's memory and format a summon greeting into
 * buf (calls wubu_bonzi_greet_format). Loads the persisted index at the
 * default /n/bonzi/mem path (AD01-gated to the companion subtree). Returns 0
 * on success (greeting written), -1 if no memory / load failed. */
int wubu_bonzi_mem_recall(char *buf, int cap);

#endif /* WUBU_BONZI_MEM_H */
