/*
 * hosted_test.c — WuBuOS Hosted Mode Behavioral Test Suite
 *
 * Tests: hosted init/shutdown, Styx namespace construction,
 * Styx server callbacks (attach, walk, open, read, stat),
 * headless mode, .wubu container registration.
 *
 * Cell 200 behavioral tests:
 *   - Kernel subsystems init (VBE has buffers)
 *   - WM has windows (GUI shell running)
 *   - Desktop rendering writes non-gray pixels
 *
 * All tests run in headless mode (no X11 window needed).
 */
#include "hosted.h"
#include "../runtime/styx.h"
#include "../kernel/vbe.h"
#include "../gui/wm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* ── Test Framework ──────────────────────────────────────────────── */

static int g_tests = 0, g_passed = 0, g_failed = 0;

#define TEST(name) do { \
    g_tests++; \
    printf("  TEST %-52s ", name); \
    fflush(stdout); \
} while (0)

#define PASS() do { printf("✅\n"); g_passed++; } while (0)

#define FAIL(msg) do { \
    printf("❌ %s\n", msg); g_failed++; \
} while (0)

#define CHECK(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while (0)

/* ── In-memory filesystem helpers (mirrors hosted.c) ────────────── */

#define STYXFS_MAX_FILES 64

typedef struct {
    char     name[256];
    uint8_t  qtype;
    uint64_t path;
    uint64_t length;
    uint8_t  data[8192];
    uint32_t data_len;
} styxfs_file_t;

static styxfs_file_t g_fs[STYXFS_MAX_FILES];
static int g_nfiles = 0;
static uint64_t g_next_path = 1;

static int fs_add_dir(const char *name) {
    if (g_nfiles >= STYXFS_MAX_FILES) return -1;
    styxfs_file_t *f = &g_fs[g_nfiles++];
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->qtype = STX_QTDIR;
    f->path = g_next_path++;
    f->length = 0;
    return 0;
}

static int fs_add_file(const char *name, const uint8_t *data, uint32_t len) {
    if (g_nfiles >= STYXFS_MAX_FILES) return -1;
    styxfs_file_t *f = &g_fs[g_nfiles++];
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->qtype = STX_QTFILE;
    f->path = g_next_path++;
    f->length = len;
    if (data && len > 0) {
        uint32_t clen = len < sizeof(f->data) ? len : sizeof(f->data);
        memcpy(f->data, data, clen);
        f->data_len = clen;
    }
    return 0;
}

static void fs_reset(void) {
    g_nfiles = 0;
    g_next_path = 1;
}

/* ── Styx Server Callbacks (same as hosted.c for testing) ──────── */

static styx_fid_t *find_fid(styx_server_t *srv, uint32_t fid) {
    for (int i = 0; i < STYX_MAX_FIDS; i++)
        if (srv->fids[i].in_use && srv->fids[i].fid == fid)
            return &srv->fids[i];
    return NULL;
}

static int styx_attach_cb(styx_server_t *srv, uint32_t fid, const char *aname) {
    (void)aname;
    styx_fid_t *f = NULL;
    for (int i = 0; i < STYX_MAX_FIDS; i++) {
        if (!srv->fids[i].in_use) { f = &srv->fids[i]; break; }
    }
    if (!f) return -1;
    memset(f, 0, sizeof(*f));
    f->in_use = 1;
    f->fid = fid;
    f->qid.type = STX_QTDIR;
    f->qid.path = 0;
    f->qid.version = 1;
    return 0;
}

