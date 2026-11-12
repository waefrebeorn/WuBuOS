/* wubu_selfbuild_test.c -- the stage-0 SELF-BUILD harness.
 *
 * The recursion that makes holyd a pipeline citizen: rather than a hardcoded
 * source string, this test reads a holyd-LANGUAGE source FILE from the repo
 * (src/runtime/selfbuild/hello.hd -- its own language, not C), compiles it
 * through the holyd program emitter, asserts the emitted PE is a valid module,
 * drops it into /n, and runs it through wubu_zxreg_load (the registry loader).
 * The module's entry must come back (g+1 = 42) and the registry holds one
 * reference -- i.e. the compiler now loads+runs a program it built from a file.
 *
 * This is the seed harness: swap hello.hd for any holyd source that parses and
 * the same loop (read file -> compile -> emit -> /n -> register -> run) carries
 * it. Links the compiler (holyd_pe/emitter) with the /n loader + module registry.
 */
#include "../../egojar.h"
#include "wubu_session_boot.h"
#include "wubu_applaunch.h"
#include "wubu_xdg_autostart.h"
#include "wubu_zxreg.h"
#include "wubu_zx.h"
#include "wubu_pe.h"
#include "wubu_ns_bridge_internal.h"
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

static void *host_map(uintptr_t base, size_t span) {
    uint8_t *p = mmap((void *)base, span, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED || p != (void *)base) return NULL;
    memset(p, 0, span);
    return p;
}
static void host_unmap(void *image, uintptr_t base, size_t span) {
    munmap(image, span); (void)base;
}

/* ---- compile a holyd SOURCE FILE through the program emitter ----
 * Mirrors test_pe_program's compile_program: emit module prologue, compile the
 * top-level decls, compile the trailing function, append the module->fn call,
 * emit epilogue, then hd_write_pe_program. Returns 0 on success with the PE at
 * `out`. */
