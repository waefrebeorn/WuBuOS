/*
 * wubu_9pc.{c,h}  --  KERNEL-CLEAN Styx/9P2000 CLIENT.
 *
 * The on-kernel half of DA#3 (docs/GAP_ROADMAP.md, GAP A2): a minimal 9P client
 * whose only libc use is memcpy/strlen, so it compiles under
 * `-ffreestanding -nostdlib -mcmodel=kernel`. It builds T-messages
 * (version/attach/walk/open/read/clunk) into a caller buffer and hands each to
 * an abstract TRANSPORT (a `w9pc_transport_fn` that returns the server's
 * reply) — on the booted kernel that transport is whatever carries 9P to the
 * /n server; in the hosted test it's the in-process styxfs_serve. Encoding
 * matches styx_enc.c byte-for-byte (the hosted server accepting a frame =
 * proof the codec is correct). No allocator of its own.
 */
#ifndef WUBU_9PC_H
#define WUBU_9PC_H

#include <stdint.h>
#include <stddef.h>

/* reply carried to/from the transport: type byte + tag + payload counters */
typedef struct {
    uint8_t  *buf;      /* response buffer (transport fills) */
    uint32_t  len;      /* response length */
    uint8_t   type;     /* response type (R*), RERROR on failure */
} w9pc_reply_t;

/* transport: send req, receive reply. Return 0 on success, nonzero on transport
 * error (NOT a 9P error — those come back as RERROR in *rsp). */
typedef int (*w9pc_transport_fn)(const uint8_t *req, uint32_t reqlen,
                                 uint8_t *rsp, uint32_t *rsplen, void *ud);

typedef struct {
    uint8_t *in; uint32_t in_cap;      /* outbound frame buffer */
    uint8_t *out; uint32_t out_cap;    /* inbound reply buffer */
    uint32_t out_len;                  /* reply length (set by internal tx) */
    w9pc_transport_fn tx; void *ud;    /* transport + opaque token */
} w9pc_t;

/* 9P2000 message types (kernel-clean: no styx.h include) */
#define W9_TVERSION 100
#define W9_TATTACH  104
#define W9_TWALK    110
#define W9_TOPEN    112
#define W9_TREAD    116
#define W9_TCLUNK   120
#define W9_RERROR   107

int w9pc_version(w9pc_t *c, uint32_t msize, const char *ver);
int w9pc_attach(w9pc_t *c, uint16_t tag, uint32_t fid, uint32_t afid,
                const char *aname);
int w9pc_walk(w9pc_t *c, uint16_t tag, uint32_t fid, uint32_t newfid,
              const char *const *wname, int nwname);
int w9pc_open(w9pc_t *c, uint16_t tag, uint32_t fid, int mode);
/* read up to count bytes at offset into dst; *got = bytes returned (0=EOF). */
int w9pc_read(w9pc_t *c, uint16_t tag, uint32_t fid, uint64_t offset,
              uint8_t *dst, uint32_t count, uint32_t *got);
int w9pc_clunk(w9pc_t *c, uint16_t tag, uint32_t fid);

#endif /* WUBU_9PC_H */
