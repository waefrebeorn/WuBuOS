/* wubu_pe_program_test.c -- self-hosting rung 3: a holyd PROGRAM (module init
 * that WRITES a global AND calls a second function that reads it) runs through
 * the emitter -> kernel-loader round trip as ONE image.
 *
 * Rungs 1,2 proved (a) a no-data holyd blob and (b) a module STORE survive as
 * .text/.data through wubu_pe_*. This rung adds the two missing pieces that
 * make a real program self-host:
 *   - FUNCTION BODIES live in the same image as the module init (so a call can
 *     cross from module code into a separately-compiled function), and
 *   - the cross-function `call rel32` is made IMAGE-RELATIVE (rewritten by the
 *     emitter to point at the function body's offset in the image), not an
 *     absolute JIT address.
 *
 * Source:   int g = 40;   int f() { return g + 2; }
 * gen.code: prologue; store g=40 (.data); call f(rel32); epilogue(ret)
 * Target:   run at BOTH 0x00400000 and 0x00420000, entry must return 42 --
 *           the module wrote g=40, then f() read it back and added 2, and the
 *           WHOLE data+call chain is position-independent (no .reloc). */
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

/* Compile a MODULE PROGRAM: top-level var stores go to gen.code+.data, and a
 * trailing FUNC_DECL is compiled to its own buffer; we then append a call to it
 * (mirroring hd_eval). Returns the module code, per-function bodies + their
 * global patches, the call patches (module-code-position -> fn index), and the
 * module's data section. */