static int emit_holyd_file(const char *hdsrc, const char *out) {
    HDLexer lex; hd_lex_init(&lex, hdsrc);
    if (lex.has_error) { fprintf(stderr, "lex error\n"); return -1; }
    HDParser parse; hd_parse_init(&parse, &lex);
    HDASTNode *ast = hd_parse_compilation_unit(&parse);
    if (parse.has_error || !ast) { fprintf(stderr, "parse error\n"); return -1; }
    HDGen gen; memset(&gen, 0, sizeof gen); hd_gen_init(&gen);
    memset(&gen.symbols, 0, sizeof gen.symbols);
    gen.n_functions = 0; gen.n_global_patches = 0; gen.n_call_patches = 0;
    gen.data = NULL; gen.data_size = 0; gen.data_cap = 0;
    emit_prologue(&gen);
    int last_fn_idx = -1;
    if (ast->kind == HD_AST_BLOCK) {
        for (int i = 0; i < ast->n_stmts; i++) {
            HDASTNode *s = ast->stmts[i];
            if (s->kind == HD_AST_FUNC_DECL && i == ast->n_stmts - 1) continue;
            gen_stmt(&gen, s);
        }
        HDASTNode *last = ast->stmts[ast->n_stmts - 1];
        if (last->kind == HD_AST_FUNC_DECL) {
            gen_stmt(&gen, last);
            last_fn_idx = gen.n_functions - 1;
            HDFunction *fn = (last_fn_idx >= 0) ? &gen.functions[last_fn_idx] : NULL;
            if (fn && fn->func_ptr && fn->n_params == 0) {
                emit_byte(&gen, 0xE8);
                size_t cp = gen.code_size; emit_dword(&gen, 0);
                if (gen.n_call_patches < 32) {
                    gen.call_patches[gen.n_call_patches].code_patch_pos = cp;
                    gen.call_patches[gen.n_call_patches].fn_ptr = NULL;
                    gen.n_call_patches++;
                }
            }
        }
    }
    emit_epilogue(&gen); hd_ast_free(ast);
    if (gen.has_error || gen.code_size == 0) { fprintf(stderr, "emit err\n"); return -1; }

    size_t nc = (size_t)gen.n_call_patches;
    size_t *coffs = nc ? (size_t *)calloc(nc, sizeof(size_t)) : NULL;
    size_t *cfn = nc ? (size_t *)calloc(nc, sizeof(size_t)) : NULL;
    for (int i = 0; i < gen.n_call_patches; i++) {
        coffs[i] = gen.call_patches[i].code_patch_pos;
        cfn[i] = (size_t)last_fn_idx;
    }
    size_t cs = gen.code_size;
    uint8_t *mod = (uint8_t *)malloc(cs);
    if (!mod) return -1;
    memcpy(mod, gen.code, cs);

    size_t ng = 0;
    for (int i = 0; i < gen.n_global_patches; i++)
        if (gen.global_patches[i].code_patch_pos + 4 <= cs) ng++;
    size_t *mgo = ng ? (size_t *)calloc(ng, sizeof(size_t)) : NULL;
    size_t *mgg = ng ? (size_t *)calloc(ng, sizeof(size_t)) : NULL;
    size_t ni = 0;
    for (int i = 0; i < gen.n_global_patches; i++) {
        if (gen.global_patches[i].code_patch_pos + 4 > cs) continue;
        mgo[ni] = gen.global_patches[i].code_patch_pos;
        mgg[ni] = gen.global_patches[i].global_offset; ni++;
    }

    size_t nf = (size_t)gen.n_functions;
    HDProgramFunc *funcs = nf ? (HDProgramFunc *)calloc(nf, sizeof(HDProgramFunc)) : NULL;
    for (size_t f = 0; f < nf; f++) {
        size_t fcs = gen.functions[f].code_size;
        uint8_t *fb = (uint8_t *)malloc(fcs ? fcs : 1);
        memcpy(fb, gen.functions[f].func_ptr, fcs);
        size_t fg = 0;
        for (int g = 0; g < gen.functions[f].n_global_patches; g++)
            if (gen.functions[f].global_patches[g].code_patch_pos + 4 <= fcs) fg++;
        size_t *fg0 = fg ? (size_t *)calloc(fg, sizeof(size_t)) : NULL;
        size_t *fg1 = fg ? (size_t *)calloc(fg, sizeof(size_t)) : NULL;
        size_t j = 0;
        for (int g = 0; g < gen.functions[f].n_global_patches; g++) {
            if (gen.functions[f].global_patches[g].code_patch_pos + 4 > fcs) continue;
            fg0[j] = gen.functions[f].global_patches[g].code_patch_pos;
            fg1[j] = gen.functions[f].global_patches[g].global_offset; j++;
        }
        funcs[f].code = fb; funcs[f].code_size = fcs;
        funcs[f].global_offs = fg0; funcs[f].global_goff = fg1;
        funcs[f].n_global_patches = fg;
    }
    uint8_t *data = gen.data_size ? (uint8_t *)malloc(gen.data_size) : NULL;
    if (gen.data && data) memcpy(data, gen.data, gen.data_size);

    int rc = hd_write_pe_program(out, mod, cs, mgo, mgg, ng,
                                 coffs, cfn, nc, funcs, nf, data, gen.data_size);
    free(mod); free(mgo); free(mgg); free(coffs); free(cfn); free(data);
    for (size_t i = 0; i < nf; i++) {
        free((void *)funcs[i].code);
        free((void *)funcs[i].global_offs);
        free((void *)funcs[i].global_goff);
    }
    free(funcs); free(gen.code); free(gen.data);
    return rc;
}

static char *read_text(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    buf[n] = 0; fclose(f);
    return buf;
}

