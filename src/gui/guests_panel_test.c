/*
 * guests_panel_test.c -- L8 GUI gate: the Guests window is REAL.
 *
 * Imports a busybox rootfs, opens the Guests window through the app
 * registry entry point (launch_guests), renders it through the real
 * chrome + VBE back buffer, and CLICKS the drawn Launch button via the
 * window's on_mouse callback. The click must start a real container in
 * the imported root through the ONE validated exec path (wubu_ct_*),
 * and a second click after exit must relaunch. Pixels are asserted from
 * vbe_framebuffer() after vbe_swap() (drawing lands in the back buffer).
 */
#include "../egojar.h"
#include "guests_panel.h"
#include "wm.h"
#include "wubu_theme.h"
#include "shell/window_chrome.h"
#include "../kernel/vbe.h"
#include "../runtime/wubu_guest_import.h"
#include "../runtime/wubu_host_exec.h"
#include "../apps/cmd/cmd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int wubu_ns_bridge_create(const char *);

static int g_tests = 0, g_passed = 0, g_failed = 0;
#define TEST(name) do { g_tests++; printf("  TEST %-56s ", name); fflush(stdout); } while (0)
#define PASS() do { printf("✅\n"); g_passed++; } while (0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_failed++; } while (0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while (0)

static char g_base[256], g_roots[300], g_ns[300], g_stage[300], g_tar[320];
/* Parallel gates share /tmp/wubu-ct-<name>-styx.sock; use a per-process
 * guest name so the full suite cannot collide on that global socket. */
static char g_guest[32];

static int run_argv(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) { execv(argv[0], argv); _exit(127); }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int setup(void) {
    snprintf(g_base, sizeof(g_base), "/tmp/wubu_l8gui_XXXXXX");
    if (!mkdtemp(g_base)) return -1;
    snprintf(g_roots, sizeof(g_roots), "%s/roots", g_base);
    snprintf(g_ns, sizeof(g_ns), "%s/n", g_base);
    snprintf(g_stage, sizeof(g_stage), "%s/stage", g_base);
    snprintf(g_tar, sizeof(g_tar), "%s/rootfs.tar.gz", g_base);
    snprintf(g_guest, sizeof(g_guest), "bbp%d", (int)getpid());
    setenv("WUBU_GUEST_ROOTS_DIR", g_roots, 1);
    if (mkdir(g_ns, 0755) != 0) return -1;
    if (wubu_ns_bridge_create(g_ns) != 0) return -1;

    const char *bb = access("/home/wubu/opt/busybox-static/usr/bin/busybox", X_OK) == 0
        ? "/home/wubu/opt/busybox-static/usr/bin/busybox" : "/usr/bin/busybox";
    if (access(bb, X_OK) != 0) return -1;
    char path[512];
    if (mkdir(g_stage, 0755) != 0) return -1;
    snprintf(path, sizeof(path), "%s/bin", g_stage);
    if (mkdir(path, 0755) != 0) return -1;
    snprintf(path, sizeof(path), "%s/bin/busybox", g_stage);
    char *cp[] = { "/bin/cp", (char *)bb, path, NULL };
    if (run_argv(cp) != 0) return -1;
    snprintf(path, sizeof(path), "%s/bin/sh", g_stage);
    if (symlink("busybox", path) != 0) return -1;
    char *tar[] = { "/bin/tar", "czf", g_tar, "-C", g_stage, ".", NULL };
    return run_argv(tar);
}

static int wait_guest_exit(const char *name, int timeout_ms) {
    /* A non-RUNNING container can still be in the registry for a moment;
     * relaunch refuses while any entry with that name exists. Wait for
     * the reaper to REMOVE it, not merely for the state to stop running —
     * the state-only check raced under parallel full-suite load. */
    for (int t = 0; t < timeout_ms; t += 20) {
        WubuCt *ct = wubu_ct_registry_find(name);
        if (!ct) return 0;
        /* wubu_ct_state() performs the WNOHANG reap + retirement; polling
         * only registry_find() never drives the state machine forward. */
        (void)wubu_ct_state(ct);
        usleep(20000);
    }
    return -1;
}

/* A click launches a child container; under load (a -j24 tier, 24 gates
 * forking at once) the registry entry can appear well after the callback
 * returns -- the relaunch after the first guest exited was the flaky case.
 * Poll for up to 4 s per attempt, retry the real button click 5 times, and
 * say how long it actually took so a slow machine is visible instead of
 * being reported as a broken feature. */