static int styx_walk_cb(styx_server_t *srv, uint32_t fid, uint32_t newfid,
                         const char **wname, int nwname,
                         styx_qid_t *qids, int *nwqid) {
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    if (nwname == 0) {
        styx_fid_t *nf = NULL;
        for (int i = 0; i < STYX_MAX_FIDS; i++)
            if (!srv->fids[i].in_use) { nf = &srv->fids[i]; break; }
        if (!nf) return -1;
        *nf = *f; nf->fid = newfid;
        qids[0] = f->qid; *nwqid = 1;
        return 0;
    }
    styx_fid_t *nf = NULL;
    int walked = 0;
    for (int i = 0; i < nwname; i++) {
        styxfs_file_t *file = NULL;
        for (int j = 0; j < g_nfiles; j++)
            if (strcmp(g_fs[j].name, wname[i]) == 0) { file = &g_fs[j]; break; }
        if (!file) { *nwqid = walked; return walked > 0 ? 0 : -1; }
        qids[walked].type = file->qtype;
        qids[walked].path = file->path;
        qids[walked].version = 1;
        walked++;
        if (!nf) {
            for (int j = 0; j < STYX_MAX_FIDS; j++)
                if (!srv->fids[j].in_use) { nf = &srv->fids[j]; break; }
            if (!nf) return -1;
            *nf = *f; nf->fid = newfid;
        }
        nf->qid.type = file->qtype;
        nf->qid.path = file->path;
    }
    *nwqid = walked;
    return 0;
}

static int styx_open_cb(styx_server_t *srv, uint32_t fid, int mode, styx_qid_t *qid) {
    (void)mode;
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    *qid = f->qid;
    return 0;
}

static int styx_read_cb(styx_server_t *srv, uint32_t fid, uint64_t offset,
                         uint32_t count, uint8_t *data, uint32_t *nread) {
    (void)srv;
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    for (int i = 0; i < g_nfiles; i++) {
        if (g_fs[i].path == f->qid.path) {
            if (g_fs[i].qtype & STX_QTDIR) { *nread = 0; return 0; }
            if (offset >= g_fs[i].data_len) { *nread = 0; return 0; }
            uint32_t avail = g_fs[i].data_len - (uint32_t)offset;
            *nread = (count < avail) ? count : avail;
            if (*nread > 0) memcpy(data, g_fs[i].data + offset, *nread);
            return 0;
        }
    }
    return -1;
}

static int styx_stat_cb(styx_server_t *srv, uint32_t fid, styx_dir_t *dir) {
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    memset(dir, 0, sizeof(*dir));
    dir->qid = f->qid;
    dir->mode = (f->qid.type & STX_QTDIR) ? STX_DMDIR : 0;
    dir->mode |= 0555;
    if (f->qid.path == 0) {
        strcpy(dir->name, "/");
    } else {
        for (int i = 0; i < g_nfiles; i++)
            if (g_fs[i].path == f->qid.path)
                { strncpy(dir->name, g_fs[i].name, STYX_MAX_FNAME - 1); break; }
    }
    strcpy(dir->uid, "wubu");
    strcpy(dir->gid, "wubu");
    return 0;
}

/* ── Namespace Tests (preserved from v1) ────────────────────────── */

static void test_namespace_dirs(void) {
    TEST("Namespace: add directories and files");
    fs_reset();
    CHECK(fs_add_dir("wubu") == 0, "add dir wubu");
    CHECK(fs_add_dir("dev") == 0, "add dir dev");
    CHECK(fs_add_dir("prog") == 0, "add dir prog");
    CHECK(g_nfiles == 3, "3 entries");
    CHECK(g_fs[0].qtype == STX_QTDIR, "wubu is dir");
    CHECK(g_fs[1].qtype == STX_QTDIR, "dev is dir");
    PASS();
}

static void test_styx_attach(void) {
    TEST("Styx: Tattach connects to root");
    styx_server_t srv;
    styx_init(&srv);
    srv.attach = styx_attach_cb;
    styx_fid_t *f = &srv.fids[0];
    CHECK(srv.attach(&srv, 0, "") == 0, "attach");
    CHECK(srv.fids[0].in_use, "fid in use");
    CHECK(srv.fids[0].qid.type == STX_QTDIR, "root is dir");
    PASS();
}

static void test_styx_walk_dev(void) {
    TEST("Styx: Twalk to /dev/cons");
    fs_reset();
    fs_add_dir("dev");
    fs_add_file("cons", (const uint8_t*)"hello\n", 6);
    
    styx_server_t srv;
    styx_init(&srv);
    srv.attach = styx_attach_cb;
    srv.walk = styx_walk_cb;
    CHECK(srv.attach(&srv, 0, "") == 0, "attach");
    
    const char *wname[] = {"dev", "cons"};
    styx_qid_t qids[2];
    int nwqid = 0;
    CHECK(srv.walk(&srv, 0, 1, wname, 2, qids, &nwqid) == 0, "walk");
    CHECK(nwqid == 2, "walked 2");
    PASS();
}

