/*
 * wubu_pe_personality_test.c -- the in-kernel Win32 personality audit.
 *
 * Proves: for EVERY DLL the real game binaries import (parsed by
 * wubu_pe), the kernel's personality answers it. This is the gate
 * that closes the "host Wine" crutch — the kernel owns the DLLs.
 *
 * The two real binaries:
 *   openarena.exe  → 10 DLLs (msvcrt/open_gl32/sdl/winmm/ws2_32/...)
 *   halo_pc_trial  → 10 DLLs (kernel32/user32/winmm/gdi32/...)
 *   together → 15 distinct DLLs, all of which the kernel must answer.
 */
#include "wubu_pe.h"
#include "wubu_pe_personality.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

static uint8_t *load(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(sz); if (!b) { fclose(f); return NULL; }
    fread(b, 1, sz, f); fclose(f); *size = sz; return b;
}

static int collect_imports(const char *path, char out[32][64], int *n)
{
    size_t size = 0;
    uint8_t *data = load(path, &size);
    if (!data) return 0;
    wubu_pe_info_t info;
    if (wubu_pe_parse(data, size, &info) != 0) { free(data); return 0; }
    wubu_pe_section_t sects[16];
    int ns = wubu_pe_sections(data, size, &info, sects, 16);
    if (ns < 1) { free(data); return 0; }
    wubu_pe_import_t imports[32];
    int ni = wubu_pe_imports(data, size, &info, sects, ns, imports, 32);
    if (ni < 0) { free(data); return 0; }
    for (int i = 0; i < ni && *n < 32; i++) {
        strncpy(out[*n], imports[i].dll, 63);
        out[*n][63] = '\0';
        (*n)++;
    }
    free(data);
    return 1;
}

int main(void)
{
    printf("=== wubu_pe_personality_test (the kernel answers the real DLLs) ===\n");
    char dlls[32][64]; int n = 0;
    if (!collect_imports("vendor/games/openarena_extract/openarena-0.8.8/openarena.exe",
                         dlls, &n))
        FAIL("load openarena.exe");
    int n1 = n;
    if (!collect_imports("vendor/games/halo_pc_trial_setup.exe", dlls, &n))
        FAIL("load halo_pc");
    printf("  real import count: %d (openarena=%d + halo=%d)\n",
           n, n1, n - n1);

    printf("  kernel answers: %d DLLs\n", wubu_pe_personality_count());
    int answered = 0, missing = 0;
    for (int i = 0; i < n; i++) {
        const wubu_pe_personality_t *p =
            wubu_pe_personality_answer(dlls[i]);
        if (p) { answered++; printf("  [OK]   %s\n", dlls[i]); }
        else   { missing++;   printf("  [MISS] %s\n", dlls[i]); }
    }
    printf("  answered=%d missing=%d\n", answered, missing);
    if (missing != 0) FAIL("the kernel does not answer %d DLLs", missing);

    printf("=== ALL PERSONALITY TESTS PASSED (the kernel owns the Win32 DLLs) ===\n");
    return 0;
}
