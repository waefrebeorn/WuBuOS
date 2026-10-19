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
#include <errno.h>

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

    TEST("Twrite of 9000 bytes is capped at STYX_MAX_DATA");
    if (arm_fid(&srv, 3) != 0) { FAIL("fid alloc"); return 1; }
    uint8_t *big = malloc(9000);
    if (!big) { FAIL("alloc"); return 1; }
    memset(big, 'A', 9000);
    uint32_t nb = 0;
    int rcb = styxfs_write_cb(&srv.base, 3, 0, 9000, big, &nb);
    struct stat st;
    int sz = (stat(g_path, &st) == 0) ? (int)st.st_size : -1;
    printf("[nwritten=%u size=%d] ", nb, sz);
    if (rcb == 0 && nb == STYX_MAX_DATA && sz == STYX_MAX_DATA) PASS();
    else FAIL("write cap not enforced");
    free(big);

    TEST("QTAPPEND node ignores the client offset");
    if (arm_fid(&srv, 4) != 0) { FAIL("fid alloc"); return 1; }
    styx_fid_t *fa = styx_fid_lookup(&srv.base, 4);
    if (fa) fa->qid.type = STX_QTAPPEND;
    uint32_t na = 0;
    uint8_t x = 'X';
    styxfs_write_cb(&srv.base, 4, 0, 1, &x, &na);
    styxfs_write_cb(&srv.base, 4, 0, 1, &x, &na);
    int sz2 = (stat(g_path, &st) == 0) ? (int)st.st_size : -1;
    int tail[2] = {0, 0};
    int fd2 = open(g_path, O_RDONLY);
    if (fd2 >= 0 && sz2 >= 2) { pread(fd2, &tail[0], 1, sz2 - 2); pread(fd2, &tail[1], 1, sz2 - 1); close(fd2); }
    printf("[size=%d tail=%c%c] ", sz2, tail[0], tail[1]);
    if (sz2 == STYX_MAX_DATA + 2 && tail[0] == 'X' && tail[1] == 'X') PASS();
    else FAIL("append did not append");

    TEST("Tstat on a mount point advertises QTMOUNT");
    styxfs_mount(&srv, "/n/kv", "/var/kv", 0);
    if (arm_fid(&srv, 5) != 0) { FAIL("fid alloc"); return 1; }
    styx_fid_t *fm = styx_fid_lookup(&srv.base, 5);
    if (fm) {
        free(fm->file_state);
        fm->file_state = strdup("/n/kv");
        fm->qid.type = STX_QTDIR;
        fm->qid.path = 7;
    }
    styx_dir_t sd;
    int rc5 = styxfs_stat_cb(&srv.base, 5, &sd);
    printf("[type=0x%02x path=0x%llx] ", sd.qid.type, (unsigned long long)sd.qid.path);
    if (rc5 == 0 && (sd.qid.type & STX_QTMOUNT) && sd.qid.path != 7) PASS();
    else FAIL("mount point not advertised");

    TEST("Tstat on a non-mount path has no QTMOUNT");
    if (arm_fid(&srv, 6) != 0) { FAIL("fid alloc"); return 1; }
    styx_dir_t nd;
    int rc6 = styxfs_stat_cb(&srv.base, 6, &nd);
    if (rc6 == 0 && (nd.qid.type & STX_QTMOUNT) == 0) PASS();
    else FAIL("plain node advertised as a mount");

    TEST("Tcreate with OEXCL on an existing name returns EEXIST");
    const char *scratch = "/home/wubu/.hermes/profiles/mind-palace/cache/scratch";
    char seed[512];
    snprintf(seed, sizeof seed, "%s/cb_existing", scratch);
    { int fd = open(seed, O_CREAT | O_TRUNC | O_WRONLY, 0644); if (fd >= 0) close(fd); }
    styxfs_mount(&srv, "/wubu", (char *)scratch, 0);

    if (arm_fid(&srv, 7) != 0) { FAIL("fid alloc"); return 1; }
    styx_fid_t *f7 = styx_fid_lookup(&srv.base, 7);
    if (f7) { free(f7->file_state); f7->file_state = strdup("/wubu"); f7->qid.type = STX_QTDIR; }
    styx_qid_t q;
    errno = 0;
    int rc7 = styxfs_create_cb(&srv.base, 7, "cb_existing", 0644, STX_OEXCL, &q);
    printf("[rc=%d errno=%d] ", rc7, errno);
    if (rc7 != 0 && errno == EEXIST) PASS(); else FAIL("OEXCL on existing name did not return EEXIST");

    TEST("Tcreate DMDIR over an existing file returns ENOTDIR");
    errno = 0;
    int rc8 = styxfs_create_cb(&srv.base, 7, "cb_existing", STX_DMDIR | 0755u, 0, &q);
    printf("[rc=%d errno=%d] ", rc8, errno);
    if (rc8 != 0 && errno == ENOTDIR) PASS(); else FAIL("DMDIR over file did not return ENOTDIR");

    TEST("accepted mode bits round-trip through Tstat");
    errno = 0;
    int rc9 = styxfs_create_cb(&srv.base, 7, "cb_newfile", 0600u, 0, &q);
    styx_dir_t cd;
    int rcs = styxfs_stat_cb(&srv.base, 7, &cd);
    printf("[rc=%d mode=0%o] ", rc9, cd.mode & 07777);
    if (rc9 == 0 && rcs == 0 && (cd.mode & 07777) == 0600) PASS();
    else FAIL("mode did not round-trip");

    TEST("Tcreate DMDIR makes a directory Tstat reports as one");
    /* create() moves the fid onto the new node, so re-root it first */
    if (f7) { free(f7->file_state); f7->file_state = strdup("/wubu"); f7->qid.type = STX_QTDIR; }
    errno = 0; int rcd = styxfs_create_cb(&srv.base, 7, "cb_dir", STX_DMDIR | 0755u, 0, &q);
    styx_dir_t dd;
    int rcds = styxfs_stat_cb(&srv.base, 7, &dd);
    printf("[rc=%d errno=%d mode=0%o] ", rcd, errno, dd.mode & 07777);
    if (rcd == 0 && rcds == 0 && (dd.mode & 0040000)) PASS();
    else FAIL("created directory not reported as a directory");

    TEST("one Dir record is 64 bytes for a 3-char name, size field 62");
    uint8_t rec[128];
    uint32_t rl = styxfs_dir_encode(rec, sizeof rec, "e00", 0, 100);
    uint16_t szf = (uint16_t)(rec[0] | (rec[1] << 8));
    printf("[len=%u size=%u] ", rl, szf);
    if (rl == 64 && szf == 62 && rl == (uint32_t)szf + 2) PASS();
    else FAIL("record length or size field wrong");

    TEST("record name field parses at offset 41");
    uint16_t nl = (uint16_t)(rec[41] | (rec[42] << 8));
    printf("[namelen=%u name=%.3s] ", nl, rec + 43);
    if (nl == 3 && memcmp(rec + 43, "e00", 3) == 0) PASS();
    else FAIL("name not where the layout says");

    TEST("12 entries page in 3 reads of 4 records by byte offset");
    const char *sc2 = "/home/wubu/.hermes/profiles/mind-palace/cache/scratch";
    char d12[512];
    snprintf(d12, sizeof d12, "%s/d12", sc2);
    mkdir(d12, 0755);
    for (int i = 0; i < 12; i++) {
        char fp[600];
        snprintf(fp, sizeof fp, "%s/e%02d", d12, i);
        int fd = open(fp, O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (fd >= 0) close(fd);
    }
    if (arm_fid(&srv, 8) != 0) { FAIL("fid alloc"); return 1; }
    styx_fid_t *f8 = styx_fid_lookup(&srv.base, 8);
    if (f8) { free(f8->file_state); f8->file_state = strdup("/wubu/d12"); f8->qid.type = STX_QTDIR; }

    uint8_t pg[512];
    uint64_t boff = 0;
    int reads = 0, names_seen = 0, uniq = 0;
    char seen[32][64];
    while (reads < 8) {
        uint32_t nr = 0;
        int r = styxfs_read_cb(&srv.base, 8, boff, 256, pg, &nr);
        if (r != 0 || nr == 0) break;
        reads++;
        boff += nr;
        uint32_t o = 0;
        while (o + 43 <= nr) {
            uint16_t recsz = (uint16_t)(pg[o] | (pg[o + 1] << 8));
            uint32_t rt = 2u + recsz;
            if (rt < 43 || o + rt > nr) break;
            uint16_t nl = (uint16_t)(pg[o + 41] | (pg[o + 42] << 8));
            if (43u + nl <= rt) {
                char nm[64];
                memcpy(nm, pg + o + 43, nl); nm[nl] = 0;
                int dup = 0;
                for (int k = 0; k < uniq; k++) if (!strcmp(seen[k], nm)) dup = 1;
                if (!dup && uniq < 32) snprintf(seen[uniq++], 64, "%s", nm);
                names_seen++;
            }
            o += rt;
        }
    }
    printf("[reads=%d names=%d uniq=%d] ", reads, names_seen, uniq);
    if (reads == 3 && names_seen == 12 && uniq == 12) PASS();
    else FAIL("paging did not cover 12 unique entries in 3 reads");

    TEST("attach with aname roots the session at that subtree");
    styxfs_mount(&srv, "/wubu", (char *)"/home/wubu/.hermes/profiles/mind-palace/cache/scratch", 0);
    uint32_t hf;
    int rc_att = styxfs_attach_cb(&srv.base, 20, "/wubu");
    styx_fid_t *fa2 = styx_fid_lookup(&srv.base, 20);
    const char *root = (fa2 && fa2->file_state) ? (const char *)fa2->file_state : "";
    printf("[rc=%d root=%s] ", rc_att, root);
    if (rc_att == 0 && strcmp(root, "/wubu") == 0) PASS(); else FAIL("aname did not root the session");

    TEST("walk from that root resolves the child path");
    char wn[1][64]; snprintf(wn[0], 64, "%s", "d12");
    char *names[1]; names[0] = wn[0];
    styx_qid_t qs[1];
    int nq = 0;
    int rcw = styxfs_walk_cb(&srv.base, 20, 21, (const char **)names, 1, qs, &nq);
    styx_fid_t *fc = styx_fid_lookup(&srv.base, 21);
    const char *cp = (fc && fc->file_state) ? (const char *)fc->file_state : "";
    printf("[rc=%d nq=%d path=%s] ", rcw, nq, cp);
    if (rcw == 0 && nq == 1 && strcmp(cp, "/wubu/d12") == 0) PASS();
    else FAIL("walk did not land under the attached root");

    TEST("Tauth on a no-auth server returns ENOTSUP and allocates no fid");
    errno = 0;
    styx_qid_t aq;
    int rca = styxfs_auth_cb(&srv.base, 22, "", "wubu", &aq);
    printf("[rc=%d errno=%d] ", rca, errno);
    if (rca == -1 && errno == ENOTSUP && styx_fid_lookup(&srv.base, 22) == NULL) PASS();
    else FAIL("auth was not rejected cleanly");

    unlink(g_path);
    printf("\n=== styxfs callbacks: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