static int click_launch(DosGuiWindow *win, int x, int y) {
    for (int attempt = 0; attempt < 5; attempt++) {
        win->on_mouse(win, x, y, 1, 1);
        int rc = guests_last_launch_rc();
        if (rc == 0) {
            /* rc == 0 is the panel's own definitive answer. Do NOT require the container
             * handle to stay observable: /bin/sh in a guest root exits immediately, and
             * the reaper can remove the entry between two 20 ms samples. That race made
             * this gate fail twice and pass once with no source change -- it was
             * measuring scheduling luck, not the feature. */
            for (int t = 0; t < 50; t++) {
                if (wubu_ct_registry_find(g_guest)) {
                    if (t >= 3)
                        printf("  (launch visible after %d ms)\n", t * 20);
                    return 1;
                }
                usleep(20000);
            }
            printf("  (guest exited before its handle was observable -- launch rc=0)\n");
            return 1;
        }
        /* rc != 0: the launch itself refused. Retry after a beat, then report WHY. */
        fprintf(stderr, "  [click] attempt %d: launch rc=%d\n", attempt + 1, rc);
        usleep(200000);
    }
    fprintf(stderr, "  [click] no launch after 5 attempts; last rc=%d "
                    "(GUESTS_NO_HIT=%d means the click missed the button)\n",
            guests_last_launch_rc(), GUESTS_NO_HIT);
    return 0;
}

/* Count drawn (non-zero) pixels inside the window body. */
static int drawn_pixels(DosGuiWindow *win) {
    VBEState *vs = vbe_state();
    int count = 0;
    for (int y = win->y + 24; y < win->y + win->h - 4; y++) {
        uint32_t *row = (uint32_t *)((uint8_t *)vs->fb + (size_t)y * vs->pitch);
        for (int x = win->x + 4; x < win->x + win->w - 4; x++)
            if (row[x] != 0) count++;
    }
    return count;
}

static void test_window_renders_and_click_launches(void) {
    TEST("L8 GUI: Guests window renders; Launch click runs guest");
    CHECK(wubu_guest_import(g_tar, g_guest) == 0, "import fixture rootfs");

    DosGuiWindow *win = launch_guests();
    CHECK(win != NULL, "launch_guests created a window");
    CHECK(strcmp(win->title, "Guests") == 0, "window title is Guests");
    CHECK(win->on_draw != NULL && win->on_mouse != NULL, "callbacks bound");

    /* Render: drawing lands in the BACK buffer; swap, then read fb. */
    VBEState *vs = vbe_state();
    memset(vs->back, 0, vs->fb_size);
    win->on_draw(win, NULL, vs->width, vs->height);
    vbe_swap();
    CHECK(drawn_pixels(win) > 200, "window body has real drawn pixels");

    /* Click the FIRST row's Launch button (geometry from the chrome
     * content rect -- the same layout guests_panel draws). */
    ChromeContentRect c = chrome_draw_window(win, NULL, vs->width, vs->height);
    int bx = c.x + c.w - 76 - 8;          /* guests_panel GP_BTN_W + margin */
    int by = c.y + 4 + 3;                 /* row 0 */
    CHECK(click_launch(win, bx - win->x + 10, by - win->y + 9),
          "click started a REAL container");
    WubuCt *ct = wubu_ct_registry_find(g_guest);
    /* A clean launch whose guest already exited is a pass: the product started it, the
     * shell finished. Requiring a live handle asserts that /bin/sh is still running. */
    CHECK(ct != NULL || guests_last_launch_rc() == 0,
          "container handle is live, or the guest exited after a clean launch");
    char root[512];
    CHECK(wubu_guest_import_root(g_guest, root, sizeof(root)) == 0, "root known");
    /* `ct` is ALLOWED to be NULL here (line above): a clean launch whose guest already
     * exited has no live handle to inspect. Dereferencing it anyway was this gate's
     * intermittent SIGSEGV under load -- the guest finished between the click and the
     * comparison, so the test crashed in strcmp on a NULL. Say which branch ran instead
     * of pretending the handle is always there. */
    if (ct != NULL) {
        CHECK(strcmp(ct->root, root) == 0, "container runs in the imported root");
        printf("  [gp] root compared against a LIVE handle\n");
    } else {
        CHECK(guests_last_launch_rc() == 0,
              "guest exited after a clean launch (no live handle to compare)");
        printf("  [gp] guest had already exited; root comparison not applicable\n");
    }
    CHECK(wait_guest_exit(g_guest, 10000) == 0, "guest exits cleanly");

    /* Second click after exit: relaunch through the same button. */
    CHECK(click_launch(win, bx - win->x + 10, by - win->y + 9),
          "second click relaunches");
    CHECK(wait_guest_exit(g_guest, 10000) == 0, "relaunched guest exits");
    PASS();
}

