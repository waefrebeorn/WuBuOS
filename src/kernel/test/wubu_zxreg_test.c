/* wubu_zxreg_test.c -- module registry (ZXE-style) semantics.
 *
 * Builds on rungs 4-6 (wubu_zx_run + wubu_zxload + import emitter) and proves
 * the registry invariants a ".ZXE loader on /n" needs:
 *   1. LOAD: run a module by name from the /n root -> entry result (7).
 *   2. DEDUP: loading the SAME module again does not re-map/run it — the slot
 *      is reused, refcount bumps, and the cached result returns (7 again).
 *   3. COUNT: registry row count is 1, not 2 (one mapping, two refs).
 *   4. UNLOAD once -> refcount drops to 1, slot still valid.
 *   5. UNLOAD again -> refcount 0, slot freed, count 0.
 *
 * Uses a real ns root (g_ns_root set directly; wubu_ns_fs.o, not the bridge).
 */
#include "wubu_zxreg.h"
#include "wubu_zx.h"
#include "wubu_modsym.h"
#include "wubu_ns_bridge_internal.h"
#include "holyd.h"
#include "holyd_codegen.h"
#include "holyd_codegen_internal.h"
#include "holyd_parser.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>

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
static void host_unmap(void *image, uintptr_t base, size_t span) { munmap(image, span); (void)base; }
static int host_answer(void) { return 7; }

/* write the one-import PE (NTOSKRNL.EXE!ANSWER) the emitter produces, into /n */
static int publish_import_module(const char *out) {
    uint8_t code[12];
    code[0]=0xFF; code[1]=0x15; code[2]=0; code[3]=0; code[4]=0; code[5]=0; code[6]=0xC3;
    size_t disp[1]={2}; const char*dll="NTOSKRNL.EXE"; const char*fn[1]={"ANSWER"};
    return hd_write_pe_imported(out, code, 7, disp, 1, dll, fn, 1, NULL);
}

/* emit the plain holyd program PE (module init writes a global + a cross-
 * function call reading it, .data section; no imports) -> 42 */
