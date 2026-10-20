/*
 * test_styx_parse.c -- the client-side T-message decoders (GOALS-1000
 * #722/#726: styx_parse had no test target). Each styx_enc builder's
 * output must decode field-exactly, and each parser must refuse a short
 * buffer or a wrong type byte.
 */
#include "styx.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

static uint8_t b[STYX_MAX_MSG];
static uint32_t n;

int main(void)
{
    /* Rversion (styx_parse_version decodes the REPLY: msize[4] version[s]) */
    uint32_t msize; char ver[16];
    {
        uint32_t pos = 0;
        styx_put32(b + pos, 19); pos += 4;
        b[pos] = STX_RVERSION; pos++;
        styx_put16(b + pos, 1); pos += 2;
        styx_put32(b + pos, 4096); pos += 4;
        pos += styx_putstr(b + pos, "9P2000");
        n = pos;
        CHECK(n == 19);
    }
    CHECK(styx_parse_version(b, n, &msize, ver) == 0);
    CHECK(msize == 4096 && strcmp(ver, "9P2000") == 0);
    CHECK(styx_parse_version(b, 12, &msize, ver) == -1);
    b[4] = STX_RREAD;
    CHECK(styx_parse_version(b, n, &msize, ver) == -1);
    CHECK(styx_parse_version(b, n, NULL, ver) == -1);

    /* Tattach (house layout: fid afid aname) */
    uint32_t fid, afid; char aname[64];
    styx_build_tattach(b, &n, 3, 7, 0xFFFFFFFF, "root");
    CHECK(styx_parse_attach(b, n, &fid, &afid, aname) == 0);
    CHECK(fid == 7 && afid == 0xFFFFFFFF && strcmp(aname, "root") == 0);
    CHECK(styx_parse_attach(b, 15, &fid, &afid, aname) == -1);

    /* Twalk with names */
    uint32_t newfid; char wnames[16][STYX_MAX_FNAME]; int nw;
    {
        uint32_t pos = 0;
        styx_put32(b + pos, 0); pos += 4;       /* patched below */
        b[pos] = STX_TWALK; pos++;
        styx_put16(b + pos, 5); pos += 2;
        styx_put32(b + pos, 1); pos += 4;       /* fid */
        styx_put32(b + pos, 9); pos += 4;       /* newfid */
        styx_put16(b + pos, 2); pos += 2;       /* nwname */
        pos += styx_putstr(b + pos, "a");
        pos += styx_putstr(b + pos, "b");
        styx_put32(b, pos);
        n = pos;
    }
    CHECK(styx_parse_walk(b, n, &fid, &newfid, wnames, &nw) == 0);
    CHECK(fid == 1 && newfid == 9 && nw == 2);
    CHECK(strcmp(wnames[0], "a") == 0 && strcmp(wnames[1], "b") == 0);
    CHECK(styx_parse_walk(b, 17, &fid, &newfid, wnames, &nw) == -1);

    /* Topen */
    int mode;
    styx_build_topen(b, &n, 4, 7, STX_ORDWR);
    CHECK(styx_parse_open(b, n, &fid, &mode) == 0);
    CHECK(fid == 7 && mode == STX_ORDWR);
    CHECK(styx_parse_open(b, 11, &fid, &mode) == -1);

    /* Tread */
    uint64_t off; uint32_t count;
    styx_build_tread(b, &n, 2, 7, 4096, 512);
    CHECK(styx_parse_read(b, n, &fid, &off, &count) == 0);
    CHECK(fid == 7 && off == 4096 && count == 512);
    CHECK(styx_parse_read(b, 22, &fid, &off, &count) == -1);

    /* Twrite: the data pointer must land exactly after the header */
    uint32_t wcount; const uint8_t *data;
    styx_build_twrite(b, &n, 6, 7, 100, 3, (const uint8_t *)"xyz");
    CHECK(styx_parse_write(b, n, &fid, &off, &wcount, &data) == 0);
    CHECK(fid == 7 && off == 100 && wcount == 3);
    CHECK(data == b + 23 && memcmp(data, "xyz", 3) == 0);
    CHECK(styx_parse_write(b, 23, &fid, &off, &wcount, &data) == -1);

    /* Tclunk / Tstat */
    styx_build_tclunk(b, &n, 8, 42);
    CHECK(styx_parse_clunk(b, n, &fid) == 0 && fid == 42);
    CHECK(styx_parse_clunk(b, 10, &fid) == -1);
    styx_build_tstat(b, &n, 9, 42);
    CHECK(styx_parse_stat(b, n, &fid) == 0 && fid == 42);
    b[4] = STX_TCLUNK;
    CHECK(styx_parse_stat(b, n, &fid) == -1);

    /* NULL buffer is refused everywhere */
    CHECK(styx_parse_version(NULL, 64, &msize, ver) == -1);
    CHECK(styx_parse_read(NULL, 64, &fid, &off, &count) == -1);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
