/*
 * cab_extract -- extract (or list) a file from an embedded CAB cabinet
 *                using ONLY the kernel's own wubu_cab reader (wubu_cab.c),
 *                which links the kernel LZX/MSZIP/DEFLATE decoders.
 *
 * Usage:
 *   cab_extract list <pe_path> <cab_offset_hex>
 *   cab_extract get  <pe_path> <cab_offset_hex> <member_name> <out_path>
 *
 * The CAB bytes are read from the host file, but ALL decompression is
 * done by the kernel's own code path -- no 7z, no libz, no host tools.
 */
#include "wubu_cab.h"
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
        fprintf(stderr, "usage: %s list <pe> <cab_off_hex>\n"
                        "       %s get  <pe> <cab_off_hex> <member> <out>\n",
                argv[0], argv[0]);
        return 1;
    }

    const char *pe_path;
    uint32_t cab_off;
    int listing = 0;
    const char *member = NULL;
    const char *out_path = NULL;

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 4) {
            fprintf(stderr, "usage: %s list <pe> <cab_off_hex>\n", argv[0]);
            return 1;
        }
        listing = 1;
        pe_path = argv[2];
        cab_off = (uint32_t)strtoul(argv[3], NULL, 0);
    } else if (strcmp(argv[1], "get") == 0) {
        if (argc != 6) {
            fprintf(stderr, "usage: %s get <pe> <cab_off_hex> <member> <out>\n", argv[0]);
            return 1;
        }
        pe_path = argv[2];
        cab_off = (uint32_t)strtoul(argv[3], NULL, 0);
        member = argv[4];
        out_path = argv[5];
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
        return 1;
    }
    if (cab_off >= pe_sz) {
        fprintf(stderr, "cab offset 0x%x past end (%u)\n", cab_off, pe_sz);
        return 1;
    }

    cab_archive cab;
    if (cab_open(&cab, pe + cab_off, pe_sz - cab_off) != 0) {
        fprintf(stderr, "cab_open failed on %s at 0x%x\n", pe_path, cab_off);
        return 1;
    }

    fprintf(stderr, "CAB: %u folders, %u files\n", cab.c_folders, cab.c_files);
    for (uint32_t i = 0; i < cab.c_files; i++) {
        fprintf(stderr, "  [%u] %s (%u bytes)\n", i, cab.files[i].name, cab.files[i].uncomp_size);
    }

    if (listing) {
        mem_shutdown();
        free(pe);
        return 0;
    }

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
        fprintf(stderr, "cab_extract failed: got %d, want %u\n", n, f->uncomp_size);
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
    fprintf(stderr, "extracted %s: %d bytes -> %s (magic: %02x %02x %02x %02x)\n",
            f->name, n, out_path, out[0], out[1], out[2], out[3]);

    mem_shutdown();
    free(pe);
    free(out);
    return 0;
}