static int compile_program(const char *src,
                           uint8_t **out_module, size_t *out_module_size,
                           size_t **out_mod_goffs, size_t **out_mod_gglobs, size_t *out_n_mod_globals,
                           size_t **out_call_offs, size_t **out_call_fn, size_t *out_n_calls,
                           HDProgramFunc **out_funcs, size_t *out_n_funcs,
                           uint8_t **out_data, size_t *out_data_size)
{
    HDLexer lex; hd_lex_init(&lex, src);
    if (lex.has_error) return -1;
    HDParser parse; hd_parse_init(&parse, &lex);
    HDASTNode *ast = hd_parse_compilation_unit(&parse);
    if (parse.has_error || !ast) return -1;

    HDGen gen; memset(&gen, 0, sizeof(gen)); hd_gen_init(&gen);
    /* Isolate: hd_gen_init PRESERVES REPL state (data/symbols/functions) across
     * calls by READING gen->data/->symbols before zeroing — if `gen` is
     * uninitialized stack, that read captures garbage/freed pointers and the
     * tail free() double-frees. Zeroing first makes the preserved slices NULL.
     * (The memset above is required; hd_eval is shaped the same but survives
     * only because its stack happens not to collide.) */
    memset(&gen.symbols, 0, sizeof(gen.symbols));
    gen.n_functions = 0;
    gen.n_global_patches = 0;
    gen.n_call_patches = 0;
    gen.data = NULL; gen.data_size = 0; gen.data_cap = 0;
    emit_prologue(&gen);

    /* find the trailing FUNC_DECL (compiled into its own body; we then emit a
     * `call` to it BEFORE the epilogue, mirroring hd_eval). `last_fn_idx` is
     * filled once the function is registered. */
    int last_fn_idx = -1;
    if (ast->kind == HD_AST_BLOCK) {
        for (int i = 0; i < ast->n_stmts; i++) {
            HDASTNode *stmt = ast->stmts[i];
            /* compile all but the trailing function here */
            if (stmt->kind == HD_AST_FUNC_DECL && i == ast->n_stmts - 1) continue;
            if (stmt->kind == HD_AST_IF || stmt->kind == HD_AST_WHILE ||
                stmt->kind == HD_AST_FOR || stmt->kind == HD_AST_DO_WHILE ||
                stmt->kind == HD_AST_VAR_DECL || stmt->kind == HD_AST_FUNC_DECL)
                gen_stmt(&gen, stmt);
            else gen_expr(&gen, stmt);
        }
        /* compile the trailing function, then append `call rel32` to it */
        HDASTNode *last = ast->stmts[ast->n_stmts - 1];
        if (last->kind == HD_AST_FUNC_DECL) {
            /* compiles body into its own buffer AND registers it (n_functions-1) */
            gen_stmt(&gen, last);
            last_fn_idx = gen.n_functions - 1;
            HDFunction *fn = (last_fn_idx >= 0) ? &gen.functions[last_fn_idx] : NULL;
            if (fn && fn->func_ptr && fn->n_params == 0) {
                emit_byte(&gen, 0xE8);       /* call rel32 (appended before epilogue) */
                size_t cp = gen.code_size;
                emit_dword(&gen, 0);
                if (gen.n_call_patches < 32) {
                    gen.call_patches[gen.n_call_patches].code_patch_pos = cp;
                    gen.call_patches[gen.n_call_patches].fn_ptr = NULL;
                    gen.n_call_patches++;
                }
            }
        }
    }
    emit_epilogue(&gen);
    hd_ast_free(ast);
    if (gen.has_error || gen.code_size == 0) return -1;

    /* collect the trailing call's (module-offset, fn_idx) — the emitter needs
     * the target as a FUNCTION INDEX so it can make the call image-relative. */
    size_t n_calls = gen.n_call_patches;
    size_t *coffs  = n_calls ? (size_t *)calloc(n_calls, sizeof(size_t)) : NULL;
    size_t *cfn    = n_calls ? (size_t *)calloc(n_calls, sizeof(size_t)) : NULL;
    for (int i = 0; i < gen.n_call_patches; i++) {
        coffs[i] = gen.call_patches[i].code_patch_pos;
        cfn[i]   = (size_t)last_fn_idx;   /* single trailing call targets it */
    }

    size_t cs = gen.code_size;
    uint8_t *mod = (uint8_t *)malloc(cs);
    if (!mod) return -1;
    memcpy(mod, gen.code, cs);

    /* module global patches */
    size_t ng = 0;
    for (int i = 0; i < gen.n_global_patches; i++)
        if (gen.global_patches[i].code_patch_pos + 4 <= cs) ng++;
    size_t *mgo  = ng ? (size_t *)calloc(ng, sizeof(size_t)) : NULL;
    size_t *mgg  = ng ? (size_t *)calloc(ng, sizeof(size_t)) : NULL;
    size_t ni = 0;
    for (int i = 0; i < gen.n_global_patches; i++) {
        if (gen.global_patches[i].code_patch_pos + 4 > cs) continue;
        mgo[ni] = gen.global_patches[i].code_patch_pos;
        mgg[ni] = gen.global_patches[i].global_offset; ni++;
    }

    /* per-function bodies + their global patches (excluding the module's own) */
    size_t nf = (size_t)gen.n_functions;
    HDProgramFunc *funcs = nf ? (HDProgramFunc *)calloc(nf, sizeof(HDProgramFunc)) : NULL;
    for (int f = 0; f < (int)nf; f++) {
        size_t fcs   = gen.functions[f].code_size;
        uint8_t *fb  = (uint8_t *)malloc(fcs ? fcs : 1);
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

    *out_module=mod; *out_module_size=cs;
    *out_mod_goffs=mgo; *out_mod_gglobs=mgg; *out_n_mod_globals=ni;
    *out_call_offs=coffs; *out_call_fn=cfn; *out_n_calls=n_calls;
    *out_funcs=funcs; *out_n_funcs=nf;
    *out_data=data; *out_data_size=gen.data_size;

    free(gen.code); free(gen.data);
    return 0;
}

static int run_once(const char *file, size_t fsize, uintptr_t base, uint32_t *out)
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
    for (int i = 0; i < ns; i++)
        memcpy(img + sects[i].virtual_addr, (const uint8_t *)file + sects[i].raw_ptr, sects[i].raw_size);
    if (wubu_pe_relocate(&info, sects, ns, file, fsize, img, base) != 0) return -1;
    int rc = wubu_pe_call_entry(&info, sects, ns, img);
    *out = (uint32_t)rc;
    return 0;
}

