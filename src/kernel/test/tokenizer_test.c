/* tokenizer_test.c -- B3: a self-hosted TOKENIZER (a real compiler stage) is
 * emitted from its own holyd-language source file and RUNS through the loader.
 * The compiler reads src/runtime/selfbuild/tokenizer.hd (its own language),
 * emits a PE, and wubu_zx_run runs it -> counts 4 tokens in "int x = 42".
 * This is the holyd-compiles-holyd seed: a compiler sub-stage executing as a
 * holyd program through the WuBuOS loader, not a host harness. */
#include "wubu_zx.h"
#include "wubu_pe.h"
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

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static void *host_map(uintptr_t base, size_t span){
    uint8_t*p=mmap((void*)base,span,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    if(p==MAP_FAILED||p!=(void*)base) return NULL; memset(p,0,span); return p;
}
static void host_unmap(void*im,uintptr_t b,size_t s){munmap(im,s);(void)b;}

static int emit_one(const char*src,const char*out){
    HDLexer lex; hd_lex_init(&lex,src); if(lex.has_error)return-1;
    HDParser parse; hd_parse_init(&parse,&lex);
    HDASTNode*ast=hd_parse_compilation_unit(&parse); if(parse.has_error||!ast)return-2;
    HDGen gen; memset(&gen,0,sizeof gen); hd_gen_init(&gen);
    memset(&gen.symbols,0,sizeof gen.symbols); gen.n_functions=0; gen.n_global_patches=0; gen.n_call_patches=0;
    gen.data=NULL; gen.data_size=0; gen.data_cap=0; emit_prologue(&gen);
    int last_fn_idx=-1;
    if(ast->kind==HD_AST_BLOCK){
        for(int i=0;i<ast->n_stmts;i++){HDASTNode*s=ast->stmts[i]; if(s->kind==HD_AST_FUNC_DECL&&i==ast->n_stmts-1)continue; gen_stmt(&gen,s);}
        HDASTNode*last=ast->stmts[ast->n_stmts-1];
        if(last->kind==HD_AST_FUNC_DECL){ gen_stmt(&gen,last); last_fn_idx=gen.n_functions-1;
            HDFunction*fn=(last_fn_idx>=0)?&gen.functions[last_fn_idx]:NULL;
            if(fn&&fn->func_ptr&&fn->n_params==0){emit_byte(&gen,0xE8);size_t cp=gen.code_size;emit_dword(&gen,0);
                if(gen.n_call_patches<32){gen.call_patches[gen.n_call_patches].code_patch_pos=cp;gen.call_patches[gen.n_call_patches].fn_ptr=NULL;gen.n_call_patches++;}}}
    }
    emit_epilogue(&gen); hd_ast_free(ast);
    if(gen.has_error||gen.code_size==0){fprintf(stderr,"emit err code=%lu\n",(unsigned long)gen.code_size);return-3;}
    size_t nc=(size_t)gen.n_call_patches; size_t*coffs=nc?(size_t*)calloc(nc,sizeof(size_t)):NULL,*cfn=nc?(size_t*)calloc(nc,sizeof(size_t)):NULL;
    for(int i=0;i<gen.n_call_patches;i++){coffs[i]=gen.call_patches[i].code_patch_pos;cfn[i]=(size_t)last_fn_idx;}
    size_t cs=gen.code_size; uint8_t*mod=(uint8_t*)malloc(cs); if(!mod)return-4; memcpy(mod,gen.code,cs);
    size_t ng=0; for(int i=0;i<gen.n_global_patches;i++)if(gen.global_patches[i].code_patch_pos+4<=cs)ng++;
    size_t*mgo=ng?(size_t*)calloc(ng,sizeof(size_t)):NULL,*mgg=ng?(size_t*)calloc(ng,sizeof(size_t)):NULL;
    size_t ni=0; for(int i=0;i<gen.n_global_patches;i++){if(gen.global_patches[i].code_patch_pos+4>cs)continue;
        mgo[ni]=gen.global_patches[i].code_patch_pos;mgg[ni]=gen.global_patches[i].global_offset;ni++;}
    size_t nf=(size_t)gen.n_functions; HDProgramFunc*funcs=nf?(HDProgramFunc*)calloc(nf,sizeof(HDProgramFunc)):NULL;
    for(size_t f=0;f<nf;f++){size_t fcs=gen.functions[f].code_size;uint8_t*fb=(uint8_t*)malloc(fcs?fcs:1);memcpy(fb,gen.functions[f].func_ptr,fcs);
        size_t fg=0; for(int g=0;g<gen.functions[f].n_global_patches;g++)if(gen.functions[f].global_patches[g].code_patch_pos+4<=fcs)fg++;
        size_t*fg0=fg?(size_t*)calloc(fg,sizeof(size_t)):NULL,*fg1=fg?(size_t*)calloc(fg,sizeof(size_t)):NULL; size_t j=0;
        for(int g=0;g<gen.functions[f].n_global_patches;g++){if(gen.functions[f].global_patches[g].code_patch_pos+4>fcs)continue;
            fg0[j]=gen.functions[f].global_patches[g].code_patch_pos;fg1[j]=gen.functions[f].global_patches[g].global_offset;j++;}
        funcs[f].code=fb;funcs[f].code_size=fcs;funcs[f].global_offs=fg0;funcs[f].global_goff=fg1;funcs[f].n_global_patches=fg;}
    uint8_t*data=gen.data_size?(uint8_t*)malloc(gen.data_size):NULL; if(gen.data&&data)memcpy(data,gen.data,gen.data_size);
    int rc=hd_write_pe_program(out,mod,cs,mgo,mgg,ng,coffs,cfn,nc,funcs,nf,data,gen.data_size);
    free(mod);free(mgo);free(mgg);free(coffs);free(cfn);free(data);
    for(size_t i=0;i<nf;i++){free((void*)funcs[i].code);free((void*)funcs[i].global_offs);free((void*)funcs[i].global_goff);}
    free(funcs);free(gen.code);free(gen.data); return rc;
}
static char* read_text(const char*p){FILE*f=fopen(p,"rb");if(!f)return NULL;fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);char*b=(char*)malloc((size_t)n+1);if(!b){fclose(f);return NULL;}if(fread(b,1,(size_t)n,f)!=(size_t)n){free(b);fclose(f);return NULL;}b[n]=0;fclose(f);return b;}

