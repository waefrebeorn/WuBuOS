/*
 * wubu_zxreg.c  --  module registry (ZXE-style loader bookkeeping).
 *
 * Wraps wubu_zx_run + /n path resolution with load/dedup/refcount/unload.
 * Loading reads + hashes the PE bytes (SHA-256 via wubu_sha256); a repeat of
 * the same bytes reuses the existing slot (refcount++) instead of re-mapping.
 * This gives the "/n module loader" real module semantics — two apps importing
 * a shared module share one mapping, and the entry runs once (the result is
 * cached and re-served on dedup).
 *
 * Kernel-clean, fixed table, no allocator. The host read path is reused from
 * wubu_zxload (path resolution + stdio); the non-host build keeps the registry
 * but refuses loads (the kernel path walks /n via 9P, not stdio).
 */

#include "wubu_zxreg.h"
#include "wubu_zx.h"
#include "wubu_sha256.h"
#include <string.h>
#ifdef WUBU_HOSTED
#include <stdlib.h>
/* wubu_zxload_read is defined in the runtime wubu_zxload.c (hosted path); we
 * forward-declare it here because wubu_zxreg is a KERNEL module and must not
 * include a runtime header (the kernel object build has no -Isrc/runtime). */
int wubu_zxload_read(const char *path, uint8_t **_out, size_t *_sz);
#endif

static wubu_zxreg_entry_t g_reg[WUBU_ZXREG_MAX];
static int g_count = 0;

int wubu_zxreg_count(void) { return g_count; }

void wubu_zxreg_clear(void) { memset(g_reg, 0, sizeof(g_reg)); g_count = 0; }

wubu_zxreg_entry_t *wubu_zxreg_entry(int idx)
{
    if (idx < 0 || idx >= g_count) return NULL;
    return &g_reg[idx];
}

int wubu_zxreg_load(const wubu_zx_ops *ops, const char *path, uintptr_t base,
                    wubu_pe_resolve_fn resolve, wubu_zxreg_entry_t **out)
{
#ifdef WUBU_HOSTED
    uint8_t *bytes = NULL; size_t n = 0;
    if (wubu_zxload_read(path, &bytes, &n) < 0 || !bytes) return WUBU_ZX_ERR_PARSE;

    unsigned char sha[WUBU_SHA256_SZ];
    wubu_sha256(bytes, n, sha);

    /* exact-match dedup: same bytes at same base -> reuse slot, bump refcount */
    for (int i = 0; i < g_count; i++) {
        if (!g_reg[i].valid) continue;
        if (g_reg[i].base == base && memcmp(g_reg[i].sha, sha, WUBU_SHA256_SZ) == 0) {
            g_reg[i].refcount++;
            if (out) *out = &g_reg[i];
            int r = g_reg[i].last_result;
            free(bytes);
            return r;
        }
    }

    if (g_count >= WUBU_ZXREG_MAX) { free(bytes); return WUBU_ZX_ERR_MAP; }

    wubu_zxreg_entry_t *e = &g_reg[g_count];
    e->valid = 1; e->refcount = 1; e->base = base;
    memcpy(e->sha, sha, WUBU_SHA256_SZ);
    strncpy(e->path, path, sizeof(e->path) - 1);
    e->path[sizeof(e->path) - 1] = 0;

    int err = -99;
    int rc = wubu_zx_run(ops, bytes, n, base, resolve, &err);
    free(bytes);
    if (err != 0) { e->valid = 0; return err; }

    e->last_result = rc;
    g_count++;
    if (out) *out = e;
    return rc;
#else
    /* kernel path: /n walk is not stdio; the caller supplies a 9P-backed
     * loader. Registry bookkeeping (dedup/count) still works for seeds. */
    (void)ops; (void)path; (void)base; (void)resolve; (void)out;
    return WUBU_ZX_ERR_MAP;
#endif
}

void wubu_zxreg_unload(wubu_zxreg_entry_t *e)
{
    if (!e || !e->valid) return;
    if (e->refcount > 0) e->refcount--;
    if (e->refcount == 0) {
        /* the image was mapped/unmapped at the end of the single wubu_zx_run;
         * releasing the slot (entry ran once) is the module-registry invariant. */
        e->valid = 0;
        memset(e, 0, sizeof(*e));
        /* compact the table toward the front */
        for (int i = 0; i < g_count - 1; i++) {
            if (!g_reg[i].valid && g_reg[i + 1].valid) {
                g_reg[i] = g_reg[i + 1];
                memset(&g_reg[i + 1], 0, sizeof(g_reg[i + 1]));
            }
        }
        while (g_count > 0 && !g_reg[g_count - 1].valid) g_count--;
    }
}