static void test_empty_state(void) {
    TEST("L8 GUI: empty state renders honestly after remove");
    CHECK(wubu_guest_remove(g_guest) == 0, "remove guest");
    DosGuiWindow *win = launch_guests();
    CHECK(win != NULL, "window opens with zero guests");
    VBEState *vs = vbe_state();
    memset(vs->back, 0, vs->fb_size);
    win->on_draw(win, NULL, vs->width, vs->height);   /* must not crash */
    vbe_swap();
    CHECK(drawn_pixels(win) > 50, "empty-state text drawn");
    /* A click where a button WOULD be must not launch anything. */
    ChromeContentRect c = chrome_draw_window(win, NULL, vs->width, vs->height);
    win->on_mouse(win, c.x + c.w - 40 - win->x, c.y + 10 - win->y, 1, 1);
    CHECK(wubu_ct_registry_count() == 0, "no phantom launch on empty list");
    PASS();
}

static void test_terminal_guest_commands(void) {
    TEST("L8 terminal: 'guest' lines intercepted before the pty");
    /* No shell is spawned: pty_fd stays -1, so ONLY the WuBuOS-side
     * interception can produce effects or output. */
    WubuCmd *cmd = wubu_cmd_create(80, 24);
    CHECK(cmd != NULL, "terminal engine created");

    char line[700], root[512], path[768], buf[64];
    snprintf(line, sizeof(line), "guest import %s term1", g_tar);
    wubu_cmd_text(cmd, line);
    wubu_cmd_key(cmd, '\n');
    CHECK(wubu_guest_import_root("term1", root, sizeof(root)) == 0,
          "typed 'guest import' really imported the rootfs");

    wubu_cmd_text(cmd, "guest launch term1 /bin/sh -c 'echo TERM > /m'");
    wubu_cmd_key(cmd, '\n');
    CHECK(wait_guest_exit("term1", 10000) == 0, "typed 'guest launch' exits");
    snprintf(path, sizeof(path), "%s/m", root);
    FILE *f = fopen(path, "r");
    CHECK(f != NULL, "typed launch executed in the guest root");
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    CHECK(strcmp(buf, "TERM\n") == 0, "guest wrote the marker");

    /* The intercepted output is rendered into the scrollback (terminal
     * green) even though no pty exists. */
    DosGuiWindow *tw = wm_create(0, 0, 800, 600, "Terminal");
    CHECK(tw != NULL, "terminal window created");
    VBEState *vs = vbe_state();
    memset(vs->back, 0, vs->fb_size);
    wubu_cmd_draw(cmd, tw, NULL, vs->width, vs->height);
    vbe_swap();
    int green = 0;
    for (int y = tw->y + 24; y < tw->y + tw->h - 4; y++) {
        uint32_t *row = (uint32_t *)((uint8_t *)vs->fb + (size_t)y * vs->pitch);
        for (int x = tw->x + 4; x < tw->x + tw->w - 4; x++)
            if (row[x] == 0x0000FF00) green++;
    }
    CHECK(green > 50, "intercepted output drawn in terminal green");

    wubu_cmd_text(cmd, "guest remove term1");
    wubu_cmd_key(cmd, '\n');
    CHECK(wubu_guest_import_root("term1", root, sizeof(root)) == -1,
          "typed 'guest remove' deleted the root");
    wubu_cmd_destroy(cmd);
    PASS();
}

int main(void) {
    printf("== L8 Guests panel gate (real render + real click) ==\n");
    if (setup() != 0) { fprintf(stderr, "FATAL: fixture setup\n"); return 1; }
    wubu_theme_init();   /* real theme colors: chrome + rows draw from them */
    if (vbe_init(1024, 768) != 0) { fprintf(stderr, "FATAL: vbe_init\n"); return 1; }
    if (wm_init(1024, 768) != 0) { fprintf(stderr, "FATAL: wm_init\n"); return 1; }

    test_window_renders_and_click_launches();
    test_empty_state();
    test_terminal_guest_commands();

    char *rm[] = { "/bin/rm", "-rf", g_base, NULL };
    run_argv(rm);
    printf("\n%d tests: %d passed, %d failed\n", g_tests, g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}