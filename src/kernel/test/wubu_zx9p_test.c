/* wubu_zx9p_test.c -- THE loader consumes a module whose bytes arrive over REAL
 * Styx/9P, not stdio.
 *
 * This is the kernel-leg TRANSPORT of DA#3: until now every green load read the
 * module from a flat dir via fopen/g_ns_root (stdio). Here the module bytes are
 * WALKed + OPENed + READ over a genuine 9P server (in-process styxfs mounted on
 * a host dir) -- the SAME transport an on-kernel 9P client would use -- and then
 * run through wubu_zx_run. Proves the PE/loader stack is transport-agnostic:
 * give it bytes from /n-over-9P and it relocates/resolves-imports/runs them.
 *
 * Reuses build_pe_a (wubu_zx_test.c) for a minimal import-resolving PE and the
 * cli_* 9P client (wubu_ns_9p_test.c). No compiler link needed.
 */
#include "wubu_zx.h"
#include "wubu_pe.h"
#include "wubu_modsym.h"
#include "styxfs.h"
#include "styx.h"
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

/* ---- build_pe_a (from wubu_zx_test.c): .text{FF 15 <disp> C3}->IAT ;
 *      .idata NTOSKRNL.EXE!ANSWER ; entry RVA 0x1030 ; base 0x00400000 */
static size_t build_pe_a(uint8_t *f, size_t cap) {
    memset(f, 0, cap);
    f[0]='M'; f[1]='Z';
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

/* --- tiny in-process 9P client (from wubu_ns_9p_test.c) --- */
static styxfs_server_t g_srv_mem;
static styxfs_server_t *g_srv;
static int cli_version(void){uint8_t in[STYX_MAX_MSG],out[STYX_MAX_MSG];uint32_t il,ol;styx_build_tversion(in,&il,8192,"9P2000");return styxfs_serve(g_srv,in,il,out,&ol);}
static int cli_attach(uint32_t fid){uint8_t in[STYX_MAX_MSG],out[STYX_MAX_MSG];uint32_t il,ol;styx_build_tattach(in,&il,1,fid,0,"/");return styxfs_serve(g_srv,in,il,out,&ol);}
static int cli_walk(uint32_t fid,uint32_t nf,const char**seg,int nseg){uint8_t in[STYX_MAX_MSG],out[STYX_MAX_MSG];uint32_t il,ol;styx_build_twalk(in,&il,2,fid,nf,seg,nseg);return styxfs_serve(g_srv,in,il,out,&ol);}
static int cli_open(uint32_t fid,int mode){uint8_t in[STYX_MAX_MSG],out[STYX_MAX_MSG];uint32_t il,ol;styx_build_topen(in,&il,3,fid,mode);return styxfs_serve(g_srv,in,il,out,&ol);}
static int cli_read(uint32_t fid,uint64_t off,uint8_t*buf,uint32_t max){uint8_t in[STYX_MAX_MSG],out[STYX_MAX_MSG];uint32_t il,ol;styx_build_tread(in,&il,4,fid,off,max);int rc=styxfs_serve(g_srv,in,il,out,&ol);if(rc)return -1;if(ol<11)return 0;uint32_t n=styx_get32(out+7);if(n>max)n=max;memcpy(buf,out+11,n);return (int)n;}
static int cli_clunk(uint32_t fid){uint8_t in[STYX_MAX_MSG],out[STYX_MAX_MSG];uint32_t il,ol;styx_build_tclunk(in,&il,6,fid);return styxfs_serve(g_srv,in,il,out,&ol);}

/* read a whole /n file (walk apps/<name>) over 9P into a malloc'd buffer */
static int read_file_over_9p(const char *name, uint8_t **_out, size_t *_sz) {
    char apps[4]="apps";
    const char *seg[2]; seg[0]=apps; seg[1]=name;
    if (cli_version()!=0 || cli_attach(1)!=0) return -1;
    if (cli_walk(1,2,seg,2)!=0) return -1;          /* fid1 -> fid2 = /apps/<name> */
    if (cli_open(2,STX_OREAD)!=0) return -1;
    size_t cap=8192, n=0; uint8_t *buf=(uint8_t*)malloc(cap); if(!buf) return -1;
    for (;;) {
        if (n+STYX_MAX_DATA > cap) { cap*=2; uint8_t*nb=(uint8_t*)realloc(buf,cap); if(!nb){free(buf);return -1;} buf=nb; }
        int got=cli_read(2,(uint64_t)n, buf+n, STYX_MAX_DATA);
        if (got <= 0) break;
        n += (size_t)got;
        if (n && got < STYX_MAX_DATA) break;        /* short read = EOF (9P) */
    }
    cli_clunk(2);
    *_out=buf; *_sz=n;
    return 0;
}

int main(void) {
    wubu_zx_ops ops = { host_map, host_unmap };

    /* module bytes: the import-resolving PE A */
    uint8_t fa[2048]; size_t sza = build_pe_a(fa, sizeof(fa));

    /* publish it under a /n host dir, then mount that dir as 9P "/" */
    char tmpl[]="/tmp/wubu_zx9p_XXXXXX"; char *root=mkdtemp(tmpl);
    CHECK(root!=NULL,"create 9P root dir");
    char ap[512]; snprintf(ap,sizeof(ap),"%s/apps",root); mkdir(ap,0755);
    char mb[600]; snprintf(mb,sizeof(mb),"%s/apps/mod.bin",root);
    FILE *wf=fopen(mb,"wb"); size_t wrote=0; if(wf){wrote=fwrite(fa,1,sza,wf);fclose(wf);}
    CHECK(wrote==sza,"published module PE under /n/apps/mod.bin");

    styxfs_init(&g_srv_mem); g_srv=&g_srv_mem;
    CHECK(styxfs_mount(g_srv,"/",root,0)==0,"9P mount host root at /");

    /* fetch the module bytes over REAL Styx/9P (walk+open+read) */
    uint8_t *mod=NULL; size_t nmod=0;
    CHECK(read_file_over_9p("mod.bin",&mod,&nmod)==0,"fetched module over real 9P");
    if (mod) {
        fprintf(stderr, "> 9P fetched %zu bytes (want %zu)\n", nmod, sza);
        CHECK(nmod==sza && memcmp(mod,fa,sza)==0,
              "9P read returned the EXACT module bytes (transport is faithful)");
    }

    /* registry: NTOSKRNL.EXE!ANSWER -> host_answer (7) */
    CHECK(wubu_modsym_register("NTOSKRNL.EXE","ANSWER",(uintptr_t)&host_answer)==0,
          "registry has NTOSKRNL.EXE!ANSWER");

    /* run the 9P-fetched bytes through the loader */
    int err= -99; int ra=-1;
    if (mod) ra = wubu_zx_run(&ops, mod, nmod, 0x00400000u, NULL, &err);
    CHECK(err==0 && ra==7, "loader ran 9P-fetched module (import via registry -> 7)");

    int err2= -99; int ra2=-1;
    if (mod) ra2 = wubu_zx_run(&ops, mod, nmod, 0x00420000u, NULL, &err2);
    CHECK(err2==0 && ra2==7, "same 9P-fetched bytes at shifted base 0x00420000 -> 7 (relocation)");

    free(mod);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
