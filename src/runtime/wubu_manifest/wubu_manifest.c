/*
 * wubu_manifest.c -- WuBuOS unified manifest API (load/resolve/gate/emit).
 * Adopted from GrahaOS etc/gcp.json: single source of truth for VSL dispatch,
 * Styx9P ops, and HolyC FFI. The VSL dispatcher resolves a syscall number
 * through this module and enforces the required capability BEFORE invoking the
 * handler (capability-only authority, replacing the old uid/permission gate).
 */
#include "wubu_manifest_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Best-effort: create the output directory if missing (idempotent). */
static void ensure_dir(const char *dir) {
    struct stat st;
    if (stat(dir, &st) == 0) return; /* already exists */
#ifdef _WIN32
    _mkdir(dir);
#else
    (void)mkdir(dir, 0755);
#endif
}

wubu_manifest_t *wubu_manifest_parse(const char *json, size_t len) {
    wubu_manifest_t *m = (wubu_manifest_t *)calloc(1, sizeof(*m));
    if (!m) return NULL;
    if (wubu_json_parse_manifest(json, len, m) != 0) {
        free(m);
        return NULL;
    }
    return m;
}

wubu_manifest_t *wubu_manifest_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    wubu_manifest_t *m = wubu_manifest_parse(buf, rd);
    free(buf);
    return m;
}

void wubu_manifest_destroy(wubu_manifest_t *m) { free(m); }

int wubu_manifest_count(const wubu_manifest_t *m) {
    return m ? m->count : 0;
}

bool wubu_manifest_get(const wubu_manifest_t *m, int i,
                       uint64_t *out_num, const char **out_handler,
                       uint64_t *out_right, const char **out_cap) {
    if (!m || i < 0 || i >= m->count) return false;
    if (out_num)    *out_num    = m->entries[i].num;
    if (out_handler)*out_handler= m->entries[i].handler;
    if (out_right)  *out_right  = m->entries[i].right;
    if (out_cap)    *out_cap    = m->entries[i].cap;
    return true;
}

bool wubu_manifest_resolve(const wubu_manifest_t *m, uint64_t num,
                           const char **out_handler, uint64_t *out_right) {
    if (!m) return false;
    for (int i = 0; i < m->count; i++) {
        if (m->entries[i].num == num) {
            if (out_handler) *out_handler = m->entries[i].handler;
            if (out_right)   *out_right   = m->entries[i].right;
            return true;
        }
    }
    return false;
}

bool wubu_manifest_resolve_gated(const wubu_manifest_t *m, uint64_t num,
                                  const char **out_handler, uint64_t *out_right,
                                  wubu_manifest_cap_fn cap_check, void *cap_ctx) {
    const char *h; uint64_t r;
    if (!wubu_manifest_resolve(m, num, &h, &r)) return false;
    if (out_handler) *out_handler = h;
    if (out_right)   *out_right   = r;
    if (cap_check && !cap_check(cap_ctx, r)) return false; /* cap gate denies */
    return true;
}

/* ---- Code generation (the GrahaOS gcp2wit.py step) ---- */

static int emit_vsl_dispatch(const wubu_manifest_t *m, const char *dir) {
    char path[512];
    snprintf(path, sizeof(path), "%s/wubu_vsl_dispatch.h", dir);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "/* wubu_vsl_dispatch.h -- GENERATED from wubu_manifest.json. Do not edit. */\n");
    fprintf(f, "#ifndef WUBU_VSL_DISPATCH_H\n#define WUBU_VSL_DISPATCH_H\n");
    fprintf(f, "#include <stdint.h>\n/* Syscall numbers (single source: manifest). */\n");
    for (int i = 0; i < m->count; i++)
        fprintf(f, "#define VSL_SYS_%s %llu\n",
                m->entries[i].name, (unsigned long long)m->entries[i].num);
    fprintf(f, "\n/* Capability-gated dispatch: caller must hold `req_right` (wubu_cap) */\n");
    fprintf(f, "typedef struct { const char *handler; uint64_t req_right; } wubu_vsl_disp_t;\n");
    fprintf(f, "static const wubu_vsl_disp_t wubu_vsl_dispatch[] = {\n");
    for (int i = 0; i < m->count; i++)
        fprintf(f, "  [%llu] = { .handler=\"%s\", .req_right=0x%llxULL }, /* %s */\n",
                (unsigned long long)m->entries[i].num, m->entries[i].handler,
                (unsigned long long)m->entries[i].right, m->entries[i].cap);
    fprintf(f, "};\n#endif\n");
    fclose(f);
    return 0;
}

static int emit_styx_ops(const wubu_manifest_t *m, const char *dir) {
    char path[512];
    snprintf(path, sizeof(path), "%s/wubu_styx_ops.h", dir);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "/* wubu_styx_ops.h -- GENERATED from wubu_manifest.json. Do not edit. */\n");
    fprintf(f, "#ifndef WUBU_STYX_OPS_H\n#define WUBU_STYX_OPS_H\ntypedef enum {\n");
    for (int i = 0; i < m->count; i++)
        fprintf(f, "  WUBU_STYX_%s = %d, /* %s */\n",
                m->entries[i].name, i, m->entries[i].styx);
    fprintf(f, "  WUBU_STYX_COUNT = %d\n} wubu_styx_op_t;\n#endif\n", m->count);
    fclose(f);
    return 0;
}

static int emit_holyc_ffi(const wubu_manifest_t *m, const char *dir) {
    char path[512];
    snprintf(path, sizeof(path), "%s/wubu_holyc_ffi.h", dir);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "/* wubu_holyc_ffi.h -- GENERATED from wubu_manifest.json. Do not edit. */\n");
    fprintf(f, "#ifndef WUBU_HOLYC_FFI_H\n#define WUBU_HOLYC_FFI_H\n");
    for (int i = 0; i < m->count; i++)
        fprintf(f, "int64 %s(int64 num, int64 rdi, int64 rsi, int64 rdx, int64 r10, int64 r8, int64 r9); /* %s */\n",
                m->entries[i].holyc, m->entries[i].handler);
    fprintf(f, "#endif\n");
    fclose(f);
    return 0;
}

int wubu_manifest_emit(const wubu_manifest_t *m, const char *out_dir) {
    if (!m || !out_dir) return -1;
    ensure_dir(out_dir);
    if (emit_vsl_dispatch(m, out_dir) != 0) return -1;
    if (emit_styx_ops(m, out_dir) != 0) return -1;
    if (emit_holyc_ffi(m, out_dir) != 0) return -1;
    return 0;
}
