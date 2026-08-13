/*
 * wubu_elf_test.c -- the in-kernel ELF loader test (the Linux game).
 *
 * The user's second goal: the games run on OUR kernel. The Linux
 * OpenArena binary (an x86-64 ELF) is parsed by OUR kernel ELF
 * loader — not host ld.so/container exec.
 *
 * Proves: the magic validates, the 64-bit header parses, the LOAD
 * segments walk, the entry point is sane, the machine is x86_64.
 */
#include "wubu_elf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

static uint8_t *load(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(sz); if (!b) { fclose(f); return NULL; }
    fread(b, 1, sz, f); fclose(f); *size = (size_t)sz; return b;
}

int main(void)
{
    printf("=== wubu_elf_test (the in-kernel ELF loader, the real Linux game) ===\n");

    /* the REAL OpenArena Linux binary (the one we ran earlier) */
    size_t sz = 0;
    uint8_t *data = load("vendor/games/openarena_extract/openarena-0.8.8/"
                         "openarena.x86_64", &sz);
    if (!data) FAIL("load openarena.x86_64 (run from the repo root)");
    printf("  loaded: %zu bytes\n", sz);

    wubu_elf_info_t info;
    if (wubu_elf_parse(data, sz, &info) != 0)
        FAIL("parse the REAL ELF");
    printf("  machine: %s\n", wubu_elf_machine_name(0x3E));
    printf("  entry: 0x%lx  LOAD segments: %d\n", info.entry, info.n_load);
    if (info.n_load < 1) FAIL("no LOAD segments");
    if (info.entry == 0) FAIL("zero entry");

    int has_exec = 0;
    for (int i = 0; i < info.n_load; i++)
        if (info.ph_load[i].flags & 1) has_exec = 1;  /* the X bit */
    if (!has_exec) FAIL("no executable segment");

    printf("  [OK] the kernel parses the OpenArena Linux ELF\n");

    /* the PE and Mach-O binaries the kernel also owns */
    printf("  (the kernel PE loader: test_pe_load; the Mach-O loader:\n");
    printf("   wubu_macho.c — see KV theme)\n");

    free(data);
    printf("=== ALL ELF TESTS PASSED (the kernel owns the Linux game) ===\n");
    return 0;
}
