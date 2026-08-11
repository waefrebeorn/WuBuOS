/*
 * zip_extract.c -- list / get / extract-all from a ZIP archive using ONLY
 * the in-kernel ZIP reader (wubu_zip.c + wubu_inflate.c).  No libz, no
 * host unzip.
 *
 * Usage:
 *   zip_extract list        <zip_path>
 *   zip_extract get         <zip_path> <member> <out_path>
 *   zip_extract extract-all <zip_path> <out_dir>
 *
 * extract-all walks the FULL central directory and writes every member
 * under <out_dir>, preserving the '/' path layout and creating parent
 * directories.  This is the capability that turns a grabbed-from-the-
 * internet game archive into a runnable install tree on the Colonel.
 * C11, freestanding kernel decoders.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

#include "wubu_zip.h"
#include "memory.h"

static uint8_t *load_file(const char *path, uint32_t *out_sz)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)sz);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(b); return NULL; }
    *out_sz = (uint32_t)sz;
    return b;
}

/* mkdir -p for a path that may end in '/'.  Returns 0 on success. */
static int mkdirs(const char *path)
{
    char tmp[1024];
    size_t n = strlen(path);
    if (n >= sizeof(tmp)) return -1;
    memcpy(tmp, path, n + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s list <zip_path>\n"
                        "       %s get  <zip_path> <member> <out_path>\n"
                        "       %s extract-all <zip_path> <out_dir>\n",
                argv[0], argv[0], argv[0]);
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
            free(data);
            return 1;
        }
        wubu_zip_archive z;
        if (wubu_zip_open(data, sz, &z) != 0) {
            fprintf(stderr, "zip_open failed on %s\n", argv[2]);
            mem_shutdown();
            free(data);
            return 1;
        }
        uint32_t n = wubu_zip_count(&z);
        uint64_t total = 0;
        fprintf(stderr, "ZIP: %u entries\n", n);
        for (uint32_t i = 0; i < n; i++) {
            const wubu_zip_entry *e = wubu_zip_entry_at(&z, i);
            if (!e) continue;
            total += e->uncomp_size;
            fprintf(stderr, "  [%u] %s (%u bytes, method=%u)\n",
                    i, e->name, e->uncomp_size, e->method);
        }
        fprintf(stderr, "TOTAL: %llu bytes (%llu MB)\n",
                (unsigned long long)total,
                (unsigned long long)(total / 1048576));
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
            free(data);
            return 1;
        }
        wubu_zip_archive z;
        if (wubu_zip_open(data, sz, &z) != 0) {
            fprintf(stderr, "zip_open failed on %s\n", argv[2]);
            mem_shutdown();
            free(data);
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
        fprintf(stderr, "extracted %s: %u bytes -> %s\n",
                argv[3], out_len, argv[4]);
        wubu_zip_close(&z);
        mem_shutdown();
        free(data);
        mem_free(out);
        return 0;
    }

    if (strcmp(argv[1], "extract-all") == 0) {
        if (argc != 4) {
            fprintf(stderr, "usage: %s extract-all <zip_path> <out_dir>\n", argv[0]);
            return 1;
        }
        uint32_t sz;
        uint8_t *data = load_file(argv[2], &sz);
        if (!data) {
            fprintf(stderr, "cannot load %s\n", argv[2]);
            return 1;
        }
        /* The OpenArena archive holds ~380MB of pk3 data; size the pool
         * to fit the largest single member plus the table. */
        if (mem_init(768 * 1024 * 1024) != 0) {
            fprintf(stderr, "mem_init failed (768MB pool)\n");
            free(data);
            return 1;
        }
        wubu_zip_archive z;
        if (wubu_zip_open(data, sz, &z) != 0) {
            fprintf(stderr, "zip_open failed on %s\n", argv[2]);
            mem_shutdown();
            free(data);
            return 1;
        }
        uint32_t n = wubu_zip_count(&z);
        uint32_t ok = 0, failed = 0;
        for (uint32_t i = 0; i < n; i++) {
            const wubu_zip_entry *e = wubu_zip_entry_at(&z, i);
            if (!e) continue;
            /* Skip directory entries (name ends in '/'). */
            size_t nl = strlen(e->name);
            if (nl > 0 && e->name[nl - 1] == '/') continue;

            char out_path[1024];
            snprintf(out_path, sizeof(out_path), "%s/%s", argv[3], e->name);
            /* Create parent dirs (up to the last '/'). */
            char *slash = strrchr(out_path, '/');
            if (slash && slash != out_path) {
                *slash = '\0';
                if (mkdirs(out_path) != 0) {
                    fprintf(stderr, "  [FAIL] mkdir %s\n", out_path);
                    failed++;
                    continue;
                }
                *slash = '/';
            }

            uint8_t *out = NULL;
            uint32_t out_len = 0;
            if (wubu_zip_extract(&z, i, &out, &out_len) != 0) {
                fprintf(stderr, "  [FAIL] %s\n", e->name);
                failed++;
                continue;
            }
            FILE *of = fopen(out_path, "wb");
            if (!of) {
                fprintf(stderr, "  [FAIL] write %s\n", out_path);
                failed++;
                mem_free(out);
                continue;
            }
            fwrite(out, 1, out_len, of);
            fclose(of);
            mem_free(out);
            ok++;
            fprintf(stderr, "  [%u/%u] %s (%u bytes)\n", ok, n, e->name, out_len);
        }
        fprintf(stderr, "extract-all: %u ok, %u failed, %u entries total\n",
                ok, failed, n);
        wubu_zip_close(&z);
        mem_shutdown();
        free(data);
        return failed == 0 ? 0 : 1;
    }

    fprintf(stderr, "unknown command '%s'\n", argv[1]);
    return 1;
}
