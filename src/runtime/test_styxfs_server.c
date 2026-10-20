/*
 * test_styxfs_server.c -- the host-backed 9P server (GOALS-1000 #734:
 * styxfs_server had no test target). Primary entry:
 * styxfs_server_create(root) wires the full vtable (attach/walk/open/
 * read/write/clunk/remove/stat/wstat) over a real host root; the test
 * drives it through styx_serve on the wire and destroys it.
 */
#include "styx.h"
#include "styxfs_server.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

static uint8_t inbuf[STYX_MAX_MSG], outbuf[STYX_MAX_MSG];
static uint32_t inlen, outlen;

static int serve1(styx_server_t *srv, const uint8_t *msg, uint32_t len)
{
    memcpy(inbuf, msg, len);
    return styx_serve(srv, inbuf, len, outbuf, &outlen);
}

int main(void)
{
    const char *scratch = "/home/wubu/.hermes/profiles/mind-palace/cache/scratch";
    char root[512], fp[512];
    snprintf(root, sizeof(root), "%s/srv_root", scratch);
    mkdir(root, 0755);
    snprintf(fp, sizeof(fp), "%s/hello.txt", root);
    FILE *f = fopen(fp, "wb");
    if (f) { fputs("hello from the host server", f); fclose(f); }

    /* create refuses NULL, a missing path, and a non-directory */
    CHECK(styxfs_server_create(NULL) == NULL);
    CHECK(styxfs_server_create("/no/such/dir") == NULL);
    CHECK(styxfs_server_create(fp) == NULL);

    styx_server_t *srv = styxfs_server_create(root);
    CHECK(srv != NULL);

    /* wire: version, attach, walk, open, read */
    styx_build_tversion(inbuf, &inlen, 8192, "9P2000");
    CHECK(serve1(srv, inbuf, inlen) == 0);
    CHECK(outbuf[4] == STX_RVERSION);

    styx_build_tattach(inbuf, &inlen, 1, 1, 0xFFFFFFFFu, "");
    CHECK(serve1(srv, inbuf, inlen) == 0);
    CHECK(outbuf[4] == STX_RATTACH);

    const char *wn[1] = { "hello.txt" };
    styx_build_twalk(inbuf, &inlen, 2, 1, 2, wn, 1);
    CHECK(serve1(srv, inbuf, inlen) == 0);
    CHECK(outbuf[4] == STX_RWALK);
    CHECK(styx_get16(outbuf + 7) == 1);              /* one qid */

    styx_build_topen(inbuf, &inlen, 3, 2, STX_OREAD);
    CHECK(serve1(srv, inbuf, inlen) == 0);
    CHECK(outbuf[4] == STX_ROPEN);

    styx_build_tread(inbuf, &inlen, 4, 2, 0, 64);
    CHECK(serve1(srv, inbuf, inlen) == 0);
    CHECK(outbuf[4] == STX_RREAD);
    uint32_t n = styx_get32(outbuf + 7);
    CHECK(n == strlen("hello from the host server"));
    CHECK(memcmp(outbuf + 11, "hello from the host server", n) == 0);

    /* stat through the server vtable */
    /* stat on an OPEN fid: the path must survive the Topen (the old code
     * replaced file_state with the slot index, breaking every post-open
     * stat/walk/reopen) */
    styx_build_tstat(inbuf, &inlen, 5, 2);
    CHECK(serve1(srv, inbuf, inlen) == 0);
    CHECK(outbuf[4] == STX_RSTAT);

    /* re-open the same fid: the preserved path makes it work again */
    styx_build_topen(inbuf, &inlen, 8, 2, STX_OREAD);
    CHECK(serve1(srv, inbuf, inlen) == 0);
    CHECK(outbuf[4] == STX_ROPEN);

    /* a walk to a missing name answers Rwalk with nwqid=0 -- the 9P2000
     * partial-walk encoding: fewer qids than names means the walk stopped
     * at the first missing component (0intro:186-196) */
    const char *wbad[1] = { "nope.txt" };
    styx_build_twalk(inbuf, &inlen, 6, 1, 3, wbad, 1);
    CHECK(serve1(srv, inbuf, inlen) == 0);
    CHECK(outbuf[4] == STX_RWALK);
    CHECK(styx_get16(outbuf + 7) == 0);

    /* clunk + destroy are clean */
    styx_build_tclunk(inbuf, &inlen, 7, 2);
    CHECK(serve1(srv, inbuf, inlen) == 0);
    CHECK(outbuf[4] == STX_RCLUNK);
    styxfs_server_destroy(srv);
    styxfs_server_destroy(NULL);                     /* NULL is a no-op */

    unlink(fp);
    rmdir(root);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