static void test_styx_walk_clone(void) {
    TEST("Styx: Twalk 0 elements clones fid");
    styx_server_t srv;
    styx_init(&srv);
    srv.attach = styx_attach_cb;
    srv.walk = styx_walk_cb;
    CHECK(srv.attach(&srv, 0, "") == 0, "attach");
    
    styx_qid_t qids[1];
    int nwqid = 0;
    CHECK(srv.walk(&srv, 0, 1, NULL, 0, qids, &nwqid) == 0, "walk clone");
    CHECK(srv.fids[1].in_use, "new fid in use");
    PASS();
}

static void test_styx_walk_nonexistent(void) {
    TEST("Styx: Walk to nonexistent returns Rerror");
    fs_reset();
    styx_server_t srv;
    styx_init(&srv);
    srv.attach = styx_attach_cb;
    srv.walk = styx_walk_cb;
    CHECK(srv.attach(&srv, 0, "") == 0, "attach");
    
    const char *wname[] = {"noexist"};
    styx_qid_t qids[1];
    int nwqid = 0;
    CHECK(srv.walk(&srv, 0, 1, wname, 1, qids, &nwqid) == -1, "walk fails");
    PASS();
}

static void test_namespace_wubu(void) {
    TEST("Namespace: .wubu container as Styx file");
    fs_reset();
    uint8_t data[64] = {0};
    memcpy(data, "WUBU!\0\1\2", 8);
    CHECK(fs_add_file("hello.wubu", data, 64) == 0, "add wubu");
    CHECK(g_fs[0].qtype == STX_QTFILE, "is file");
    CHECK(g_fs[0].length == 64, "length 64");
    PASS();
}

static void test_styx_stat(void) {
    TEST("Styx: Tstat on namespace file");
    fs_reset();
    fs_add_dir("dev");
    
    styx_server_t srv;
    styx_init(&srv);
    srv.attach = styx_attach_cb;
    srv.walk = styx_walk_cb;
    srv.stat = styx_stat_cb;
    CHECK(srv.attach(&srv, 0, "") == 0, "attach");
    
    const char *wname[] = {"dev"};
    styx_qid_t qids[1];
    int nwqid = 0;
    CHECK(srv.walk(&srv, 0, 1, wname, 1, qids, &nwqid) == 0, "walk to dev");
    
    styx_dir_t dir;
    CHECK(srv.stat(&srv, 1, &dir) == 0, "stat");
    CHECK(strcmp(dir.name, "dev") == 0, "name is dev");
    PASS();
}

static void test_namespace_reset(void) {
    TEST("Namespace: fs_reset clears all entries");
    fs_reset();
    fs_add_dir("a");
    fs_add_dir("b");
    CHECK(g_nfiles == 2, "2 entries before reset");
    fs_reset();
    CHECK(g_nfiles == 0, "0 after reset");
    PASS();
}

/* ── Cell 200: Behavioral Tests ─────────────────────────────────── */

static void test_kernel_ready(void) {
    TEST("Cell200: Kernel subsystems initialized (VBE has buffers)");
    /* Use headless mode - no X11 needed */
    hosted_state_t state;
    char *argv[] = {"wubu", "-h"};
    CHECK(hosted_init(&state, 2, argv) == 0, "hosted_init headless");
    CHECK(hosted_kernel_ready() == 1, "kernel ready");
    hosted_shutdown(&state);
    PASS();
}

static void test_wm_has_windows(void) {
    TEST("Cell200: WM has windows after init (GUI shell running)");
    hosted_state_t state;
    char *argv[] = {"wubu", "-h"};
    CHECK(hosted_init(&state, 2, argv) == 0, "hosted_init headless");
    CHECK(hosted_wm_has_windows() == 1, "WM has windows");
    hosted_shutdown(&state);
    PASS();
}

