/* wubu_applaunch_test.c -- the session->/n app-launch bridge.
 *
 * Proves the P3/module seam: a session autostart app is STARTED by loading its
 * PE from the /n namespace through the module registry, rather than exec()-ing
 * a host binary. We build a real session boot (wubu_session_boot), install the
 * wubu_applaunch_as_session_launcher with an ops/registry config, point the
 * autostart at a dir containing a .desktop for the holyd program module, and
 * `start_all` — which must scan the entry, claim its bus name, and "launch" it
 * by running /n/prog.bin through wubu_zxreg_load -> the module ran (its result
 * is 42) and the registry holds one reference.
 *
 * Links the P3 session-boot + xdg-autostart + bus with the module subsystem.
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
#include <errno.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s (errno=%d)\n", msg, errno); } } while (0)

static void *host_map(uintptr_t base, size_t span) {
    uint8_t *p = mmap((void *)base, span, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED || p != (void *)base) return NULL;
    memset(p, 0, span);
    return p;
}
static void host_unmap(void *image, uintptr_t base, size_t span) { munmap(image, span); (void)base; }

/* emit the plain holyd program PE into /n -> runs to 42 */
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
    g_ns_root = "/tmp/ns_applaunch";
    mkdir(g_ns_root, 0755);
    mkdir("/tmp/ns_applaunch/apps", 0755);

    /* /n/apps/prog.bin = the holyd program module */
    CHECK(publish_holyd_program("/tmp/ns_applaunch/apps/prog.bin") == 0,
          "published holyd program into /n/apps/prog.bin");

    /* autostart dir with a .desktop whose Exec points at the module */
    const char *asdir = "/tmp/applaunch_autostart";
    mkdir(asdir, 0755);
    {
        FILE *f = fopen("/tmp/applaunch_autostart/prog.desktop", "w");
        if (f) {
            fprintf(f, "[Desktop Entry]\nType=Application\nName=prog\nExec=/n/apps/prog.bin\n");
            fclose(f);
        }
    }
    CHECK(access("/tmp/applaunch_autostart/prog.desktop", R_OK) == 0,
          "autostart .desktop written");

    /* session boot with the appl-aunch launcher */
    wubu_session_boot_t *boot = wubu_session_boot_create();
    CHECK(boot != NULL, "session boot created");
    wubu_zx_ops ops = { host_map, host_unmap };
    wubu_zx_applaunch_cfg cfg = { &ops, NULL, 0x00400000u };
    wubu_session_boot_set_launcher(boot, wubu_applaunch_as_session_launcher, &cfg);
    wubu_zxreg_clear();

    char *dirs[1]; dirs[0] = (char*)asdir;
    int nscan = wubu_session_boot_count(boot);
    int scanned = wubu_session_boot_scan(boot, dirs, 1);
    fprintf(stderr, "> scan count=%d (pre-scan count was %d)\n", scanned, nscan);
    CHECK(scanned >= 1, "session scanned the launchable app");

    int started = wubu_session_boot_start_all(boot);
    fprintf(stderr, "> start_all started=%d registry slots=%d\n", started, wubu_zxreg_count());
    CHECK(started == 1, "session STARTED the app via the /n module launcher");
    CHECK(wubu_zxreg_count() == 1, "registry holds the launched app's module reference");

    wubu_session_boot_destroy(boot);
    wubu_zxreg_clear();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
