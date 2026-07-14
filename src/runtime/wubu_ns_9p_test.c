/*
 * wubu_ns_9p_test.c -- prove /n is served over REAL Styx/9P end-to-end.
 *
 * The bridge publishes control-plane files into a host dir; a StyxFS server
 * exports that same dir over 9P. This test acts as a 9P *client* (using the
 * styx_build_t* / styxfs_serve harness) to:
 *   1. walk+open+READ  /n/snap/<c>/list      -> see the live snapshot list
 *   2. walk+open+WRITE /n/snap/<c>/create     -> write lands in the host file
 *   3. dispatch wubu_ns_snap_create (the control-plane consumer)
 *   4. READ /n/snap/<c>/list again            -> shows the new snapshot
 *
 * This verifies the /n namespace is a genuine 9P filesystem (a 9P client reads
 * the same live data the bridge wrote), not just synthesized host files.
 */

#define _GNU_SOURCE
#include "wubu_ns_bridge.h"
#include "wubu_snapshot.h"
#include "styxfs.h"
#include "styx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else      { g_fail++; printf("  ❌ %s\n", msg); } \
} while (0)

/* --- tiny 9P client against an in-process styxfs_server_t --- */
static styxfs_server_t g_srv_mem;
static styxfs_server_t *g_srv;

static int cli_version(void) {
    uint8_t in[STYX_MAX_MSG], out[STYX_MAX_MSG]; uint32_t il, ol;
    styx_build_tversion(in, &il, 8192, "9P2000");
    return styxfs_serve(g_srv, in, il, out, &ol);
}
static int cli_attach(uint32_t fid) {
    uint8_t in[STYX_MAX_MSG], out[STYX_MAX_MSG]; uint32_t il, ol;
    styx_build_tattach(in, &il, 1, fid, 0, "/");
    return styxfs_serve(g_srv, in, il, out, &ol);
}
/* walk from fid to a path of segments; returns newfid in *outfid */
static int cli_walk(uint32_t fid, uint32_t newfid, const char **seg, int nseg) {
    uint8_t in[STYX_MAX_MSG], out[STYX_MAX_MSG]; uint32_t il, ol;
    styx_build_twalk(in, &il, 2, fid, newfid, seg, nseg);
    return styxfs_serve(g_srv, in, il, out, &ol);
}
static int cli_open(uint32_t fid, int mode) {
    uint8_t in[STYX_MAX_MSG], out[STYX_MAX_MSG]; uint32_t il, ol;
    styx_build_topen(in, &il, 3, fid, mode);
    return styxfs_serve(g_srv, in, il, out, &ol);
}
/* read up to max bytes from fid; returns nread, data copied to buf */
static int cli_read(uint32_t fid, uint64_t off, uint8_t *buf, uint32_t max) {
    uint8_t in[STYX_MAX_MSG], out[STYX_MAX_MSG]; uint32_t il, ol;
    styx_build_tread(in, &il, 4, fid, off, max);
    int rc = styxfs_serve(g_srv, in, il, out, &ol);
    if (rc != 0) return -1;
    if (ol < 11) return 0;
    uint32_t n = styx_get32(out + 7);   /* Rread count */
    if (n > max) n = max;
    memcpy(buf, out + 11, n);
    return (int)n;
}
/* write bytes to fid; returns 0 on success */
static int cli_write(uint32_t fid, uint64_t off, const uint8_t *data, uint32_t n) {
    uint8_t in[STYX_MAX_MSG], out[STYX_MAX_MSG]; uint32_t il, ol;
    styx_build_twrite(in, &il, 5, fid, off, n, data);
    int rc = styxfs_serve(g_srv, in, il, out, &ol);
    if (rc != 0) return -1;
    uint32_t wrote = styx_get32(out + 7);  /* Rwrite count */
    return (wrote == n) ? 0 : -1;
}
static int cli_clunk(uint32_t fid) {
    uint8_t in[STYX_MAX_MSG], out[STYX_MAX_MSG]; uint32_t il, ol;
    styx_build_tclunk(in, &il, 6, fid);
    return styxfs_serve(g_srv, in, il, out, &ol);
}

static int file_read_posix(const char *hostpath, char *buf, size_t n) {
    FILE *f = fopen(hostpath, "r"); if (!f) return -1;
    size_t r = fread(buf, 1, n - 1, f); buf[r] = '\0'; fclose(f); return (int)r;
}

