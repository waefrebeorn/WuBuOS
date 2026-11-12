/* wubu_pe_globals_test.c -- self-hosting rung 2: a HOLYD MODULE THAT WRITES A
 * GLOBAL survives the emitter -> kernel-loader round trip.
 *
 * Prior rung (wubu_pe_holyd_test.c) proved a single position-independent holyd
 * expression-blob runs through wubu_pe_*; that blob had NO data section.
 *
 * This rung (the one the holyd_pe.c upgrade serves) proves the .data path:
 *   int g = 40;
 * A top-level VAR_DECL with initializer emits (a) `mov [rip+disp32],rax` (a
 * STORE into the .data section) in gen.code (the module init that RUNS on
 * entry) and (b) reserves the 8-byte slot in gen.data. holograph global access
 * is a RIP-relative operand whose disp32 the emitter must fix against the
 * .data section's REAL RVA, and the kernel loader must map that section so the
 * store lands in mapped memory. Because the reference is RIP-relative, the
 * code<->data distance is invariant across load base: the image must write the
 * SAME value at the preferred base AND at a shifted base, with no .reloc dir.
 *
 * Pipeline: parse+codegen the module through the holyd API -> emit PE via
 * hd_write_pe (carries .text + .data, disp32 fixed against data RVA) ->
 * wubu_pe_parse/sections/map/relocate/call_entry at 0x00400000 and 0x00420000
 * -> then read mapped .data[0..7] and assert it holds 40 in BOTH mappings. */
#include "../../egojar.h"
#include "wubu_pe.h"
#include "holyd.h"
#include "holyd_codegen.h"
#include "holyd_codegen_internal.h"
#include "holyd_parser.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

/* Compile a top-level module: returns a copy of gen.code (module init incl. the
 * global store), the module's data section, and the collected module-level
 * global patches. Mirrors holyd.c run_emit_pe but through the exported API. */
static void *compile_module(const char *src,
                            size_t *out_code_size,
                            size_t *out_patches_n,
                            size_t **out_p_offs, size_t **out_p_globs,
                            uint8_t **out_data, size_t *out_data_size)
{
    HDLexer lex;
    hd_lex_init(&lex, src);
    if (lex.has_error) return NULL;

    HDParser parse;
    hd_parse_init(&parse, &lex);
    HDASTNode *ast = hd_parse_compilation_unit(&parse);
    if (parse.has_error || !ast) return NULL;

    HDGen gen;
    hd_gen_init(&gen);
    emit_prologue(&gen);

    if (ast->kind == HD_AST_BLOCK) {
        for (int i = 0; i < ast->n_stmts; i++) {
            HDASTNode *stmt = ast->stmts[i];
            if (stmt->kind == HD_AST_IF || stmt->kind == HD_AST_WHILE ||
                stmt->kind == HD_AST_FOR || stmt->kind == HD_AST_DO_WHILE ||
                stmt->kind == HD_AST_VAR_DECL || stmt->kind == HD_AST_FUNC_DECL)
                gen_stmt(&gen, stmt);
            else gen_expr(&gen, stmt);
        }
    }
    emit_epilogue(&gen);
    hd_ast_free(ast);
    if (gen.has_error || gen.code_size == 0) return NULL;

    size_t cs = gen.code_size;
    void *code = malloc(cs);
    if (!code) return NULL;
    memcpy(code, gen.code, cs);

    /* module-level global patches (the store disp32 slots) */
    size_t nv = 0;
    for (int i = 0; i < gen.n_global_patches; i++)
        if (gen.global_patches[i].code_patch_pos + 4 <= cs) nv++;
    size_t *offs = nv ? (size_t *)malloc(nv * sizeof(size_t)) : NULL;
    size_t *globs = nv ? (size_t *)malloc(nv * sizeof(size_t)) : NULL;
    size_t n = 0;
    for (int i = 0; i < gen.n_global_patches; i++) {
        if (gen.global_patches[i].code_patch_pos + 4 > cs) continue;
        offs[n] = gen.global_patches[i].code_patch_pos;
        globs[n] = gen.global_patches[i].global_offset;
        n++;
    }
    *out_code_size = cs;
    *out_patches_n = n;
    *out_p_offs = offs; *out_p_globs = globs;
    *out_data = gen.data ? (uint8_t *)malloc(gen.data_size) : NULL;
    *out_data_size = gen.data_size;
    if (gen.data && *out_data) memcpy(*out_data, gen.data, gen.data_size);

    free(gen.code); free(gen.data);
    return code;
}