static int publish_holyd_program(const char *out) {
    const char *src = "int g = 40;  int f() { return g + 2; }";
    HDLexer lex; hd_lex_init(&lex, src);
    if (lex.has_error) return -1;
    HDParser parse; hd_parse_init(&parse, &lex);
    HDASTNode *ast = hd_parse_compilation_unit(&parse);
    if (parse.has_error || !ast) return -1;
    HDGen gen; memset(&gen,0,sizeof gen); hd_gen_init(&gen);
    memset(&gen.symbols,0,sizeof gen.symbols);
    gen.n_functions=0; gen.n_global_patches=0; gen.n_call_patches=0;
    gen.data=NULL; gen.data_size=0; gen.data_cap=0;
    emit_prologue(&gen);
    int last_fn_idx=-1;
    if (ast->kind==HD_AST_BLOCK){
        for(int i=0;i<ast->n_stmts;i++){
            HDASTNode *s=ast->stmts[i];
            if(s->kind==HD_AST_FUNC_DECL && i==ast->n_stmts-1) continue;
            if(s->kind==HD_AST_IF||s->kind==HD_AST_WHILE||s->kind==HD_AST_FOR||
               s->kind==HD_AST_DO_WHILE||s->kind==HD_AST_VAR_DECL||s->kind==HD_AST_FUNC_DECL)
                gen_stmt(&gen,s); else gen_expr(&gen,s);
        }
        HDASTNode *last=ast->stmts[ast->n_stmts-1];
        if(last->kind==HD_AST_FUNC_DECL){
            gen_stmt(&gen,last); last_fn_idx=gen.n_functions-1;
            HDFunction *fn=(last_fn_idx>=0)?&gen.functions[last_fn_idx]:NULL;
            if(fn&&fn->func_ptr&&fn->n_params==0){
                emit_byte(&gen,0xE8); size_t cp=gen.code_size; emit_dword(&gen,0);
                if(gen.n_call_patches<32){ gen.call_patches[gen.n_call_patches].code_patch_pos=cp;
                    gen.call_patches[gen.n_call_patches].fn_ptr=NULL; gen.n_call_patches++; }
            }
        }
    }
    emit_epilogue(&gen); hd_ast_free(ast);
    if(gen.has_error||gen.code_size==0) return -1;
    size_t nc=(size_t)gen.n_call_patches;
    size_t *coffs=nc?(size_t*)calloc(nc,sizeof(size_t)):NULL, *cfn=nc?(size_t*)calloc(nc,sizeof(size_t)):NULL;
    for(int i=0;i<gen.n_call_patches;i++){ coffs[i]=gen.call_patches[i].code_patch_pos; cfn[i]=(size_t)last_fn_idx; }
    size_t cs=gen.code_size; uint8_t *mod=(uint8_t*)malloc(cs); if(!mod) return -1;
    memcpy(mod,gen.code,cs);
    size_t ng=0; for(int i=0;i<gen.n_global_patches;i++) if(gen.global_patches[i].code_patch_pos+4<=cs) ng++;
    size_t *mgo=ng?(size_t*)calloc(ng,sizeof(size_t)):NULL, *mgg=ng?(size_t*)calloc(ng,sizeof(size_t)):NULL;
    size_t ni=0; for(int i=0;i<gen.n_global_patches;i++){ if(gen.global_patches[i].code_patch_pos+4>cs) continue;
        mgo[ni]=gen.global_patches[i].code_patch_pos; mgg[ni]=gen.global_patches[i].global_offset; ni++; }
    size_t nf=(size_t)gen.n_functions;
    HDProgramFunc *funcs=nf?(HDProgramFunc*)calloc(nf,sizeof(HDProgramFunc)):NULL;
    for(size_t f=0;f<nf;f++){
        size_t fcs=gen.functions[f].code_size; uint8_t *fb=(uint8_t*)malloc(fcs?fcs:1);
        memcpy(fb,gen.functions[f].func_ptr,fcs);
        size_t fg=0; for(int g=0;g<gen.functions[f].n_global_patches;g++) if(gen.functions[f].global_patches[g].code_patch_pos+4<=fcs) fg++;
        size_t *fg0=fg?(size_t*)calloc(fg,sizeof(size_t)):NULL,*fg1=fg?(size_t*)calloc(fg,sizeof(size_t)):NULL;
        size_t j=0; for(int g=0;g<gen.functions[f].n_global_patches;g++){ if(gen.functions[f].global_patches[g].code_patch_pos+4>fcs) continue;
            fg0[j]=gen.functions[f].global_patches[g].code_patch_pos; fg1[j]=gen.functions[f].global_patches[g].global_offset; j++; }
        funcs[f].code=fb; funcs[f].code_size=fcs; funcs[f].global_offs=fg0; funcs[f].global_goff=fg1; funcs[f].n_global_patches=fg;
    }
    uint8_t *data=gen.data_size?(uint8_t*)malloc(gen.data_size):NULL;
    if(gen.data&&data) memcpy(data,gen.data,gen.data_size);
    int rc=hd_write_pe_program(out,mod,cs,mgo,mgg,ng,coffs,cfn,nc,funcs,nf,data,gen.data_size);
    free(mod);free(mgo);free(mgg);free(coffs);free(cfn);free(data);
    for(size_t i=0;i<nf;i++){ free((void*)funcs[i].code); free((void*)funcs[i].global_offs); free((void*)funcs[i].global_goff); }
    free(funcs); free(gen.code); free(gen.data);
    return rc;
}