static void test_ns_over_9p(void) {
    printf("\n-- /n served over REAL Styx/9P (end-to-end) --\n");

    /* host dir that is BOTH the bridge's root AND the 9P export root */
    char tmpl[] = "/tmp/wubu_ns_9p_XXXXXX";
    char *root = mkdtemp(tmpl);
    CHECK(root != NULL, "create 9P root dir");

    /* Use a stack server (like test_styxfs); mount host root at 9P "/". */
    styxfs_init(&g_srv_mem);
    g_srv = &g_srv_mem;
    CHECK(styxfs_mount(g_srv, "/", root, 0) == 0, "9P mount host root at /");
    CHECK(cli_version() == 0, "9P version handshake");
    CHECK(cli_attach(1) == 0, "9P attach to /");

    /* bridge publishes /n/snap into the same root */
    char store_tmpl[] = "/tmp/wubu_ns_9p_store_XXXXXX";
    char *store = mkdtemp(store_tmpl);
    WubuSnapshotManager *mgr = calloc(1, sizeof(WubuSnapshotManager));
    CHECK(mgr != NULL, "alloc snapshot manager");
    CHECK(wubu_snapshot_manager_init(store, mgr) == 0, "snapshot manager init");

    wubu_ns_bridge_create(root);
    CHECK(wubu_ns_publish_snapshots(mgr, "deck-root") == 0,
          "publish /n/snap/deck-root via bridge");

    /* (1) 9P client READs /snap/deck-root/list and sees empty (live) */
    const char *segl0[] = { "snap", "deck-root", "list" };
    CHECK(cli_walk(1, 2, segl0, 3) == 0, "9P walk to /snap/deck-root/list");
    CHECK(cli_open(2, STX_OREAD) == 0, "9P open list (read)");
    uint8_t rd[4096]; int n0 = cli_read(2, 0, rd, sizeof(rd) - 1);
    rd[n0 < 0 ? 0 : n0] = '\0';
    CHECK(n0 == 0, "9P read of list is empty before any snapshot (live view)");
    cli_clunk(2);

    /* (2) 9P client WRITEs a label to /snap/deck-root/create */
    const char *segc[] = { "snap", "deck-root", "create" };
    CHECK(cli_walk(1, 3, segc, 3) == 0, "9P walk to /snap/deck-root/create");
    CHECK(cli_open(3, STX_OWRITE) == 0, "9P open create (write)");
    const char *label = "pre-game-rollback";
    CHECK(cli_write(3, 0, (const uint8_t*)label, strlen(label)) == 0,
          "9P write label to /snap/deck-root/create");
    cli_clunk(3);
    /* the 9P write must land in the host file (proves /n write path) */
    char ctlpath[4096]; snprintf(ctlpath, sizeof(ctlpath),
        "%s/snap/deck-root/create", root);
    char cbuf[256];
    CHECK(file_read_posix(ctlpath, cbuf, sizeof(cbuf)) >= 0 &&
          strstr(cbuf, label) != NULL,
          "9P write is visible to a host reader (real /n write)");

    /* (3) control-plane consumer dispatches the write -> real snapshot */
    CHECK(wubu_ns_snap_create(mgr, "deck-root", label) == 0,
          "dispatch create -> wubu_snapshot_create");

    /* (4) 9P client READs list again -> sees the new snapshot (full loop) */
    CHECK(cli_walk(1, 4, segl0, 3) == 0, "9P walk to list (again)");
    CHECK(cli_open(4, STX_OREAD) == 0, "9P open list (again)");
    int n1 = cli_read(4, 0, rd, sizeof(rd) - 1);
    rd[n1 < 0 ? 0 : n1] = '\0';
    CHECK(n1 > 0 && strstr((char*)rd, label) != NULL,
          "9P read of list NOW shows the snapshot (live /n over 9P)");
    cli_clunk(4);

    cli_clunk(1);
    styxfs_unmount(g_srv, "/");
    wubu_snapshot_manager_shutdown(mgr); free(mgr);
    char cmd[9000]; snprintf(cmd, sizeof(cmd), "rm -rf %s %s", root, store);
    system(cmd);
}

int main(void) {
    test_ns_over_9p();
    printf("\n==================================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("==================================================\n");
    return g_fail == 0 ? 0 : 1;
}
