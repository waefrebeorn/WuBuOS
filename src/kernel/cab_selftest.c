/*
 * cab_selftest.c -- exercise the IN-KERNEL CAB reader (wubu_cab.c)
 * against the real Halo PC CAB1.CAB embedded in halo_pc_trial_setup.exe
 * at file offset 550324.
 *
 * Oracle: host-7z-extracted redist/instmsiw.exe (1,822,848 bytes).
 * The reader must open the CAB, find the file by name, and extract it
 * byte-identical -- all in the kernel's own code (LZX + CAB walking),
 * no host tools.
 */
#include "wubu_cab.h"
#include "wubu_lzx.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *load(const char *p, uint32_t *sz) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n); fread(b, 1, n, f); fclose(f);
    *sz = (uint32_t)n; return b;
}

int main(void) {
    setbuf(stderr, NULL);
    if (mem_init(256*1024*1024) != 0) { fprintf(stderr, "mem_init fail\n"); return 1; }

    uint32_t exesz, osz;
    uint8_t *exe = load("/home/wubu/wubunos/vendor/games/halo_pc_trial_setup.exe", &exesz);
    uint8_t *oracle = load("/tmp/cab_extract/redist/instmsiw.exe", &osz);
    if (!exe) {
        fprintf(stderr, "  [skip] vendor/games/halo_pc_trial_setup.exe not present\n");
        mem_shutdown();
        return 0;
    }
    if (!oracle) {
        fprintf(stderr, "  [skip] /tmp/cab_extract/redist/instmsiw.exe (host-7z oracle) not present\n");
        free(exe);
        mem_shutdown();
        return 0;
    }

    const uint32_t CAB_OFF = 550324;   /* embedded CAB1.CAB */
    cab_archive cab;
    int fails = 0;

    if (cab_open(&cab, exe + CAB_OFF, exesz - CAB_OFF) != 0) {
        fprintf(stderr, "FAIL: cab_open\n"); fails = 1;
    } else {
        fprintf(stderr, "  ok   cab_open: %u folders, %u files\n",
                cab.c_folders, cab.c_files);

        /* find by full name and by suffix */
        int idx = cab_find(&cab, "redist\\instmsiw.exe");
        int idx2 = cab_find(&cab, "instmsiw.exe");
        int miss = cab_find(&cab, "nonexistent.exe");
        if (idx != 1 || idx2 != 1) { fprintf(stderr, "FAIL: cab_find idx=%d/%d\n", idx, idx2); fails = 1; }
        else fprintf(stderr, "  ok   cab_find: instmsiw.exe -> CFFILE[%d] (missing -> %d)\n", idx, miss);
        if (miss != -1) { fprintf(stderr, "FAIL: cab_find missing hit %d\n", miss); fails = 1; }

        /* extract */
        uint8_t *out = malloc(osz);
        int32_t n = cab_extract(&cab, idx, out, (uint32_t)osz);
        if (n != (int32_t)osz) {
            fprintf(stderr, "FAIL: cab_extract n=%d want %u\n", n, osz); fails = 1;
        } else if (memcmp(out, oracle, osz) != 0) {
            fprintf(stderr, "FAIL: cab_extract bytes differ\n"); fails = 1;
        } else {
            fprintf(stderr, "  ok   cab_extract: instmsiw.exe %u bytes byte-identical\n", osz);
        }
        free(out);

        /* extract every file in folder 0 and verify against oracles */
        static const struct { const char *name; const char *oracle; } t[] = {
            { "instmsia.exe", "/tmp/cab_extract/redist/instmsia.exe" },
            { "msxmlenu.msi", "/tmp/cab_extract/redist/msxmlenu.msi" },
            { "Shfolder.exe", "/tmp/cab_extract/redist/Shfolder.exe" },
            { "GSArcade.exe", "/tmp/cab_extract/redist/GSArcade.exe" },
        };
        for (size_t i = 0; i < sizeof(t)/sizeof(t[0]); i++) {
            int fi = cab_find(&cab, t[i].name);
            if (fi < 0) { fprintf(stderr, "FAIL: find %s\n", t[i].name); fails = 1; continue; }
            uint32_t oo = 0;
            uint8_t *o = load(t[i].oracle, &oo);
            if (!o) { fprintf(stderr, "  [skip] oracle %s\n", t[i].oracle); continue; }
            uint8_t *b = malloc(oo);
            int32_t en = cab_extract(&cab, fi, b, oo);
            if (en != (int32_t)oo || memcmp(b, o, oo) != 0) {
                fprintf(stderr, "FAIL: extract %s (n=%d)\n", t[i].name, en); fails = 1;
            } else {
                fprintf(stderr, "  ok   %s: %u bytes byte-identical\n", t[i].name, oo);
            }
            free(b); free(o);
        }

        /* robustness: truncated CAB must be rejected, not crash */
        uint8_t *trunc = malloc(4096);
        memcpy(trunc, exe + CAB_OFF, 4096);
        cab_archive t2;
        if (cab_open(&t2, trunc, 4096) == 0) {
            fprintf(stderr, "FAIL: truncated CAB accepted\n"); fails = 1;
        } else {
            fprintf(stderr, "  ok   truncated CAB rejected\n");
        }
        free(trunc);

        cab_close(&cab);
    }

    free(exe); free(oracle); mem_shutdown();
    if (!fails) { fprintf(stderr, "=== CAB SELFTEST PASSED (kernel reads the Halo CAB) ===\n"); return 0; }
    fprintf(stderr, "CAB SELFTEST FAILED\n");
    return 1;
}
