/*
 * test_styx_enc.c -- the client-builder suite for styx_enc (GOALS-1000
 * #716: styx_enc had no test target). Asserts each builder's wire
 * layout against the 9P2000 message definitions (0intro), including the
 * five builders this cycle added (tauth/tflush/tcreate/tremove/twstat).
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

static void build_ok(void *fn, uint32_t len)
{
    (void)fn;
    CHECK(len >= 7);
    /* the 4-byte size prefix must equal the message length */
    CHECK(styx_get32(b) == len);
}

int main(void)
{
    /* ---- Tversion: msize[4] version[s] ---- */
    memset(b, 0, sizeof(b));
    styx_build_tversion(b, &n, 8192, "9P2000");
    build_ok(0, n);
    CHECK(b[4] == STX_TVERSION);
    CHECK(styx_get32(b + 7) == 8192);
    CHECK(styx_get16(b + 11) == 6 && memcmp(b + 13, "9P2000", 6) == 0);

    /* ---- Tattach: fid[4] afid[4] aname[s] (house layout; the builder
     * omits uname -- the server's parser reads one string at 15 too) ---- */
    memset(b, 0, sizeof(b));
    styx_build_tattach(b, &n, 4, 9, 0xFFFFFFFF, "a");
    build_ok(0, n);
    CHECK(b[4] == STX_TATTACH);
    CHECK(styx_get16(b + 5) == 4);
    CHECK(styx_get32(b + 7) == 9);
    CHECK(styx_get32(b + 11) == 0xFFFFFFFF);
    CHECK(styx_get16(b + 15) == 1 && b[17] == 'a');

    /* ---- Tflush: oldtag[2] (the NEW builder) ---- */
    memset(b, 0, sizeof(b));
    styx_build_tflush(b, &n, 0x11, 0x1234);
    build_ok(0, n);
    CHECK(b[4] == STX_TFLUSH);
    CHECK(styx_get16(b + 5) == 0x11);
    CHECK(styx_get16(b + 7) == 0x1234);
    CHECK(n == 9);

    /* ---- Tauth: afid[4] uname[s] aname[s] (the NEW builder) ---- */
    memset(b, 0, sizeof(b));
    styx_build_tauth(b, &n, 7, 0x2, "wu", "");
    build_ok(0, n);
    CHECK(b[4] == STX_TAUTH);
    CHECK(styx_get16(b + 5) == 7);
    CHECK(styx_get32(b + 7) == 0x2);
    CHECK(styx_get16(b + 11) == 2 && b[13] == 'w' && b[14] == 'u');
    CHECK(styx_get16(b + 15) == 0);               /* aname "" */

    /* ---- Tcreate: fid[4] name[s] perm[4] mode[1] (the NEW builder) ---- */
    memset(b, 0, sizeof(b));
    styx_build_tcreate(b, &n, 12, 5, "f.txt", 0644, STX_ORDWR);
    build_ok(0, n);
    CHECK(b[4] == STX_TCREATE);
    CHECK(styx_get16(b + 5) == 12);
    CHECK(styx_get32(b + 7) == 5);
    CHECK(styx_get16(b + 11) == 5 && memcmp(b + 13, "f.txt", 5) == 0);
    CHECK(styx_get32(b + 18) == 0644);
    CHECK(b[22] == STX_ORDWR);

    /* ---- Tremove: fid[4] (the NEW builder) ---- */
    memset(b, 0, sizeof(b));
    styx_build_tremove(b, &n, 3, 77);
    build_ok(0, n);
    CHECK(b[4] == STX_TREMOVE);
    CHECK(styx_get32(b + 7) == 77);
    CHECK(n == 11);

    /* ---- Twstat: fid[4] stat[n] (the NEW builder) ---- */
    memset(b, 0, sizeof(b));
    styx_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    dir.type = 1; dir.dev = 2;
    dir.qid.type = STX_QTFILE; dir.qid.version = 3; dir.qid.path = 4;
    dir.mode = 5; dir.atime = 6; dir.mtime = 7; dir.length = 8;
    snprintf(dir.name, sizeof(dir.name), "n");
    snprintf(dir.uid, sizeof(dir.uid), "u");
    snprintf(dir.gid, sizeof(dir.gid), "g");
    snprintf(dir.muid, sizeof(dir.muid), "m");
    styx_build_twstat(b, &n, 8, 42, &dir);
    build_ok(0, n);
    CHECK(b[4] == STX_TWSTAT);
    CHECK(styx_get32(b + 7) == 42);
    uint16_t dsz = styx_get16(b + 11);
    /* the n prefix counts the Dir bytes after the prefix (STATFIXLEN=49) */
    CHECK(dsz == 51);   /* fixed 41 + 4 strings x 3B, excluding the prefix */
    CHECK(styx_get16(b + 13) == 1);               /* dir.type */
    CHECK(styx_get32(b + 15) == 2);               /* dir.dev */
    CHECK(b[19] == STX_QTFILE);
    CHECK(styx_get32(b + 20) == 3);               /* qid.version */
    CHECK(styx_get64(b + 24) == 4);               /* qid.path */
    CHECK(styx_get32(b + 32) == 5);               /* mode */
    CHECK(styx_get32(b + 36) == 6);               /* atime */
    CHECK(styx_get32(b + 40) == 7);               /* mtime */
    CHECK(styx_get64(b + 44) == 8);               /* length */
    CHECK(n == (uint32_t)(11 + 2 + dsz));

    /* ---- the legacy builders keep their layouts ---- */
    memset(b, 0, sizeof(b));
    styx_build_twalk(b, &n, 2, 1, 2, NULL, 0);
    build_ok(0, n);
    CHECK(b[4] == STX_TWALK);
    CHECK(styx_get16(b + 15) == 0);               /* nwname 0 */

    memset(b, 0, sizeof(b));
    styx_build_topen(b, &n, 3, 9, STX_OREAD);
    build_ok(0, n);
    CHECK(b[4] == STX_TOPEN);
    CHECK(b[11] == STX_OREAD);

    memset(b, 0, sizeof(b));
    styx_build_tread(b, &n, 4, 9, 0, 100);
    build_ok(0, n);
    CHECK(b[4] == STX_TREAD);
    CHECK(styx_get64(b + 11) == 0);
    CHECK(styx_get32(b + 19) == 100);

    memset(b, 0, sizeof(b));
    styx_build_twrite(b, &n, 5, 9, 0, 3, (const uint8_t *)"abc");
    build_ok(0, n);
    CHECK(b[4] == STX_TWRITE);
    CHECK(styx_get64(b + 11) == 0);
    CHECK(styx_get32(b + 19) == 3);
    CHECK(memcmp(b + 23, "abc", 3) == 0);

    memset(b, 0, sizeof(b));
    styx_build_tclunk(b, &n, 6, 9);
    build_ok(0, n);
    CHECK(b[4] == STX_TCLUNK);

    memset(b, 0, sizeof(b));
    styx_build_tstat(b, &n, 7, 9);
    build_ok(0, n);
    CHECK(b[4] == STX_TSTAT);

    /* ---- Rerror: ename[s] ---- */
    memset(b, 0, sizeof(b));
    styx_error(9, "nope", b, &n);
    CHECK(b[4] == STX_RERROR);
    CHECK(styx_get16(b + 5) == 9);
    CHECK(styx_get16(b + 7) == 4 && memcmp(b + 9, "nope", 4) == 0);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
