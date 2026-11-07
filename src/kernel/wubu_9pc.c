#include "wubu_9pc.h"
#include <string.h>

/* local strlen: -nostdlib may not provide the libc symbol */
static size_t w9_strlen(const char *s){ size_t n=0; if(!s) return 0; while(s[n]) n++; return n; }
static void w9_memcpy(void *d, const void *s, size_t n){ if(n) memcpy(d, s, n); }

/* ---- little endian primitives (memcpy: alignment-safe) ---- */
static void w9_put16(uint8_t *p, uint16_t v){ w9_memcpy(p,&v,2); }
static void w9_put32(uint8_t *p, uint32_t v){ w9_memcpy(p,&v,4); }
static void w9_put64(uint8_t *p, uint64_t v){ w9_memcpy(p,&v,8); }
static uint32_t w9_get32(const uint8_t *p){ uint32_t v; w9_memcpy(&v,p,4); return v; }
static size_t w9_putstr(uint8_t *p, const char *s){
    size_t l = w9_strlen(s);
    w9_put16(p, (uint16_t)l);
    if (l) w9_memcpy(p+2, s, l);
    return 2 + l;
}
/* write size/type/tag header over the first 7 bytes; total message = pos */
static void w9_header(uint8_t *b, uint32_t pos, uint8_t type, uint16_t tag){
    w9_put32(b, pos); b[4]=type; w9_put16(b+5, tag);
}
/* transmit buf[0..pos]; returns 0 and sets c->out reply, -1 on transport err */
static int w9_tx(w9pc_t *c, uint8_t type, uint16_t tag, uint32_t pos){
    if (pos < 7) return -1;
    w9_header(c->in, pos, type, tag);
    uint32_t ol = 0;
    if (c->tx(c->in, pos, c->out, &ol, c->ud) != 0) return -1;
    c->out_len = ol;
    if (ol < 7) return -1;
    if (c->out[4] == W9_RERROR) return -1;   /* server replied with a 9P error */
    return 0;
}

int w9pc_version(w9pc_t *c, uint32_t msize, const char *ver){
    uint32_t pos = 7;
    w9_put32(c->in + pos, msize); pos += 4;
    pos += (uint32_t)w9_putstr(c->in + pos, ver ? ver : "");
    return w9_tx(c, W9_TVERSION, 0, pos);
}
int w9pc_attach(w9pc_t *c, uint16_t tag, uint32_t fid, uint32_t afid,
                const char *aname){
    uint32_t pos = 7;
    w9_put32(c->in + pos, fid); pos += 4;
    w9_put32(c->in + pos, afid); pos += 4;
    pos += (uint32_t)w9_putstr(c->in + pos, aname ? aname : "");
    return w9_tx(c, W9_TATTACH, tag, pos);
}
int w9pc_walk(w9pc_t *c, uint16_t tag, uint32_t fid, uint32_t newfid,
              const char *const *wname, int nwname){
    uint32_t pos = 7;
    w9_put32(c->in + pos, fid); pos += 4;
    w9_put32(c->in + pos, newfid); pos += 4;
    w9_put16(c->in + pos, (uint16_t)nwname); pos += 2;
    for (int i = 0; i < nwname && i < 16; i++)
        pos += (uint32_t)w9_putstr(c->in + pos, wname[i]);
    return w9_tx(c, W9_TWALK, tag, pos);
}
int w9pc_open(w9pc_t *c, uint16_t tag, uint32_t fid, int mode){
    uint32_t pos = 7;
    w9_put32(c->in + pos, fid); pos += 4;
    c->in[pos] = (uint8_t)mode; pos++;
    return w9_tx(c, W9_TOPEN, tag, pos);
}
int w9pc_read(w9pc_t *c, uint16_t tag, uint32_t fid, uint64_t offset,
              uint8_t *dst, uint32_t count, uint32_t *got){
    uint32_t pos = 7;
    w9_put32(c->in + pos, fid); pos += 4;
    w9_put64(c->in + pos, offset); pos += 8;
    w9_put32(c->in + pos, count); pos += 4;
    if (w9_tx(c, W9_TREAD, tag, pos) != 0) return -1;
    if (c->out[4] != 117) return -1;            /* expect Rread */
    if (c->out_len < 11) return -1;             /* no data returned */
    uint32_t n = w9_get32(c->out + 7);          /* Rread count */
    if (n > count) n = count;
    if (n) memcpy(dst, c->out + 11, n);
    if (got) *got = n;
    return 0;
}
int w9pc_clunk(w9pc_t *c, uint16_t tag, uint32_t fid){
    uint32_t pos = 7;
    w9_put32(c->in + pos, fid); pos += 4;
    return w9_tx(c, W9_TCLUNK, tag, pos);
}