int main(void) {
    g_ns_root = "/tmp/ns_zxreg_test";
    mkdir(g_ns_root, 0755);
    wubu_zx_ops ops = { host_map, host_unmap };
    wubu_zxreg_clear();

    /* publish the module into /n */
    CHECK(publish_import_module("/tmp/ns_zxreg_test/mod.bin") == 0,
          "published import module into /n");
    CHECK(wubu_modsym_register("NTOSKRNL.EXE","ANSWER",(uintptr_t)&host_answer)==0,
          "registry has NTOSKRNL.EXE!ANSWER");

    /* 1. load by name -> 7 */
    wubu_zxreg_entry_t *e = NULL;
    int r1 = wubu_zxreg_load(&ops, "/n/mod.bin", 0x00400000u, NULL, &e);
    fprintf(stderr, "> load1 -> %d (entry=%p refcount=%d)\n", r1, (void*)e, e?e->refcount:-1);
    CHECK(r1 == 7 && e && e->refcount == 1, "load #1 runs module -> 7, refcount 1");

    /* 2. dedup: load the SAME module again */
    wubu_zxreg_entry_t *e2 = NULL;
    int r2 = wubu_zxreg_load(&ops, "/n/mod.bin", 0x00400000u, NULL, &e2);
    fprintf(stderr, "> load2 (dedup) -> %d (same=%d refcount=%d count=%d)\n",
            r2, e2==e, e2?e2->refcount:-1, wubu_zxreg_count());
    CHECK(r2 == 7 && e2 == e && e->refcount == 2, "load #2 dedups -> cached 7, refcount 2");
    CHECK(wubu_zxreg_count() == 1, "one mapping for two refs (registry count == 1)");

    /* 3. unload once -> refcount 1, slot still valid */
    wubu_zxreg_unload(e);
    fprintf(stderr, "> unload1 refcount=%d count=%d\n", e->refcount, wubu_zxreg_count());
    CHECK(e->valid && e->refcount == 1 && wubu_zxreg_count() == 1,
          "unload #1 decrements to 1, slot stays");

    /* 4. unload again -> freed, count 0 */
    wubu_zxreg_unload(e);
    fprintf(stderr, "> unload2 refcount=%d count=%d\n", e->refcount, wubu_zxreg_count());
    CHECK(wubu_zxreg_count() == 0, "unload #2 releases slot, count 0");

    /* ---- MULTI-MODULE: two DISTINCT modules, one registry ---- */
    /* publish a second, differently-behaving module: the plain holyd program
     * (module init writes a global + a cross-function call, .data; no imports) */
    wubu_zxreg_clear();
    CHECK(publish_holyd_program("/tmp/ns_zxreg_test/prog.bin") == 0,
          "published second (holyd program) module into /n");
    CHECK(publish_import_module("/tmp/ns_zxreg_test/mod2.bin") == 0,
          "published import module #2 into /n");

    wubu_zxreg_entry_t *ep = NULL, *em = NULL;
    int rp = wubu_zxreg_load(&ops, "/n/prog.bin", 0x00400000u, NULL, &ep);
    int rm = wubu_zxreg_load(&ops, "/n/mod2.bin", 0x00410000u, NULL, &em);
    fprintf(stderr, "> multi: prog=%d@%p mod2=%d@%p count=%d\n",
            rp, (void*)ep, rm, (void*)em, wubu_zxreg_count());
    CHECK(rp == 42 && rm == 7, "multi-module: holyd program -> 42, import module -> 7");
    CHECK(ep && em && ep != em, "two DISTINCT registry slots");
    CHECK(wubu_zxreg_count() == 2, "two modules registered, count == 2");

    /* dedup WITHIN each module, not across: reload prog -> same slot */
    wubu_zxreg_entry_t *ep2 = NULL;
    int rp2 = wubu_zxreg_load(&ops, "/n/prog.bin", 0x00400000u, NULL, &ep2);
    fprintf(stderr, "> multi dedup prog: ->%d same=%d count=%d\n", rp2, ep2==ep, wubu_zxreg_count());
    CHECK(rp2 == 42 && ep2 == ep && ep->refcount == 2 && wubu_zxreg_count() == 2,
          "dedup is per-module (reload prog reuses its slot, count stays 2)");

    /* unload the import module -> its slot frees, prog stays */
    wubu_zxreg_unload(em);
    fprintf(stderr, "> multi unload mod2: count=%d\n", wubu_zxreg_count());
    CHECK(wubu_zxreg_count() == 1 && ep->valid, "unloading module #2 leaves #1 resident");

    /* unload prog twice -> registry empty */
    wubu_zxreg_unload(ep);
    wubu_zxreg_unload(ep);
    fprintf(stderr, "> multi unload prog x2: count=%d\n", wubu_zxreg_count());
    CHECK(wubu_zxreg_count() == 0, "both modules released, registry empty");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