int main(void) {
    g_ns_root = "/tmp/ns_selfbuild";
    mkdir(g_ns_root, 0755);
    mkdir("/tmp/ns_selfbuild/apps", 0755);

    /* STAGE 0: the compiler reads ITS OWN language file from the repo */
    const char *hd_file = "src/runtime/selfbuild/hello.hd";
    char *src = read_text(hd_file);
    CHECK(src != NULL, "read holyd source file from the repo");
    if (!src) return 1;
    CHECK(strstr(src, "char buf[9]") != NULL,
          "source is holyd's own language (data-array program)");

    /* compile it (emitter) -> /n/apps/hello.bin */
    CHECK(emit_holyd_file(src, "/tmp/ns_selfbuild/apps/hello.bin") == 0,
          "compiled the holyd source file into a /n module PE");
    free(src);

    /* stage 1: the module registry loads + runs it */
    wubu_zx_ops ops = { host_map, host_unmap };
    wubu_zxreg_entry_t *slot = NULL;
    int rc = wubu_zxreg_load(&ops, "apps/hello.bin", 0x00400000u, NULL, &slot);
    CHECK(rc == 72, "registry-loaded module read buf[0] = 'H' = 72 from relocated .data");
    CHECK(slot && slot->valid, "registry holds the loaded module");

    /* stage 2: STRING LITERAL -- char* s="HELLO" survives emit->map->run. */
    wubu_zxreg_entry_t *strl = NULL;
    char *ssrc0 = read_text("src/runtime/selfbuild/str.hd");
    CHECK(ssrc0 != NULL, "read the string-literal holyd source");
    if (ssrc0) {
        CHECK(strstr(ssrc0, "\"HELLO\"") != NULL, "str.hd uses a string literal");
        CHECK(emit_holyd_file(ssrc0, "/tmp/ns_selfbuild/apps/str.bin") == 0,
              "compiled str.hd into a /n module PE");
        free(ssrc0);
        int strc = wubu_zxreg_load(&ops, "apps/str.bin", 0x00450000u, NULL, &strl);
        CHECK(strc == 72, "registry-loaded module read string-literal s[0] = 'H' = 72");
        CHECK(wubu_zxreg_count() == 2, "two self-built modules registered");
    }

    /* stage 3: the RECURSION program -- params + recursion through the loader */
    char *fsrc = read_text("src/runtime/selfbuild/fib.hd");
    wubu_zxreg_entry_t *fslot = NULL;
    CHECK(fsrc != NULL, "read the recursion holyd source");
    if (fsrc) {
        CHECK(strstr(fsrc, "int fib(int n)") != NULL,
              "fib.hd is a parametrized recursive program");
        CHECK(emit_holyd_file(fsrc, "/tmp/ns_selfbuild/apps/fib.bin") == 0,
              "compiled fib.hd into a /n module PE");
        free(fsrc);
        int frc = wubu_zxreg_load(&ops, "apps/fib.bin", 0x00430000u, NULL, &fslot);
        CHECK(frc == 55, "registry-loaded RECURSIVE module computed fib(10) = 55");
        CHECK(wubu_zxreg_count() == 3, "three self-built modules registered");
    }

    /* stage 4: COMPUTED-INDEX data access -- a global array read by a variable
     * index in a loop (sum(1..10)=55), proving the loaded .data is indexable. */
    wubu_zxreg_entry_t *sslot = NULL;
    char *ssrc = read_text("src/runtime/selfbuild/sumbuf.hd");
    CHECK(ssrc != NULL, "read the computed-index holyd source");
    if (ssrc) {
        CHECK(strstr(ssrc, "buf[i]") != NULL,
              "sumbuf.hd indexes a global array by a computed offset");
        CHECK(emit_holyd_file(ssrc, "/tmp/ns_selfbuild/apps/sumbuf.bin") == 0,
              "compiled sumbuf.hd into a /n module PE");
        free(ssrc);
        int src = wubu_zxreg_load(&ops, "apps/sumbuf.bin", 0x00440000u, NULL, &sslot);
        CHECK(src == 55, "registry-loaded module summed buf[i] (computed index) = 55");
        CHECK(wubu_zxreg_count() == 4, "four self-built modules registered");
    }

    /* stage 5: position independence -- the SELF-BUILT PE runs at a second base */
    wubu_zxreg_entry_t *slot2 = NULL;
    int rc2 = wubu_zxreg_load(&ops, "apps/hello.bin", 0x00420000u, NULL, &slot2);
    CHECK(rc2 == 72, "same self-built module runs at base 0x00420000 (relocation-invariant)");
    CHECK(slot2 && slot2->valid && slot2 != slot, "second base got its own registry slot");
    CHECK(wubu_zxreg_count() == 5,
          "five slots coexist: hello@0x400, str@0x450000, fib@0x430000, sumbuf@0x440000, hello@0x420000");

    /* stage 6: ephemeral-app lifecycle -- unload each reference. Unload
     * LAST-registered first: wubu_zxreg_unload COMPACTS the table when a slot
     * hits 0, moving later entries left and invalidating pointers to them. So
     * always free the highest-index (last-loaded) slot first, so compaction
     * never shifts an entry a still-held pointer refers to. */
    wubu_zxreg_unload(slot2);                 /* hello@0x00420000 (idx4, last) */
    CHECK(wubu_zxreg_count() == 4, "unload hello@0x00420000 (count 4)");
    wubu_zxreg_unload(sslot);                 /* sumbuf@0x00440000 (idx3) */
    CHECK(wubu_zxreg_count() == 3, "unload sumbuf@0x00440000 (count 3)");
    wubu_zxreg_unload(fslot);                 /* fib@0x00430000 (idx2) */
    CHECK(wubu_zxreg_count() == 2, "unload fib@0x00430000 (count 2)");
    wubu_zxreg_unload(strl);                  /* str@0x00450000 (idx1) */
    CHECK(wubu_zxreg_count() == 1, "unload str@0x00450000 (count 1)");
    wubu_zxreg_unload(slot);                  /* hello@0x00400000 (idx0, first) */
    CHECK(wubu_zxreg_count() == 0, "unload hello@0x00400000 (count 0)");

    wubu_zxreg_clear();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
