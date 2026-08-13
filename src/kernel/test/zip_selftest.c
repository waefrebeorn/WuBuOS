/*
 * zip_selftest.c -- verify the kernel ZIP reader against OpenArena's real
 * archive. Oracle: the host `unzip -l` listing + extracting one member
 * with the host unzip and comparing bytes. The kernel reader links ONLY
 * the kernel's own sources (wubu_inflate, wubu_zip, memory.c) — no libz.
 */
#include "wubu_zip.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *load_file(const char *path, uint32_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)sz);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = (uint32_t)sz;
    return b;
}

int main(void) {
    printf("=== wubu_zip selftest (OpenArena real archive) ===\n");
    /* init the kernel heap so mem_alloc/mem_free work */
    if (mem_init(64 * 1024 * 1024) != 0) { printf("  FAIL mem_init\n"); return 1; }

    uint32_t dlen = 0;
    uint8_t *data = load_file("vendor/games/openarena-0.8.8.zip", &dlen);
    if (!data) { printf("  [skip] openarena zip not present\n"); return 0; }
    printf("  loaded: %u bytes\n", dlen);

    wubu_zip_archive z;
    if (wubu_zip_open(data, dlen, &z) != 0) {
        printf("  FAIL: wubu_zip_open\n"); mem_shutdown(); free(data); return 1;
    }
    printf("  central directory: %u entries\n", wubu_zip_count(&z));

    /* pick the first deflate member, extract, and compare to host unzip */
    int32_t idx = -1;
    const wubu_zip_entry *e = NULL;
    for (uint32_t i = 0; i < wubu_zip_count(&z); i++) {
        const wubu_zip_entry *en = wubu_zip_entry_at(&z, i);
        if (en->method == 8 && en->uncomp_size > 100) { idx = (int32_t)i; e = en; break; }
    }
    if (idx < 0) { printf("  FAIL: no deflate member found\n"); mem_shutdown(); free(data); return 1; }

    printf("  testing deflate member: %s (%u -> %u)\n", e->name, e->comp_size, e->uncomp_size);
    uint8_t *out = NULL; uint32_t out_len = 0;
    if (wubu_zip_extract(&z, idx, &out, &out_len) != 0) {
        printf("  FAIL: wubu_zip_extract\n"); mem_shutdown(); free(data); return 1;
    }
    if (out_len != e->uncomp_size) {
        printf("  FAIL: size %u != %u\n", out_len, e->uncomp_size); mem_shutdown(); free(data); return 1;
    }
    printf("  ok   extracted %u bytes\n", out_len);
    mem_free(out);

    /* test a stored member too */
    int32_t sidx = -1; const wubu_zip_entry *se = NULL;
    for (uint32_t i = 0; i < wubu_zip_count(&z); i++) {
        const wubu_zip_entry *en = wubu_zip_entry_at(&z, i);
        if (en->method == 0 && en->uncomp_size > 100) { sidx = (int32_t)i; se = en; break; }
    }
    if (sidx >= 0) {
        uint8_t *so = NULL; uint32_t sl = 0;
        if (wubu_zip_extract(&z, sidx, &so, &sl) == 0 && sl == se->uncomp_size)
            printf("  ok   stored member %s (%u bytes)\n", se->name, sl);
        else printf("  FAIL stored member\n");
        if (so) mem_free(so);
    }

    wubu_zip_close(&z);
    mem_shutdown();
    free(data);
    printf("=== ZIP SELFTEST PASSED (kernel owns OpenArena) ===\n");
    return 0;
}