/* Map a complete PE at base, run the entry, and return the mapped .data[0]
 * (the written global) plus the entry return. */
static int run_and_recover(const char *file, size_t fsize, uintptr_t base,
                           uint64_t *out_global)
{
    wubu_pe_info_t info;
    if (wubu_pe_parse(file, fsize, &info) != 0) return -1;
    wubu_pe_section_t sects[8];
    int ns = wubu_pe_sections(file, fsize, &info, sects, 8);
    if (ns <= 0) return -1;

    uint8_t *img = mmap((void *)base, 0x20000,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (img == MAP_FAILED || img != (void *)base) return -1;
    memset(img, 0, 0x20000);
    for (int i = 0; i < ns; i++) {
        uint8_t *dst = img + sects[i].virtual_addr;
        const uint8_t *src = (const uint8_t *)file + sects[i].raw_ptr;
        memcpy(dst, src, sects[i].raw_size);
    }
    if (wubu_pe_relocate(&info, sects, ns, file, fsize, img, base) != 0) return -1;
    (void)wubu_pe_call_entry(&info, sects, ns, img);

    /* find the .data section: the global slot resides at its virtual_addr.
     * .data is the WRITE | INITIALIZED (non-EXECUTE) section. */
    uint64_t global = 0xFFFFFFFFFFFFFFFFull;
    for (int i = 0; i < ns; i++) {
        if ((sects[i].characteristics & 0x80000000u) &&  /* IMAGE_SCN_MEM_WRITE */
            !(sects[i].characteristics & 0x20000000u)) { /* not EXECUTE */
            memcpy(&global, img + sects[i].virtual_addr, 8);
            break;
        }
    }
    *out_global = global;
    return 0;
}

int main(void) {
    /* top-level module: a global STORE the module init runs on entry. */
    const char *src = "int g = 40;";
    size_t cs = 0, npat = 0, dsz = 0;
    size_t *po = NULL, *pg = NULL;
    uint8_t *data = NULL;
    void *code = compile_module(src, &cs, &npat, &po, &pg, &data, &dsz);
    CHECK(code != NULL, "compiled module with a global");
    CHECK(cs > 0, "module has code");
    CHECK(dsz >= 8, "gen produced a .data section (the global slot)");
    CHECK(npat >= 1, "module records a global store patch");
    fprintf(stderr, "> module=%zu B, data=%zu B, global-patches=%zu\n",
            cs, dsz, npat);

    /* NOTE: we do NOT call the raw copy — its disp32s are unfixed (0), so a
     * store in it would corrupt adjacent code. The EMITTED image through the
     * loader is authoritative: hd_write_pe fixes each disp32 against the real
     * .data RVA and the loader maps that section in. */

    /* emit the PE: carries .text AND .data, global fixed against data RVA */
    char path[] = "/tmp/holyd_globals_pe.bin";
    int we = hd_write_pe(path, (const uint8_t *)code, cs,
                         data, dsz, po, pg, npat);
    CHECK(we == 0, "hd_write_pe emitted a PE with .data");
    free(po); free(pg); free(code); free(data);

    /* load the emitted file */
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL, "reopened emitted PE");
    long sz = -1;
    if (f) { fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET); }
    uint8_t *file = sz > 0 ? (uint8_t *)malloc((size_t)sz) : NULL;
    if (file && f) { if (fread(file, 1, (size_t)sz, f) != (size_t)sz) { free(file); file=NULL; } }
    if (f) fclose(f);
    CHECK(file != NULL && sz > 0, "read emitted PE into memory");

    if (file) {
        uint64_t ga = 0, gb = 0;
        int ra = run_and_recover((const char*)file, (size_t)sz, 0x00400000u, &ga);
        int rb = run_and_recover((const char*)file, (size_t)sz, 0x00420000u, &gb);
        CHECK(ra == 0 && ga == 40,
              "module's global store landed in mapped .data = 40 (preferred base)");
        CHECK(rb == 0 && gb == 40,
              "same store at shifted base = 40 (base-independent, no .reloc)");
        fprintf(stderr, "> mapped .data[0] @0x400000 -> %llu ; @0x420000 -> %llu\n",
                (unsigned long long)ga, (unsigned long long)gb);
        free(file);
    }

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
