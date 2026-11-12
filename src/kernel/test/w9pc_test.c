/* test_w9pc.c -- the KERNEL-CLEAN 9P client (wubu_9pc) drives a real Styx/9P
 * server and feeds the loader. Proves the freestanding client's message codec
 * is correct (the server rejects a malformed frame) AND that the codec path an
 * on-kernel 9P client will use loads a module end-to-end. */
#include "../../egojar.h"
#include "wubu_9pc.h"
#include "wubu_zx.h"
#include "wubu_pe.h"
#include "wubu_modsym.h"
#include "styxfs.h"
#include "styx.h"
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
    memset(p, 0, span); return p;
}
static void host_unmap(void *image, uintptr_t base, size_t span) { munmap(image, span); (void)base; }
static int host_answer(void) { return 7; }

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

static styxfs_server_t g_srv_mem; static styxfs_server_t *g_srv;
static int transport(const uint8_t *req, uint32_t reqlen, uint8_t *rsp,
                     uint32_t *rsplen, void *ud) {
    (void)ud;
    return styxfs_serve(g_srv, (uint8_t*)req, reqlen, rsp, rsplen);
}
/* fetch /apps/<name> via the KERNEL-CLEAN client into a malloc'd buffer */
static int fetch(const char *name, uint8_t **out, size_t *sz) {
    uint8_t inb[4096], outb[4096], big[STYX_MAX_DATA];
    w9pc_t c; memset(&c,0,sizeof c); c.in=inb; c.in_cap=sizeof inb; c.out=outb; c.out_cap=sizeof outb; c.tx=transport;
    const char *apps="apps";
    const char *seg[2]; seg[0]=apps; seg[1]=name;
    if (w9pc_version(&c,8192,"9P2000")!=0) return -1;
    if (w9pc_attach(&c,1,1,0,"/")!=0)      return -1;
    if (w9pc_walk(&c,1,1,2,seg,2)!=0)      return -1;
    if (w9pc_open(&c,1,2,0)!=0)            return -1;
    size_t cap=8192, n=0; uint8_t *buf=(uint8_t*)malloc(cap); if(!buf) return -1;
    for (;;) {
        if (n+STYX_MAX_DATA > cap){cap*=2; uint8_t*nb=(uint8_t*)realloc(buf,cap); if(!nb){free(buf);return -1;}buf=nb;}
        uint32_t got=0;
        if (w9pc_read(&c,1,2,(uint64_t)n, buf+n, STYX_MAX_DATA, &got)!=0){free(buf);return -1;}
        if (got==0) break;
        n += got;
        if (got < STYX_MAX_DATA) break;   /* short read = EOF */
    }
    w9pc_clunk(&c,1,2);
    *out=buf; *sz=n;
    return 0;
}

int main(void) {
    wubu_zx_ops ops = { host_map, host_unmap };
    uint8_t fa[2048]; size_t sza = build_pe_a(fa, sizeof(fa));

    char tmpl[]="/tmp/w9pc_XXXXXX"; char *root=mkdtemp(tmpl);
    CHECK(root!=NULL,"create 9P root dir");
    char ap[512]; snprintf(ap,sizeof(ap),"%s/apps",root); mkdir(ap,0755);
    char mb[600]; snprintf(mb,sizeof(mb),"%s/apps/mod.bin",root);
    FILE *wf=fopen(mb,"wb"); size_t wrote=0; if(wf){wrote=fwrite(fa,1,sza,wf);fclose(wf);}
    CHECK(wrote==sza,"published module under /n/apps/mod.bin");

    styxfs_init(&g_srv_mem); g_srv=&g_srv_mem;
    CHECK(styxfs_mount(g_srv,"/",root,0)==0,"9P mount host root at /");

    uint8_t *mod=NULL; size_t nmod=0;
    CHECK(fetch("mod.bin",&mod,&nmod)==0,"kernel-clean w9pc fetched the module over 9P");
    if (mod) {
        fprintf(stderr, "> w9pc fetched %zu bytes (want %zu)\n", nmod, sza);
        CHECK(nmod==sza && memcmp(mod,fa,sza)==0,"w9pc bytes are byte-exact");
    }
    CHECK(wubu_modsym_register("NTOSKRNL.EXE","ANSWER",(uintptr_t)&host_answer)==0,"registry ANSWER");
    int err=-99, r=-1; if(mod) r=wubu_zx_run(&ops,mod,nmod,0x00400000u,NULL,&err);
    CHECK(err==0 && r==7,"kernel-clean client -> loader -> 7");
    int err2=-99, r2=-1; if(mod) r2=wubu_zx_run(&ops,mod,nmod,0x00420000u,NULL,&err2);
    CHECK(err2==0 && r2==7,"at shifted base -> 7 (relocation)");
    free(mod);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
