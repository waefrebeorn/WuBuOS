/*
 * cab_extract.c -- list / get / extract-all from a CAB archive embedded in
 * a Windows installer, using ONLY the in-kernel CAB/LZX decoder
 * (wubu_cab.c + wubu_lzx.c + wubu_inflate.c).  No host tools.
 *
 * Usage:
 *   cab_extract list        <pe> <cab_off_hex>
 *   cab_extract get         <pe> <cab_off_hex> <member> <out>
 *   cab_extract extract-all <pe> <cab_off_hex> <out_dir>
 *
 * extract-all writes every member under <out_dir> (preserving the '\'
 * path layout as '/' dirs).  This turns a grabbed-from-the-internet
 * installer into the full game install tree on the Colonel.
 * C11, freestanding kernel decoders.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

#include "wubu_cab.h"
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
        fprintf(stderr, "usage: %s list <pe> <cab_off_hex>\n"
                        "       %s get  <pe> <cab_off_hex> <member> <out>\n"
                        "       %s extract-all <pe> <cab_off_hex> <out_dir>\n",
                argv[0], argv[0], argv[0]);
        return 1;
    }

    const char *pe_path;
    uint32_t cab_off;
    int mode = 0;   /* 1=list, 2=get, 3=extract-all */
    const char *member = NULL;
    const char *out_path = NULL;

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 4) {
            fprintf(stderr, "usage: %s list <pe> <cab_off_hex>\n", argv[0]);
            return 1;
        }
        mode = 1;
        pe_path = argv[2];
        cab_off = (uint32_t)strtoul(argv[3], NULL, 0);
    } else if (strcmp(argv[1], "get") == 0) {
        if (argc != 6) {
            fprintf(stderr, "usage: %s get <pe> <cab_off_hex> <member> <out>\n", argv[0]);
            return 1;
        }
        mode = 2;
        pe_path = argv[2];
        cab_off = (uint32_t)strtoul(argv[3], NULL, 0);
        member = argv[4];
        out_path = argv[5];
    } else if (strcmp(argv[1], "extract-all") == 0) {
        if (argc != 5) {
            fprintf(stderr, "usage: %s extract-all <pe> <cab_off_hex> <out_dir>\n", argv[0]);
            return 1;
        }
        mode = 3;
        pe_path = argv[2];
        cab_off = (uint32_t)strtoul(argv[3], NULL, 0);
        out_path = argv[4];
    } else {
        fprintf(stderr, "unknown command '%s'\n", argv[1]);
        return 1;
    }

    if (mem_init(256 * 1024 * 1024) != 0) {
        fprintf(stderr, "mem_init failed\n");
        return 1;
    }

    uint32_t pe_sz;
    uint8_t *pe = load_file(pe_path, &pe_sz);
    if (!pe) {
        fprintf(stderr, "cannot load %s\n", pe_path);
        mem_shutdown();
        return 1;
    }
    if (cab_off >= pe_sz) {
        fprintf(stderr, "cab offset 0x%x past end (%u)\n", cab_off, pe_sz);
        mem_shutdown();
        free(pe);
        return 1;
    }

    cab_archive cab;
    if (cab_open(&cab, pe + cab_off, pe_sz - cab_off) != 0) {
        fprintf(stderr, "cab_open failed on %s at 0x%x\n", pe_path, cab_off);
        mem_shutdown();
        free(pe);
        return 1;
    }

    fprintf(stderr, "CAB: %u folders, %u files\n", cab.c_folders, cab.c_files);

    if (mode == 1) {
        for (uint32_t i = 0; i < cab.c_files; i++) {
            fprintf(stderr, "  [%u] %s (%u bytes)\n", i, cab.files[i].name,
                    cab.files[i].uncomp_size);
        }
        mem_shutdown();
        free(pe);
        return 0;
    }

    if (mode == 2) {
        int idx = cab_find(&cab, member);
        if (idx < 0) {
            fprintf(stderr, "member '%s' not found\n", member);
            mem_shutdown();
            free(pe);
            return 1;
        }
        const cab_file *f = &cab.files[idx];
        uint8_t *out = malloc(f->uncomp_size);
        if (!out) {
            fprintf(stderr, "oom for %u bytes\n", f->uncomp_size);
            mem_shutdown();
            free(pe);
            return 1;
        }
        int32_t n = cab_extract(&cab, idx, out, f->uncomp_size);
        if (n != (int32_t)f->uncomp_size) {
            fprintf(stderr, "cab_extract failed: got %d, want %u\n", n,
                    f->uncomp_size);
            mem_shutdown();
            free(pe);
            free(out);
            return 1;
        }
        FILE *of = fopen(out_path, "wb");
        if (!of) {
            fprintf(stderr, "cannot write %s\n", out_path);
            mem_shutdown();
            free(pe);
            free(out);
            return 1;
        }
        fwrite(out, 1, (size_t)n, of);
        fclose(of);
        fprintf(stderr, "extracted %s: %d bytes -> %s\n", f->name, n, out_path);
        mem_shutdown();
        free(pe);
        free(out);
        return 0;
    }

    /* mode 3: extract-all */
    uint32_t ok = 0, failed = 0;
    for (uint32_t i = 0; i < cab.c_files; i++) {
        const cab_file *f = &cab.files[i];
        char out_path2[1024];
        /* Convert '\' separators to '/' and prefix out_dir. */
        char rel[512];
        size_t rl = strlen(f->name);
        if (rl >= sizeof(rel)) { failed++; continue; }
        for (size_t j = 0; j <= rl; j++) {
            rel[j] = (f->name[j] == '\\') ? '/' : f->name[j];
        }
        snprintf(out_path2, sizeof(out_path2), "%s/%s", out_path, rel);

        char *slash = strrchr(out_path2, '/');
        if (slash && slash != out_path2) {
            *slash = '\0';
            if (mkdirs(out_path2) != 0) {
                fprintf(stderr, "  [FAIL] mkdir %s\n", out_path2);
                failed++;
                continue;
            }
            *slash = '/';
        }

        uint8_t *out = malloc(f->uncomp_size);
        if (!out) {
            fprintf(stderr, "  [FAIL] oom %s\n", f->name);
            failed++;
            continue;
        }
        int32_t n = cab_extract(&cab, i, out, f->uncomp_size);
        if (n != (int32_t)f->uncomp_size) {
            fprintf(stderr, "  [FAIL] %s (%d/%u)\n", f->name, n,
                    f->uncomp_size);
            failed++;
            free(out);
            continue;
        }
        FILE *of = fopen(out_path2, "wb");
        if (!of) {
            fprintf(stderr, "  [FAIL] write %s\n", out_path2);
            failed++;
            free(out);
            continue;
        }
        fwrite(out, 1, (size_t)n, of);
        fclose(of);
        free(out);
        ok++;
    }
    fprintf(stderr, "extract-all: %u ok, %u failed, %u files total\n",
            ok, failed, cab.c_files);
    mem_shutdown();
    free(pe);
    return failed == 0 ? 0 : 1;
}
