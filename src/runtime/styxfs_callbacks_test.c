/*
 * styxfs_callbacks_test.c -- direct tests of the 9P filesystem callbacks.
 *
 * Calls styxfs_read_cb/styxfs_write_cb with a real fid and a real backing file,
 * which is what the serve loop does; no socket is involved.
 */
#include "styxfs.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_pass, g_fail;
#define TEST(n) do { printf("  TEST %-52s", n); } while (0)
#define PASS()  do { printf("✅\n"); g_pass++; } while (0)
#define FAIL(m) do { printf("❌ %s\n", m); g_fail++; } while (0)
#define CHECK(c, m) do { if (!(c)) { FAIL(m); return; } } while (0)

static char g_path[256];

static void make_file(size_t nbytes)
{
    snprintf(g_path, sizeof g_path, "/home/wubu/.hermes/profiles/mind-palace/cache/scratch/styx_cb_%d.bin",
             (int)getpid());
    int fd = open(g_path, O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (fd < 0) return;
    for (size_t i = 0; i < nbytes; i++) {
        unsigned char c = (unsigned char)(i & 0xFF);
        (void)write(fd, &c, 1);
    }
    close(fd);
}

/* Attach a fid to g_path as a regular file. */
static int arm_fid(styxfs_server_t *srv, uint32_t fidno)
{
    styx_fid_t *f = styx_fid_alloc(&srv->base, fidno);
    if (!f) return -1;
    f->file_state = strdup(g_path);
    f->qid.type = 0;
    f->qid.path = 42;
    f->offset = 0;
    return 0;
}

int main(void)
{
    make_file(37);

    styxfs_server_t srv;
    styxfs_init(&srv);

    TEST("Tread at an offset past EOF returns count=0");
    if (arm_fid(&srv, 1) != 0) { FAIL("fid alloc"); return 1; }
    uint8_t buf[64];
    uint32_t nread = 123;
    int rc = styxfs_read_cb(&srv.base, 1, 1000, sizeof buf, buf, &nread);
    printf("[rc=%d nread=%u] ", rc, nread);
    if (rc == 0 && nread == 0) PASS(); else FAIL("past-EOF read was not a zero-count success");

    TEST("Tread at offset 0 returns the file bytes");
    nread = 0;
    rc = styxfs_read_cb(&srv.base, 1, 0, 16, buf, &nread);
    int ok = (rc == 0 && nread == 16);
    for (int i = 0; ok && i < 16; i++) if (buf[i] != (uint8_t)i) ok = 0;
    if (ok) PASS(); else FAIL("head read mismatch");

    TEST("Tread at EOF boundary returns count=0");
    nread = 99;
    rc = styxfs_read_cb(&srv.base, 1, 37, 16, buf, &nread);
    if (rc == 0 && nread == 0) PASS(); else FAIL("read exactly at EOF was not zero-count");

    TEST("Tread of an unknown fid fails");
    nread = 0;
    rc = styxfs_read_cb(&srv.base, 999, 0, 16, buf, &nread);
    if (rc != 0) PASS(); else FAIL("unknown fid accepted");

    TEST("Twrite then Tread returns the exact bytes");
    if (arm_fid(&srv, 2) != 0) { FAIL("fid alloc"); return 1; }
    const uint8_t payload[8] = {'W','U','B','U','O','S','!','!'};
    uint32_t nw = 0;
    rc = styxfs_write_cb(&srv.base, 2, 0, sizeof payload, payload, &nw);
    uint8_t back[8];
    uint32_t nr = 0;
    int rc2 = styxfs_read_cb(&srv.base, 2, 0, sizeof back, back, &nr);
    if (rc == 0 && rc2 == 0 && nw == sizeof payload && nr == sizeof payload &&
        memcmp(back, payload, sizeof payload) == 0) PASS();
    else FAIL("write/read round trip mismatch");

    unlink(g_path);
    printf("\n=== styxfs callbacks: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
