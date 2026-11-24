/* test_9psrv.c -- the kernel 9P SERVER against the kernel 9P CLIENT.
 *
 * Both halves are kernel-clean codecs, so wiring the client's transport
 * straight into wubu_9psrv_serve() is the strongest gate available off-metal:
 * if either side mis-encodes a field, the round trip breaks. The bytes served
 * for a tick file must equal what was written into the LIVE tensor through
 * wubu_kvfs_route_write -- no disk, no placeholder file.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "wubu_kvfs.h"
#include "wubu_9pc.h"
#include "wubu_9psrv.h"

static int has_bytes(const uint8_t *hay, uint32_t n, const char *needle)
{
    uint32_t nl = (uint32_t)strlen(needle);
    if (!nl || n < nl) return 0;
    for (uint32_t i = 0; i + nl <= n; i++)
        if (memcmp(hay + i, needle, nl) == 0) return 1;
    return 0;
}

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); ++fails; } } while (0)

static uint8_t reqbuf[1024];
static uint8_t rspbuf[WUBU_9PSRV_MSIZE];

static int tx(const uint8_t *req, uint32_t n, uint8_t *rsp, uint32_t *rn, void *ud)
{
    (void)ud;
    if (wubu_9psrv_serve(req, n, rsp, WUBU_9PSRV_MSIZE, rn) != 0) return -1;
    return 0;
}

int main(void)
{
    /* the live namespace: 256 floats per block, 4096 blocks */
    CHECK(wubu_kvfs_kernel_init(256, 4096) == 0, "kvfs kernel init");
    CHECK(g_wubu_kvfs != NULL, "kvfs singleton");
    CHECK(wubu_kvfs_mount(g_wubu_kvfs, "/kv/world", 0, 1024) == 0, "mount /kv/world");

    /* the body writes one experience tick into the tensor */
    const float tick[4] = { 11.0f, 22.0f, 33.0f, 44.0f };
    uint32_t wr = 0;
    CHECK(wubu_kvfs_route_write("/kv/world/tick_3", 0, sizeof tick,
                                (const uint8_t *)tick, &wr) == 0, "route write");
    CHECK(wr == sizeof tick, "route write count");

    wubu_9psrv_reset();
    w9pc_t c;
    memset(&c, 0, sizeof c);
    c.in = reqbuf;  c.in_cap = sizeof reqbuf;
    c.out = rspbuf; c.out_cap = sizeof rspbuf;
    c.tx = tx;      c.ud = NULL;

    CHECK(w9pc_version(&c, 8192, "9P2000") == 0, "Tversion served");
    CHECK(c.out[4] == 101, "Rversion type");
    CHECK(memcmp(rspbuf + 13, "9P2000", 6) == 0, "Rversion string");

    CHECK(w9pc_attach(&c, 1, 1, (uint32_t)~0u, "") == 0, "Tattach served");
    CHECK(c.out[4] == 105, "Rattach type");
    CHECK((rspbuf[7] & 0x80u) != 0, "attach qid is a directory");

    /* walk / -> kv -> world -> tick_3 */
    const char *names[3] = { "kv", "world", "tick_3" };
    CHECK(w9pc_walk(&c, 2, 1, 2, names, 3) == 0, "Twalk served");
    CHECK(c.out[4] == 111, "Rwalk type");

    CHECK(w9pc_open(&c, 4, 2, 0) == 0, "Topen served");
    CHECK(c.out[4] == 113, "Ropen type");

    uint8_t got[64];
    uint32_t n = 0;
    CHECK(w9pc_read(&c, 5, 2, 0, got, sizeof tick, &n) == 0, "Tread served");
    CHECK(n == sizeof tick, "tick read size");
    CHECK(memcmp(got, tick, sizeof tick) == 0,
          "tick bytes come from the LIVE tensor");

    /* past the block -> EOF, not an error */
    n = 0xffffu;
    CHECK(w9pc_read(&c, 6, 2, 4096, got, sizeof got, &n) == 0, "Tread past EOF served");
    CHECK(n == 0, "past-EOF read returns 0 bytes");

    CHECK(w9pc_clunk(&c, 7, 3) == 0, "Tclunk served");
    CHECK(c.out[4] == 121, "Rclunk type");

    /* a directory read must list the mount as "world" */
    CHECK(w9pc_walk(&c, 8, 1, 3, names, 1) == 0, "Twalk to /kv");
    CHECK(w9pc_open(&c, 9, 3, 0) == 0, "Topen /kv");
    n = 0;
    CHECK(w9pc_read(&c, 10, 3, 0, got, sizeof got, &n) == 0, "Tread /kv");
    CHECK(n > 2, "/kv lists at least one child");
    CHECK(has_bytes(got, n, "world"), "/kv lists \"world\"");

    /* reads only: the body owns the tensor */
    uint8_t *o = rspbuf;
    (void)o;
    {
        /* Twrite (118): fid=2, offset, count, data */
        uint8_t frame[32];
        memset(frame, 0, sizeof frame);
        uint32_t at = 7;
        uint32_t fid = 2;
        memcpy(frame + at, &fid, 4); at += 4;
        at += 8;                                     /* offset = 0 (frame zeroed) */
        memcpy(frame + at, &fid, 4); at += 4;       /* count = 4 */
        frame[at++] = 0xAA;
        uint32_t sz = at;
        memcpy(frame, &sz, 4);
        frame[4] = 118;                             /* Twrite */
        frame[5] = 11; frame[6] = 0;
        uint32_t rl = 0;
        CHECK(wubu_9psrv_serve(frame, at, rspbuf, sizeof rspbuf, &rl) == 0,
              "Twrite answered");
        CHECK(rspbuf[4] == 107, "Twrite -> Rerror (body-owned)");
    }
    {
        /* unsupported message type -> Rerror, never a silent drop */
        uint8_t frame[11] = { 11, 0, 0, 0, 126, 12, 0, 2, 0, 0, 0 }; /* Twstat on the tick file */
        uint32_t rl = 0;
        CHECK(wubu_9psrv_serve(frame, sizeof frame, rspbuf, sizeof rspbuf, &rl) == 0,
              "Twstat answered");
        CHECK(rspbuf[4] == 107, "Twstat -> Rerror");
    }

    /* Tstat on the tick file: one block of floats = 1024 bytes */
    {
        uint8_t frame[11];
        uint32_t fid = 2, sz = 11;
        memcpy(frame, &sz, 4); frame[4] = 124; frame[5] = 13; frame[6] = 0;
        memcpy(frame + 7, &fid, 4);
        uint32_t rl = 0;
        CHECK(wubu_9psrv_serve(frame, sizeof frame, rspbuf, sizeof rspbuf, &rl) == 0,
              "Tstat served");
        CHECK(rspbuf[4] == 125, "Rstat type");
        uint64_t len = 0;
        memcpy(&len, rspbuf + 7 + 33, 8);
        CHECK(len == 1024, "stat length = one block of floats");
    }

    if (g_wubu_kv_base) { free(g_wubu_kv_base); g_wubu_kv_base = NULL; }
    wubu_kvfs_free(g_wubu_kvfs);
    g_wubu_kvfs = NULL;

    printf("test_9psrv: %s (9P server on the live KV tensor)\n",
           fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