int main(void) {
    const char *src = "int g = 40;  int f() { return g + 2; }";
    uint8_t *mod=NULL,*data=NULL; size_t msz=0,nd=0;
    size_t *mgo=NULL,*mgg=NULL; size_t ng=0;
    size_t *coff=NULL,*cfn=NULL; size_t nc=0;
    HDProgramFunc *funcs=NULL; size_t nf=0;

    int rc = compile_program(src, &mod,&msz, &mgo,&mgg,&ng, &coff,&cfn,&nc,
                             &funcs,&nf, &data,&nd);
    CHECK(rc == 0, "compiled module program");
    CHECK(msz > 0, "module code present");
    CHECK(nf >= 1, "function body present");
    CHECK(nc >= 1, "module records a call into the function");
    CHECK(ng >= 1, "module records a global store patch");
    CHECK(nd >= 8, "data section present (the global slot)");
    fprintf(stderr, "> module=%zu B funcs=%zu calls=%zu data=%zu B\n",
            msz, nf, nc, nd);

    char path[] = "/tmp/holyd_prog_pe.bin";
    int we = hd_write_pe_program(path,
                                 mod, msz, mgo, mgg, ng,
                                 coff, cfn, nc,
                                 funcs, nf, data, nd);
    CHECK(we == 0, "hd_write_pe_program emitted a single-image PE");
    free(mod); free(mgo); free(mgg); free(coff); free(cfn); free(data);
    for (size_t i=0;i<nf;i++){ free((void*)funcs[i].code); free((void*)funcs[i].global_offs); free((void*)funcs[i].global_goff); }
    free(funcs);

    FILE *f = fopen(path, "rb");
    CHECK(f != NULL, "reopened emitted program PE");
    long sz = -1;
    if (f) { fseek(f,0,SEEK_END); sz=ftell(f); fseek(f,0,SEEK_SET); }
    uint8_t *file = sz>0 ? (uint8_t*)malloc((size_t)sz) : NULL;
    if (file && f) { if (fread(file,1,(size_t)sz,f)!=(size_t)sz){ free(file); file=NULL; } }
    if (f) fclose(f);
    CHECK(file != NULL && sz > 0, "read emitted program PE");

    if (file) {
        uint32_t a=0,b=0;
        int ra = run_once((const char*)file,(size_t)sz,0x00400000u,&a);
        int rb = run_once((const char*)file,(size_t)sz,0x00420000u,&b);
        CHECK(ra==0 && a==42, "program init+cross-function call -> 42 (preferred base)");
        CHECK(rb==0 && b==42, "same program at shifted base -> 42 (image-relative call, no .reloc)");
        fprintf(stderr, "> entry @0x400000 -> %u ; @0x420000 -> %u\n", a, b);
        free(file);
    }

    /* DETERMINISM GATE (self-hosting fixpoint): compile the SAME program a
     * second time from scratch and emit it again; the two PEs must be
     * BYTE-IDENTICAL. holyd has no __TIME__/__DATE__, so its codegen must be
     * a pure function of the source (deterministic jit_alloc_exec layout,
     * patch values, section layout). Two independent runs producing different
     * files would make the stage0->stage1->... bootstrap non-reproducible;
     * bit-stability is what lets a later self-build collide to a hash. */
    {
        uint8_t *m2=NULL,*d2=NULL; size_t m2sz=0,nd2=0;
        size_t *go2=NULL,*gg2=NULL; size_t ng2=0;
        size_t *co2=NULL,*cf2=NULL; size_t nc2=0;
        HDProgramFunc *fu2=NULL; size_t nf2=0;
        int rc2 = compile_program(src, &m2,&m2sz, &go2,&gg2,&ng2, &co2,&cf2,&nc2,
                                  &fu2,&nf2, &d2,&nd2);
        CHECK(rc2 == 0, "determinism: recompiled the source fresh");
        char path2[] = "/tmp/holyd_prog_pe2.bin";
        CHECK(hd_write_pe_program(path2, m2,m2sz, go2,gg2,ng2, co2,cf2,nc2,
                                  fu2,nf2, d2,nd2) == 0, "determinism: re-emitted a second PE");
        free(m2); free(go2); free(gg2); free(co2); free(cf2); free(d2);
        for (size_t i=0;i<nf2;i++){ free((void*)fu2[i].code); free((void*)fu2[i].global_offs); free((void*)fu2[i].global_goff); }
        free(fu2);

        FILE *f1=fopen(path,"rb"), *f2=fopen(path2,"rb");
        long s1=-1,s2=-1;
        if (f1){fseek(f1,0,SEEK_END);s1=ftell(f1);fseek(f1,0,SEEK_SET);}
        if (f2){fseek(f2,0,SEEK_END);s2=ftell(f2);fseek(f2,0,SEEK_SET);}
        int same = (s1>0 && s1==s2);
        if (same) {
            uint8_t *b1=(uint8_t*)malloc((size_t)s1), *b2=(uint8_t*)malloc((size_t)s1);
            if (b1&&b2) {
                size_t r1=fread(b1,1,(size_t)s1,f1), r2=fread(b2,1,(size_t)s1,f2);
                same = (r1==(size_t)s1 && r2==(size_t)s1 && memcmp(b1,b2,(size_t)s1)==0);
            }
            free(b1);free(b2);
        }
        if (f1)fclose(f1); if (f2)fclose(f2);
        CHECK(same, "determinism: two independent emits are byte-identical (fixpoint)");
        fprintf(stderr, "> determinism: file1=%ldB file2=%ldB %s\n",
                s1, s2, same?"IDENTICAL":"DIFFER");
    }

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
