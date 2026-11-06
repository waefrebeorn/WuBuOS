/*
 * wubu_zxload.{c,h} -- the /n NAMESPACE loader.
 *
 * The glue between the named filesystem and the module-runner primitive:
 *   wubu_zxload_run(ops, path, base, resolve, out_err)
 * reads the file at `<g_ns_root>/<path>` (a /n path, e.g. "modules/hello.bin"
 * -> /n/modules/hello.bin) and hands the bytes to wubu_zx_run, which runs the
 * full parse->map->copy->relocate->snap-IAT(registry)->call-entry pipeline.
 *
 * This is the P2 "module loader on /n" seam: a compiled/holyd-emitted PE is
 * dropped into the namespace by a writer (ns_write / publisher), and any
 * reader — the shell, the session manager, a future .ZXE interpreter — can
 * "load" it by name with one call. Resolver defaults to the runtime symbol
 * registry (wubu_modsym), so the module's imports resolve against the same
 * export table the NT personality publishes into.
 *
 * Runtime-clen (no kernel-allocator dependence beyond wubu_zx_run's own ops).
 */

#include "wubu_zxload.h"
#include "wubu_zx.h"
#include "wubu_ns_bridge_internal.h"
#include <string.h>
#ifdef WUBU_HOSTED
#include <stdio.h>
#include <stdlib.h>
#endif

/* Name resolution: accept both "/n/..." and bare "modules/x"; prefix the root. */
static int build_path(const char *path, char *out, size_t cap)
{
    if (!path || !out) return -1;
    if (!g_ns_root) return -1;
    const char *rel = path;
    /* strip a leading version of the root or "/n/" */
    if (rel[0] == '/') {
        size_t rl = strlen(g_ns_root);
        if (strncmp(rel, g_ns_root, rl) == 0) rel = rel + rl;
        else if (strncmp(rel, "/n", 2) == 0) rel = rel + 2;
    }
    while (rel[0] == '/') rel++;
    size_t need = strlen(g_ns_root) + 1 + strlen(rel) + 1;
    if (need > cap) return -1;
    strcpy(out, g_ns_root);
    strcat(out, "/");
    strcat(out, rel);
    return 0;
}

#ifdef WUBU_HOSTED
/* Read the whole file at `/n`<path> (or <path>) into a new buffer (*out, caller
 * frees); returns byte count or <0. Reuses build_path resolution. */
int wubu_zxload_read(const char *path, uint8_t **_out, size_t *_sz)
{
    char full[512];
    if (build_path(path, full, sizeof(full)) != 0) return -1;
    FILE *f = fopen(full, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long n = ftell(f);
    if (n <= 0) { fclose(f); return -1; }
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return -1; }
    fclose(f);
    *_out = buf; *_sz = (size_t)n;
    return (int)n;
}

int wubu_zxload_run(const wubu_zx_ops *ops, const char *path, uintptr_t base,
                    wubu_pe_resolve_fn resolve, int *out_err)
{
    char full[512];
    if (build_path(path, full, sizeof(full)) != 0) {
        if (out_err) *out_err = WUBU_ZX_ERR_PARAM; return WUBU_ZX_ERR_PARAM;
    }
    FILE *f = fopen(full, "rb");
    if (!f) { if (out_err) *out_err = WUBU_ZX_ERR_PARSE; return WUBU_ZX_ERR_PARSE; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); if (out_err) *out_err = WUBU_ZX_ERR_PARSE; return WUBU_ZX_ERR_PARSE; }
    long n = ftell(f);
    if (n <= 0) { fclose(f); if (out_err) *out_err = WUBU_ZX_ERR_PARSE; return WUBU_ZX_ERR_PARSE; }
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(f); if (out_err) *out_err = WUBU_ZX_ERR_MAP; return WUBU_ZX_ERR_MAP; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if ((long)got != n) { free(buf); if (out_err) *out_err = WUBU_ZX_ERR_PARSE; return WUBU_ZX_ERR_PARSE; }
    int rc = wubu_zx_run(ops, buf, (size_t)n, base, resolve, out_err);
    free(buf);
    return rc;
}
#else
int wubu_zxload_run(const wubu_zx_ops *ops, const char *path, uintptr_t base,
                    wubu_pe_resolve_fn resolve, int *out_err);
#endif
