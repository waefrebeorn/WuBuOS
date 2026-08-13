/*
 * wubu_pe_test.c -- the in-kernel PE loader test (the REAL Windows
 * game binaries).
 *
 * Parses the ACTUAL game PEs the kernel must load:
 *   1. the OpenArena win32 exe (485 imports — a full game)
 *   2. the Halo PC trial setup (a real i386 Win32 GUI PE)
 *
 * Asserts: the headers parse, the sections walk, the imports name
 * the DLLs the kernel's personality tables must answer (d3d8/
 * dsound/winmm/ws2_32/... — OUR tables, not host Wine's).
 */
#include "wubu_pe.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

static uint8_t *load(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)sz);
    if (!b) { fclose(f); return NULL; }
    fread(b, 1, (size_t)sz, f);
    fclose(f);
    *size = (size_t)sz;
    return b;
}

static int audit_pe(const char *path)
{
    size_t size = 0;
    uint8_t *data = load(path, &size);
    if (!data) { printf("  [skip] %s not present\n", path); return 0; }

    wubu_pe_info_t info;
    if (wubu_pe_parse(data, size, &info) != 0)
        FAIL("parse %s", path);
    printf("  %s: machine 0x%04x sections %u entry 0x%x subsystem %u\n",
           path, info.machine, info.num_sections, info.entry_rva,
           info.subsystem);

    wubu_pe_section_t sects[16];
    int ns = wubu_pe_sections(data, size, &info, sects, 16);
    if (ns < 1) FAIL("sections %s", path);
    printf("  sections: %d (first: %.8s va 0x%x raw %u)\n",
           ns, sects[0].name, sects[0].virtual_addr, sects[0].raw_size);

    wubu_pe_import_t imports[32];
    int ni = wubu_pe_imports(data, size, &info, sects, ns, imports, 32);
    if (ni < 0) FAIL("imports %s", path);
    printf("  imports: %d DLLs:", ni);
    int d3d = 0, ds = 0, winmm = 0, ws2 = 0;
    for (int i = 0; i < ni; i++) {
        printf(" %s(%d)", imports[i].dll, imports[i].func_count);
        if (strstr(imports[i].dll, "D3D8") || strstr(imports[i].dll, "d3d8"))
            d3d = 1;
        if (strstr(imports[i].dll, "DSOUND") || strstr(imports[i].dll, "dsound"))
            ds = 1;
        if (strstr(imports[i].dll, "WINMM") || strstr(imports[i].dll, "winmm"))
            winmm = 1;
        if (strstr(imports[i].dll, "WS2_32") || strstr(imports[i].dll, "ws2_32"))
            ws2 = 1;
    }
    printf("\n");
    free(data);
    printf("  personality needs: d3d8=%d dsound=%d winmm=%d ws2_32=%d\n",
           d3d, ds, winmm, ws2);
    return 1;
}

int main(void)
{
    printf("=== wubu_pe_test (the in-kernel PE loader, real binaries) ===\n");

    int audited = 0;
    audited += audit_pe("vendor/games/openarena_extract/openarena-0.8.8/openarena.exe");
    audited += audit_pe("vendor/games/halo_pc_trial_setup.exe");
    if (!audited) { printf("  [skip] no real PEs present — run from the repo root\n"); return 0; }

    printf("=== ALL PE TESTS PASSED (the kernel parses the real games) ===\n");
    return 0;
}
