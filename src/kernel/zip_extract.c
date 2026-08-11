/*
 * zip_extract -- list or extract a member from a ZIP archive using ONLY
 *                the kernel's own wubu_zip reader (wubu_zip.c), which links
 *                the kernel DEFLATE decoder (wubu_inflate.c).
 *
 * Usage:
 *   zip_extract list <zip_path>
 *   zip_extract get  <zip_path> <member_name> <out_path>
 *
 * The archive bytes are read from the host file, but ALL decompression is
 * done by the kernel's own code path -- no libz, no unzip, no host tools.
 */
#include "wubu_zip.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *load_file(const char *path, uint32_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *sz = (uint32_t)n;
    return b;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s list <zip_path>\n"
                        "       %s get  <zip_path> <member> <out_path>\n",
                argv[0], argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 3) {
            fprintf(stderr, "usage: %s list <zip_path>\n", argv[0]);
            return 1;
        }
        uint32_t sz;
        uint8_t *data = load_file(argv[2], &sz);
        if (!data) {
            fprintf(stderr, "cannot load %s\n", argv[2]);
            return 1;
        }
        if (mem_init(256 * 1024 * 1024) != 0) {
            fprintf(stderr, "mem_init failed\n");
            return 1;
        }
        wubu_zip_archive z;
        if (wubu_zip_open(data, sz, &z) != 0) {
            fprintf(stderr, "zip_open failed on %s\n", argv[2]);
            return 1;
        }
        uint32_t n = wubu_zip_count(&z);
        fprintf(stderr, "ZIP: %u entries\n", n);
        for (uint32_t i = 0; i < n; i++) {
            const wubu_zip_entry *e = wubu_zip_entry_at(&z, i);
            if (!e) continue;
            fprintf(stderr, "  [%u] %s (%u bytes, method=%u)\n",
                    i, e->name, e->uncomp_size, e->method);
        }
        wubu_zip_close(&z);
        mem_shutdown();
        free(data);
        return 0;
    }

    if (strcmp(argv[1], "get") == 0) {
        if (argc != 5) {
            fprintf(stderr, "usage: %s get <zip_path> <member> <out_path>\n", argv[0]);
            return 1;
        }
        uint32_t sz;
        uint8_t *data = load_file(argv[2], &sz);
        if (!data) {
            fprintf(stderr, "cannot load %s\n", argv[2]);
            return 1;
        }
        if (mem_init(256 * 1024 * 1024) != 0) {
            fprintf(stderr, "mem_init failed\n");
            return 1;
        }
        wubu_zip_archive z;
        if (wubu_zip_open(data, sz, &z) != 0) {
            fprintf(stderr, "zip_open failed on %s\n", argv[2]);
            return 1;
        }
        int32_t idx = wubu_zip_find(&z, argv[3]);
        if (idx < 0) {
            fprintf(stderr, "member '%s' not found\n", argv[3]);
            wubu_zip_close(&z);
            mem_shutdown();
            free(data);
            return 1;
        }
        uint8_t *out = NULL;
        uint32_t out_len = 0;
        if (wubu_zip_extract(&z, (uint32_t)idx, &out, &out_len) != 0) {
            fprintf(stderr, "extract failed\n");
            wubu_zip_close(&z);
            mem_shutdown();
            free(data);
            free(out);
            return 1;
        }
        FILE *of = fopen(argv[4], "wb");
        if (!of) {
            fprintf(stderr, "cannot write %s\n", argv[4]);
            wubu_zip_close(&z);
            mem_shutdown();
            free(data);
            mem_free(out);
            return 1;
        }
        fwrite(out, 1, out_len, of);
        fclose(of);
        fprintf(stderr, "extracted %s: %u bytes -> %s (magic: %02x %02x %02x %02x)\n",
                argv[3], out_len, argv[4], out[0], out[1], out[2], out[3]);
        wubu_zip_close(&z);
        mem_shutdown();
        free(data);
        mem_free(out);
        return 0;
    }

    fprintf(stderr, "unknown command '%s'\n", argv[1]);
    return 1;
}
