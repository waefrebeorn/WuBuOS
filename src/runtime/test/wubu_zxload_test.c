/* wubu_zxload_test.c -- rung 5: RUN A MODULE BY NAME FROM THE /n NAMESPACE.
 *
 * The namespace loader (wubu_zxload_run) reads a PE at a /n path off g_ns_root
 * and hands it to the module-runner primitive (wubu_zx_run), which runs the
 * whole parse->map->copy->relocate->snap-IAT(registry)->call pipeline. This
 * test drops BOTH image kinds into a real ns root and loads them BY NAME:
 *   A. "answer.exe"  -- .text{ call [rip+disp->IAT]; ret }, .idata importing
 *      NTOSKRNL.EXE!ANSWER (resolved from the registry) -> 7.
 *   B. "prog.bin"    -- a holyd-emitted PROGRAM (module init writes a global,
 *                      image-relative cross-function call, .data section) -> 42.
 * One call, by name, off the namespace — the P2 "/n module loader" seam.
 *
 * Memory-constrained test pattern: links wubu_ns_fs.o (NOT wubu_ns_bridge.o) +
 * wubu_zx/wubu_zxload; g_ns_root set directly.
 */
#include "../../egojar.h"
#include "wubu_zx.h"
#include "wubu_zxload.h"
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

static void nsw(const char *rel, const uint8_t *data, size_t n) {
    char full[512];
    snprintf(full, sizeof(full), "%s/%s", g_ns_root, rel);
    /* mkdir -p parent */
    char dir[512]; strncpy(dir, full, sizeof(dir)); 
    char *sl = strrchr(dir, '/'); if (sl) *sl = 0;
    mkdir(dir, 0755);
    FILE *f = fopen(full, "wb"); fwrite(data, 1, n, f); fclose(f);
}

/* --- rebuild the "answer.exe" PE (call-through-IAT import) --- */
static size_t build_pe_a(uint8_t *f, size_t cap) {
    memset(f, 0, cap); f[0]='M'; f[1]='Z';
    uint32_t pe_off=0x80; memcpy(f+0x3C,&pe_off,4);
    uint8_t *pe=f+pe_off; uint32_t sig=0x00004550; memcpy(pe,&sig,4);
    uint16_t nsects=2,optsize=160; memcpy(pe+6,&nsects,2); memcpy(pe+20,&optsize,2);
    uint8_t *opt=pe+24; uint16_t magic=0x10B; memcpy(opt,&magic,2);
    uint32_t base=0x00400000; memcpy(opt+28,&base,4);
    uint32_t entry=0x1030; memcpy(opt+16,&entry,4);
    uint16_t subsystem=3; memcpy(opt+68,&subsystem,2);
    uint32_t irva=0x2000,isz=0x80; memcpy(opt+104,&irva,4); memcpy(opt+108,&isz,4);
    uint8_t *sect=opt+optsize; uint32_t v;
    memcpy(sect,".text",8); v=0x40; memcpy(sect+8,&v,4); v=0x1000; memcpy(sect+12,&v,4);
    v=0x40; memcpy(sect+16,&v,4); v=0x190; memcpy(sect+20,&v,4);
    memcpy(sect+40,".idata",8); v=0x80; memcpy(sect+48,&v,4); v=0x2000; memcpy(sect+52,&v,4);
    v=0x80; memcpy(sect+56,&v,4); v=0x1D0; memcpy(sect+60,&v,4);
    uint8_t *text=f+0x190; memset(text,0xCC,0x40);
    uint32_t iat_va=0x00400000u+0x2050u, call_rip=0x00400000u+0x1030u+6u;
    int32_t disp=(int32_t)(iat_va-call_rip);
    uint8_t *ep=text+0x30; ep[0]=0xFF; ep[1]=0x15; memcpy(ep+2,&disp,4); ep[6]=0xC3;
    uint8_t *d=f+0x1D0;
    *((uint32_t*)(d+0))=0x2040; *((uint32_t*)(d+12))=0x2030; *((uint32_t*)(d+16))=0x2050;
    strcpy((char*)(d+0x30),"NTOSKRNL.EXE");
    uint64_t *ilt=(void*)(d+0x40); ilt[0]=0x2060; ilt[1]=0;
    uint8_t *ns2=d+0x60; *((uint16_t*)ns2)=0; strcpy((char*)(ns2+2),"ANSWER");
    return 0x1D0+0x80;
}

/* --- emit the holyd program PE (prog.bin) --- */
static int emit_holyd_program(const char *out) {
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
    /* real /n root for this test (memory-constrained: no bridge, set directly) */
    g_ns_root = "/tmp/ns_zxload_test";
    mkdir(g_ns_root, 0755);

    wubu_zx_ops ops = { host_map, host_unmap };

    /* drop both images into the namespace */
    uint8_t fa[2048]; size_t sza = build_pe_a(fa, sizeof(fa));
    nsw("answer.exe", fa, sza);
    CHECK(emit_holyd_program("/tmp/ns_zxload_test/prog.bin") == 0,
          "emitted holyd program PE into the namespace");
    /* confirm both physical files exist under the root */
    struct stat st;
    CHECK(stat("/tmp/ns_zxload_test/answer.exe", &st) == 0, "answer.exe present in /n");
    CHECK(stat("/tmp/ns_zxload_test/prog.bin", &st) == 0, "prog.bin present in /n");

    /* registry: the NT personality would publish here; register ANSWER directly */
    CHECK(wubu_modsym_register("NTOSKRNL.EXE", "ANSWER", (uintptr_t)&host_answer) == 0,
          "registry has NTOSKRNL.EXE!ANSWER");

    /* A: LOAD BY NAME from the namespace -> 7 via registry import */
    int errA = -99;
    int ra = wubu_zxload_run(&ops, "/n/answer.exe", 0x00400000u, NULL, &errA);
    fprintf(stderr, "> [A] /n/answer.exe -> %d (err=%d)\n", ra, errA);
    CHECK(errA == 0 && ra == 7,
          "wubu_zxload: /n/answer.exe (import via registry) -> 7");

    /* B: LOAD BY NAME the holyd program -> 42 (module init + cross-call + .data) */
    int errB = -99;
    int rb = wubu_zxload_run(&ops, "prog.bin", 0x00400000u, NULL, &errB);
    fprintf(stderr, "> [B] prog.bin (holyd program) -> %d (err=%d)\n", rb, errB);
    CHECK(errB == 0 && rb == 42,
          "wubu_zxload: /n/prog.bin (holyd program) -> 42");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