int main(void){
    const char*hdf="src/runtime/selfbuild/tokenizer.hd";
    char*src=read_text(hdf);
    CHECK(src!=NULL,"compiler read its own tokenizer.hd source");
    if(!src)return 1;
    CHECK(strstr(src,"int toks")!=NULL,"tokenizer.hd is a self-hosted lexer");

    CHECK(emit_one(src,"/tmp/tokenizer.bin")==0,"emitted the tokenizer PE");
    free(src);

    wubu_zx_ops ops={host_map,host_unmap};
    FILE*ff=fopen("/tmp/tokenizer.bin","rb"); CHECK(ff!=NULL,"opened emitted tokenizer");
    long n=-1; if(ff){fseek(ff,0,SEEK_END);n=ftell(ff);fseek(ff,0,SEEK_SET);}
    uint8_t*buf=n>0?(uint8_t*)malloc((size_t)n):NULL;
    if(buf&&ff) fread(buf,1,(size_t)n,ff); if(ff)fclose(ff);
    CHECK(buf!=NULL,"read emitted tokenizer");

    int err=-99, r=-1; if(buf) r=wubu_zx_run(&ops,buf,(size_t)n,0x00400000u,NULL,&err);
    fprintf(stderr, "> tokenizer entry r=%d (want 4) err=%d\n", r, err);
    CHECK(err==0 && r==4,"SELF-HOSTED TOKENIZER counted 4 tokens in 'int x = 42' through the loader");
    int e2=-99, r2=-1; if(buf) r2=wubu_zx_run(&ops,buf,(size_t)n,0x00420000u,NULL,&e2);
    CHECK(e2==0 && r2==4,"tokenizer at shifted base 0x00420000 -> 4 (relocation)");
    free(buf);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
