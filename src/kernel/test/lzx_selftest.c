/*
 * lzx_selftest.c -- validate the kernel wubu_lzx.c decoder against real
 * Halo PC cabinet bytes, walking the REAL CFDATA block list.
 *
 * Oracle: redist/instmsiw.exe (1,822,848 bytes) extracted by host 7z.
 *
 * CAB1.CAB is embedded in halo_pc_trial_setup.exe at file offset 550324.
 * Real structure (parsed + verified against 7z -slt, 2026-08-11):
 *   - CFHEADER at cab+0: cFolders=3, flags=0 (no reserve, no prev/next)
 *   - CFFOLDER table at cab+36: folder[0] coffCabStart=3040 cCFData=595
 *     typeCompress=0x1503 (LZX:21)
 *   - CFDATA blocks are CONTIGUOUS (no padding): 8-byte header
 *     [csum:4][cbData:2][cbUnc:2] followed by cbData compressed bytes;
 *     each block yields cbUnc bytes. cbUnc=32768 for most of folder 0.
 *   - folder[0] uncompressed stream layout (from CFFILE at coffFiles=60):
 *     instmsia.exe @0 (1709160), instmsiw.exe @1709160 (1822848),
 *     msxmlenu.msi @3538008 (5289984), Shfolder.exe, GSArcade.exe.
 *
 * The decoder is called ONCE PER CFDATA BLOCK with that block's compressed
 * bytes (ReactOS fdi.c calling convention). The LZX block state (block
 * type, block_remaining, R0/R1/R2, window, E8 curpos) persists across
 * calls; the bit reader restarts per call.
 */
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

static int verify_slice(const uint8_t *stream, uint32_t off, uint32_t size,
                        const char *path, const char *label) {
    uint32_t osz = 0;
    uint8_t *o = load(path, &osz);
    if (!o) {
        fprintf(stderr, "  [skip] oracle %s not present\n", label);
        return 0;
    }
    if (osz != size) {
        fprintf(stderr, "  FAIL %s: oracle size %u != %u\n", label, osz, size);
        free(o); return 1;
    }
    uint32_t s = 0; while (s < size && stream[off+s] == o[s]) s++;
    free(o);
    if (s == size) { fprintf(stderr, "  ok   %s: %u bytes byte-identical\n", label, size); return 0; }
    fprintf(stderr, "  FAIL %s: first diff at %u of %u\n", label, s, size);
    return 1;
}

int main(void) {
    setbuf(stderr, NULL);
    if (mem_init(256*1024*1024) != 0) { fprintf(stderr, "mem_init fail\n"); return 1; }

    uint32_t exesz;
    uint8_t *exe = load("/home/wubu/wubunos/vendor/games/halo_pc_trial_setup.exe", &exesz);
    if (!exe) {
        fprintf(stderr, "  [skip] vendor/games/halo_pc_trial_setup.exe not present\n");
        mem_shutdown();
        return 0;
    }

    const uint32_t CAB_OFF  = 550324;   /* CAB1.CAB inside the exe */
    const uint32_t FOLDER0  = 3040;     /* folder[0] first CFDATA    */
    const uint32_t NBLOCKS  = 595;      /* folder[0] cCFData         */
    const uint32_t FILE_OFF = 1709160;  /* instmsiw.exe in stream    */
    const uint32_t FILE_SZ  = 1822848;

    struct lzx_state *st = lzx_init(1u << 21);
    if (!st) { fprintf(stderr, "lzx_init fail\n"); return 1; }
    lzx_reset(st);

    /* folder 0 uncompressed total = 19,476,546 bytes (all 5 files) */
    uint8_t *stream = malloc(20u*1024*1024);
    uint32_t have = 0, p = CAB_OFF + FOLDER0;
    int blocks = 0, fails = 0;

    while (blocks < NBLOCKS && p + 8 <= exesz) {
        uint32_t cbData = (uint32_t)exe[p+4] | ((uint32_t)exe[p+5] << 8);
        uint32_t cbUnc  = (uint32_t)exe[p+6] | ((uint32_t)exe[p+7] << 8);
        p += 8;
        if (p + cbData > exesz || have + cbUnc > 20u*1024*1024) {
            fprintf(stderr, "CFDATA overruns input/output\n"); fails = 1; break;
        }
        /* per-CFDATA call: decoder persists LZX block state, restarts
         * the bit reader. Append +4 zero padding (ReactOS requirement). */
        uint8_t *in = malloc(cbData + 4);
        memcpy(in, exe + p, cbData);
        in[cbData] = in[cbData+1] = in[cbData+2] = in[cbData+3] = 0;
        int rc = lzx_decompress(st, in, cbData, stream + have, cbUnc);
        free(in);
        if (rc != LZX_OK) {
            fprintf(stderr, "  FAIL block %d: rc=%d (%s) fail-site=%d cbData=%u cbUnc=%u\n",
                    blocks, rc, lzx_strerror(rc), lzx_debug_fail(st), cbData, cbUnc);
            fails = 1; break;
        }
        have += cbUnc;
        p += cbData;
        blocks++;
    }
    fprintf(stderr, "decoded %d/%d CFDATA blocks, stream=%u bytes\n", blocks, NBLOCKS, have);
    if (blocks != NBLOCKS || have < FILE_OFF + FILE_SZ) fails = 1;

    if (!fails) {
        fails |= verify_slice(stream, 0, 1709160,
                    "/tmp/cab_extract/redist/instmsia.exe", "instmsia.exe @0");
        fails |= verify_slice(stream, FILE_OFF, FILE_SZ,
                    "/tmp/cab_extract/redist/instmsiw.exe", "instmsiw.exe @1709160");
        /* CFFILE uoffFolderStart (coffFiles=60): 3532008 = 1709160+1822848 */
        fails |= verify_slice(stream, 1709160 + 1822848, 5289984,
                    "/tmp/cab_extract/redist/msxmlenu.msi", "msxmlenu.msi @3532008");
        fails |= verify_slice(stream, 8821992, 117288,
                    "/tmp/cab_extract/redist/Shfolder.exe", "Shfolder.exe @8821992");
        fails |= verify_slice(stream, 8939280, 5312429,
                    "/tmp/cab_extract/redist/GSArcade.exe", "GSArcade.exe @8939280");
    }

    lzx_free(st); free(stream); free(exe); mem_shutdown();
    if (!fails) { fprintf(stderr, "=== LZX SELFTEST PASSED (kernel owns the Halo CAB) ===\n"); return 0; }
    fprintf(stderr, "LZX SELFTEST FAILED\n");
    return 1;
}