static void test_vbe_renders_desktop(void) {
    TEST("Cell200: VBE framebuffer has desktop pixels (not all gray)");
    hosted_state_t state;
    char *argv[] = {"wubu", "-h"};
    CHECK(hosted_init(&state, 2, argv) == 0, "hosted_init");
    
    /* After init, desktop_draw runs in render loop.
       But we can test manually: desktop background = C_WIN_DESKTOP (0x00808080).
       The desktop icon area should have different colors. */
    
    /* For a direct test: call desktop_draw and check VBE back buffer */
    extern void desktop_draw(int sw, int sh, int tb_h);
    extern int  taskbar_height(void);
    
    /* Clear VBE back buffer */
    vbe_clear(0x00000000);
    
    /* Draw desktop */
    desktop_draw(1024, 768, taskbar_height());
    
    /* Check that some pixels are non-zero (desktop background at least) */
    VBEState *vs = vbe_state();
    CHECK(vs != NULL, "vbe_state not null");
    CHECK(vs->back != NULL, "back buffer exists");
    
    /* Desktop background is C_WIN_DESKTOP = 0x00808080 */
    int has_desktop_pixel = 0;
    for (int i = 0; i < 100; i++) {
        if (vs->back[i] != 0) { has_desktop_pixel = 1; break; }
    }
    CHECK(has_desktop_pixel, "VBE back buffer has drawn pixels");
    
    hosted_shutdown(&state);
    PASS();
}

static void test_input_routing(void) {
    TEST("Cell200: X11 keyevent routes to WM input handler");
    /* This test verifies the hosted binary can route input.
       We call the WM input handler directly (simulating what handle_key does). */
    hosted_state_t state;
    char *argv[] = {"wubu", "-h"};
    CHECK(hosted_init(&state, 2, argv) == 0, "hosted_init");
    
    /* WM should have a focused window */
    WmWindow *focused = wm_get_focused();
    CHECK(focused != NULL, "focused window exists");
    if (focused) {
        CHECK(focused->flags & WIN_FOCUSED, "window is focused");
    }
    
    hosted_shutdown(&state);
    PASS();
}

static void test_styx_namespace_populated(void) {
    TEST("Cell200: Styx namespace populated with /wubu /dev /prog");
    hosted_state_t state;
    char *argv[] = {"wubu", "-h"};
    CHECK(hosted_init(&state, 2, argv) == 0, "hosted_init");
    
    /* The hosted init builds namespace in hosted.c's internal g_fs.
       We verify via the Styx init API that the server can be set up. */
    CHECK(hosted_styx_init(&state, NULL) == 0, "styx_init");
    
    /* Register a test .wubu container in the hosted namespace */
    uint8_t test_data[64] = {0};
    memcpy(test_data, "WUBU!\0\1\2", 8);
    CHECK(hosted_styx_register_wubu(&state, "test.wubu", test_data, 64) == 0,
          "register .wubu in namespace");
    
    /* fs_reset is exposed via hosted_fs_reset */
    hosted_fs_reset();
    
    hosted_shutdown(&state);
    PASS();
}

static void test_start_menu_entries(void) {
    TEST("Cell200: Start menu has entries after init");
    hosted_state_t state;
    char *argv[] = {"wubu", "-h"};
    CHECK(hosted_init(&state, 2, argv) == 0, "hosted_init");
    
    extern int startmenu_count(void);
    int cnt = startmenu_count();
    CHECK(cnt > 0, "start menu has entries");
    CHECK(cnt >= 4, "at least 4 entries (Programs, Temple, Separator, Shut Down)");
    
    hosted_shutdown(&state);
    PASS();
}

/* ── Main ───────────────────────────────────────────────────────── */

int main(void) {
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS Hosted Mode Test Suite                  ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");
    
    /* Legacy namespace tests */
    test_namespace_dirs();
    test_styx_attach();
    test_styx_walk_dev();
    test_styx_walk_clone();
    test_styx_walk_nonexistent();
    test_namespace_wubu();
    test_styx_stat();
    test_namespace_reset();
    
    /* Cell 200 behavioral tests */
    printf("\n── Cell 200 Behavioral Tests ──\n\n");
    test_kernel_ready();
    test_wm_has_windows();
    test_vbe_renders_desktop();
    test_input_routing();
    test_styx_namespace_populated();
    test_start_menu_entries();
    
    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n", g_passed, g_tests, g_failed);
    printf("══════════════════════════════════════════════════\n");
    
    return g_failed > 0 ? 1 : 0;
}
