/*
 * wubu_guest_import_test.c -- L8 gate: unified rootfs import -> launch.
 *
 * Builds a REAL minimal rootfs (static busybox + /bin/sh symlink + marker)
 * as a tar.gz, imports it through wubu_guest_import (which delegates to
 * wubu_rd_load), asserts /n/guests publication, EXECUTES through the guest
 * via the L9 validated path (wubu_ct_*), asserts the marker the guest
 * wrote, relaunches after exit, exercises the 9P control writes
 * (guests/import + guests/<name>/launch), directory import, tar.zst and
 * cpio.gz round-trips, removal, and honest refusal of bad archives/names.
 *
 * No network. busybox: /home/wubu/opt/busybox-static/usr/bin/busybox,
 * falling back to /usr/bin/busybox (both static on the reference box).
 */
#include "../egojar.h"
#include "wubu_guest_import.h"
#include "wubu_host_exec.h"
#include "wubu_ns_guests.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

extern int wubu_ns_bridge_create(const char *);

/* -- Test framework (same shape as the L9 gate) ---------------------- */

static int g_tests = 0, g_passed = 0, g_failed = 0;

#define TEST(name) do { \
    g_tests++; \
    printf("  TEST %-56s ", name); \
    fflush(stdout); \
} while (0)

#define PASS() do { printf("✅\n"); g_passed++; } while (0)

#define FAIL(msg) do { printf("❌ %s\n", msg); g_failed++; } while (0)

#define CHECK(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while (0)

/* -- Fixture paths ---------------------------------------------------- */

static char g_base[256];      /* mkdtemp root */
static char g_roots[300];     /* managed roots dir (WUBU_GUEST_ROOTS_DIR) */
static char g_ns[300];        /* /n root */
static char g_stage[300];     /* rootfs staging dir */
static char g_tar[320];       /* rootfs.tar.gz */
static char g_busybox[256];

static int run_argv(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execv(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return (WIFEXITED(status)) ? WEXITSTATUS(status) : -1;
}

static int write_text(const char *path, const char *text) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

/* Read a whole small file; returns length or -1. */
static int read_text(const char *path, char *out, size_t outsz) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(out, 1, outsz - 1, f);
    fclose(f);
    out[n] = '\0';
    return (int)n;
}

static int file_has(const char *path, const char *needle) {
    char buf[8192];
    if (read_text(path, buf, sizeof(buf)) < 0) return 0;
    return strstr(buf, needle) != NULL;
}

/* Poll the LIVE registry until the named guest is gone (exited, retired,
 * unregistered). wubu_ct_state() is what drives retirement -- the same
 * poll the /n/guests sync does. */
static int wait_guest_exit(const char *name, int timeout_ms) {
    for (int t = 0; t < timeout_ms; t += 20) {
        WubuCt *ct = wubu_ct_registry_find(name);
        if (!ct) return 0;
        if (wubu_ct_state(ct) != CT_RUNNING) return 0;
        usleep(20000);
    }
    return -1;
}

/* Launch + wait + assert the guest wrote `expect` into <root>/<file>. */
static int launch_and_check(const char *name, const char *cmd,
                            const char *file, const char *expect) {
    if (wubu_guest_launch(name, cmd) != 0) return -1;
    if (wait_guest_exit(name, 10000) != 0) return -2;
    char root[512], path[768], buf[256];
    if (wubu_guest_import_root(name, root, sizeof(root)) != 0) return -3;
    snprintf(path, sizeof(path), "%s/%s", root, file);
    if (read_text(path, buf, sizeof(buf)) < 0) return -4;
    if (strcmp(buf, expect) != 0) return -5;
    return 0;
}

static void rm_rf(const char *path) {
    char *argv[] = { "/bin/rm", "-rf", (char *)path, NULL };
    run_argv(argv);
}

/* -- Setup ------------------------------------------------------------ */

static int setup(void) {
    snprintf(g_base, sizeof(g_base), "/tmp/wubu_l8_XXXXXX");
    if (!mkdtemp(g_base)) return -1;
    snprintf(g_roots, sizeof(g_roots), "%s/roots", g_base);
    snprintf(g_ns, sizeof(g_ns), "%s/n", g_base);
    snprintf(g_stage, sizeof(g_stage), "%s/stage", g_base);
    snprintf(g_tar, sizeof(g_tar), "%s/rootfs.tar.gz", g_base);
    setenv("WUBU_GUEST_ROOTS_DIR", g_roots, 1);
    unsetenv("WUBU_GUESTS_LAUNCH_ROOT");
    if (mkdir(g_ns, 0755) != 0) return -1;
    if (wubu_ns_bridge_create(g_ns) != 0) return -1;

    /* Static busybox: the reference box keeps it under ~/opt; the L9
     * gate's /usr/bin/busybox is the fallback. */
    const char *bb1 = "/home/wubu/opt/busybox-static/usr/bin/busybox";
    const char *bb2 = "/usr/bin/busybox";
    if (access(bb1, X_OK) == 0) snprintf(g_busybox, sizeof(g_busybox), "%s", bb1);
    else if (access(bb2, X_OK) == 0) snprintf(g_busybox, sizeof(g_busybox), "%s", bb2);
    else return -1;

    /* Stage the rootfs: bin/busybox + bin/sh symlink + a marker file. */
    char path[512];
    if (mkdir(g_stage, 0755) != 0) return -1;
    snprintf(path, sizeof(path), "%s/bin", g_stage);
    if (mkdir(path, 0755) != 0) return -1;
    snprintf(path, sizeof(path), "%s/bin/busybox", g_stage);
    char *cp[] = { "/bin/cp", g_busybox, path, NULL };
    if (run_argv(cp) != 0) return -1;
    snprintf(path, sizeof(path), "%s/bin/sh", g_stage);
    if (symlink("busybox", path) != 0) return -1;
    snprintf(path, sizeof(path), "%s/rootfs-marker.txt", g_stage);
    if (write_text(path, "L8-ROOTFS-MARKER\n") != 0) return -1;

    /* tar.gz of the staging dir (relative entries: ./bin/... ). */
    char *tar[] = { "/bin/tar", "czf", g_tar, "-C", g_stage, ".", NULL };
    if (run_argv(tar) != 0) return -1;
    return 0;
}

/* -- Tests ------------------------------------------------------------ */

static void test_import_tar_gz(void) {
    TEST("L8: import tar.gz -> managed root + /n/guests");
    CHECK(wubu_guest_import(g_tar, "bb") == 0, "import tar.gz succeeds");
    char root[512], path[768];
    CHECK(wubu_guest_import_root("bb", root, sizeof(root)) == 0, "root registered");
    snprintf(path, sizeof(path), "%s/bin/sh", root);
    CHECK(access(path, X_OK) == 0, "guest /bin/sh present + executable");
    snprintf(path, sizeof(path), "%s/rootfs-marker.txt", root);
    CHECK(file_has(path, "L8-ROOTFS-MARKER"), "marker file extracted");
    snprintf(path, sizeof(path), "%s/.wubu-guest", root);
    CHECK(access(path, F_OK) == 0, "ownership marker written");

    /* /n/guests publication: list + per-guest state + import control. */
    snprintf(path, sizeof(path), "%s/guests/list", g_ns);
    CHECK(file_has(path, "bb\t"), "guests/list shows the imported name");
    CHECK(file_has(path, "imported"), "guests/list state is imported");
    snprintf(path, sizeof(path), "%s/guests/bb/state", g_ns);
    CHECK(file_has(path, "state: imported"), "guests/bb/state published");
    snprintf(path, sizeof(path), "%s/guests/bb/launch", g_ns);
    CHECK(access(path, F_OK) == 0, "guests/bb/launch control file exists");
    snprintf(path, sizeof(path), "%s/guests/import", g_ns);
    CHECK(access(path, F_OK) == 0, "guests/import control file exists");

    /* Scan accessors agree with the namespace. */
    int n = wubu_guest_import_count();
    CHECK(n >= 1, "import count >= 1");
    char name[64], state[16];
    CHECK(wubu_guest_import_entry(0, name, sizeof(name), NULL, 0,
                                  state, sizeof(state)) == 0, "entry 0 readable");
    CHECK(strcmp(name, "bb") == 0, "entry 0 is bb");
    CHECK(strcmp(state, "imported") == 0, "entry 0 state imported");
    PASS();
}

static void test_launch_executes_in_guest(void) {
    TEST("L8: launch executes INSIDE the imported root");
    int rc = launch_and_check("bb",
        "/bin/sh -c 'echo GUEST-IMPORT-OK > /marker'",
        "marker", "GUEST-IMPORT-OK\n");
    CHECK(rc == 0, "guest wrote /marker through the chroot exec path");
    /* The marker must be in the GUEST root, not the host cwd. */
    CHECK(access("marker", F_OK) != 0, "no marker leaked into host cwd");
    PASS();
}

static void test_relaunch_after_exit(void) {
    TEST("L8: relaunch after exit works");
    int rc = launch_and_check("bb",
        "/bin/sh -c 'echo ROUND2 > /marker2'",
        "marker2", "ROUND2\n");
    CHECK(rc == 0, "second launch executes and writes marker2");
    char path[768];
    snprintf(path, sizeof(path), "%s/guests/list", g_ns);
    CHECK(file_has(path, "bb\t"), "bb still listed after exit");
    CHECK(file_has(path, "imported"), "state back to imported after exit");
    PASS();
}

static void test_9p_launch_write(void) {
    TEST("L8: 9P write guests/<name>/launch runs the guest");
    const char *cmd = "/bin/sh -c 'echo VIA9P > /marker3'";
    uint32_t nw = 0;
    int rc = wubu_ns_guests_write("guests/bb/launch",
                                  (const uint8_t *)cmd,
                                  (uint32_t)strlen(cmd), &nw);
    CHECK(rc == 1, "launch write handled");
    CHECK(nw == strlen(cmd), "write consumed the whole command");
    CHECK(wait_guest_exit("bb", 10000) == 0, "guest exited");
    char root[512], path[768], buf[64];
    CHECK(wubu_guest_import_root("bb", root, sizeof(root)) == 0, "root found");
    snprintf(path, sizeof(path), "%s/marker3", root);
    CHECK(read_text(path, buf, sizeof(buf)) > 0, "marker3 exists");
    CHECK(strcmp(buf, "VIA9P\n") == 0, "marker3 written by the 9P-launched guest");
    PASS();
}

static void test_9p_import_write(void) {
    TEST("L8: 9P write guests/import imports a rootfs");
    char args[700], path[768];
    snprintf(args, sizeof(args), "%s bb2", g_tar);
    uint32_t nw = 0;
    int rc = wubu_ns_guests_write("guests/import",
                                  (const uint8_t *)args,
                                  (uint32_t)strlen(args), &nw);
    CHECK(rc == 1, "import write handled");
    CHECK(nw == strlen(args), "write consumed the whole body");
    snprintf(path, sizeof(path), "%s/guests/list", g_ns);
    CHECK(file_has(path, "bb2\t"), "guests/list shows bb2");
    int rcl = launch_and_check("bb2",
        "/bin/sh -c 'echo BB2 > /m'", "m", "BB2\n");
    CHECK(rcl == 0, "bb2 launches into its OWN root");
    PASS();
}

static void test_directory_import(void) {
    TEST("L8: directory import + launch");
    CHECK(wubu_guest_import(g_stage, "dirg") == 0, "directory import succeeds");
    char path[768];
    snprintf(path, sizeof(path), "%s/guests/list", g_ns);
    CHECK(file_has(path, "dirg\t"), "guests/list shows dirg");
    int rc = launch_and_check("dirg",
        "/bin/sh -c 'echo DIRG > /m'", "m", "DIRG\n");
    CHECK(rc == 0, "dir-imported guest executes");
    PASS();
}

static void test_tar_zst_import(void) {
    TEST("L8: tar.zst import + launch");
    char zst[320];
    snprintf(zst, sizeof(zst), "%s/rootfs.tar.zst", g_base);
    char *tar[] = { "/bin/tar", "--zstd", "-cf", zst, "-C", g_stage, ".", NULL };
    CHECK(run_argv(tar) == 0, "build tar.zst fixture");
    CHECK(wubu_guest_import(zst, "zg") == 0, "tar.zst import succeeds");
    int rc = launch_and_check("zg",
        "/bin/sh -c 'echo ZST > /m'", "m", "ZST\n");
    CHECK(rc == 0, "zstd-imported guest executes");
    PASS();
}

static void test_cpio_gz_import(void) {
    TEST("L8: cpio.gz import + launch");
    char cgz[320], cmd[900];
    snprintf(cgz, sizeof(cgz), "%s/rootfs.cpio.gz", g_base);
    /* find | cpio -o -H newc | gzip -- the initramfs shape rd_load carries. */
    snprintf(cmd, sizeof(cmd),
             "cd '%s' && find . | cpio -o -H newc 2>/dev/null | gzip > '%s'",
             g_stage, cgz);
    char *sh[] = { "/bin/sh", "-c", cmd, NULL };
    CHECK(run_argv(sh) == 0, "build cpio.gz fixture");
    CHECK(wubu_guest_import(cgz, "cg") == 0, "cpio.gz import succeeds");
    int rc = launch_and_check("cg",
        "/bin/sh -c 'echo CPIO > /m'", "m", "CPIO\n");
    CHECK(rc == 0, "cpio-imported guest executes");
    PASS();
}

static void test_running_state_and_remove_refusal(void) {
    TEST("L8: running state visible; remove refused while running");
    CHECK(wubu_guest_launch("bb", "/bin/busybox sleep 1") == 0, "sleep guest starts");
    CHECK(wubu_ct_registry_find("bb") != NULL, "live registry holds bb");
    char name[64], state[16], path[768];
    int found = 0;
    for (int i = 0; i < wubu_guest_import_count(); i++) {
        if (wubu_guest_import_entry(i, name, sizeof(name), NULL, 0,
                                    state, sizeof(state)) == 0 &&
            strcmp(name, "bb") == 0) { found = 1; break; }
    }
    CHECK(found && strcmp(state, "running") == 0, "scan state is running");
    snprintf(path, sizeof(path), "%s/guests/list", g_ns);
    CHECK(file_has(path, "running"), "guests/list shows running");
    CHECK(wubu_guest_remove("bb") == -1, "remove refused while running");
    CHECK(wait_guest_exit("bb", 10000) == 0, "sleep guest exits");
    PASS();
}

static void test_remove(void) {
    TEST("L8: remove cleans root + registry");
    char root[512], path[768];
    CHECK(wubu_guest_import_root("bb", root, sizeof(root)) == 0, "bb root known");
    CHECK(wubu_guest_remove("bb") == 0, "remove succeeds");
    CHECK(access(root, F_OK) != 0, "root directory deleted");
    snprintf(path, sizeof(path), "%s/guests/list", g_ns);
    CHECK(!file_has(path, "bb\t"), "guests/list no longer shows bb");
    snprintf(path, sizeof(path), "%s/guests/bb", g_ns);
    CHECK(access(path, F_OK) != 0, "guests/bb entry removed");
    CHECK(wubu_guest_import_root("bb", root, sizeof(root)) == -1, "bb unregistered");
    PASS();
}

static void test_bad_archives_refused(void) {
    TEST("L8: bad archives refused honestly, no residue");
    char bad[320], path[768];
    snprintf(bad, sizeof(bad), "%s/bad.tar.gz", g_base);
    CHECK(write_text(bad, "this is definitely not a gzip tarball\x01\x02") == 0,
          "write garbage tar fixture");
    CHECK(wubu_guest_import(bad, "bad1") == -1, "garbage tar.gz refused");
    snprintf(path, sizeof(path), "%s/bad1", g_roots);
    CHECK(access(path, F_OK) != 0, "no partial root left behind");

    snprintf(bad, sizeof(bad), "%s/bad.cpio.gz", g_base);
    CHECK(write_text(bad, "not a cpio either") == 0, "write garbage cpio fixture");
    CHECK(wubu_guest_import(bad, "bad2") == -1, "garbage cpio.gz refused");

    snprintf(bad, sizeof(bad), "%s/bad.txt", g_base);
    CHECK(write_text(bad, "plain text") == 0, "write unknown-format fixture");
    CHECK(wubu_guest_import(bad, "bad3") == -1, "unknown format refused");

    /* A valid tarball with NO shell is not a launchable system. */
    char nosh[300], nosh_tar[320];
    snprintf(nosh, sizeof(nosh), "%s/nosh", g_base);
    CHECK(mkdir(nosh, 0755) == 0, "mkdir shell-less stage");
    snprintf(path, sizeof(path), "%s/only-marker.txt", nosh);
    CHECK(write_text(path, "no shell here\n") == 0, "write marker");
    snprintf(nosh_tar, sizeof(nosh_tar), "%s/nosh.tar.gz", g_base);
    char *tar[] = { "/bin/tar", "czf", nosh_tar, "-C", nosh, ".", NULL };
    CHECK(run_argv(tar) == 0, "build shell-less tar");
    CHECK(wubu_guest_import(nosh_tar, "nosh") == -1, "shell-less root refused");
    snprintf(path, sizeof(path), "%s/nosh", g_roots);
    CHECK(access(path, F_OK) != 0, "shell-less root cleaned up");

    snprintf(path, sizeof(path), "%s/guests/list", g_ns);
    CHECK(!file_has(path, "bad1") && !file_has(path, "bad2") &&
          !file_has(path, "bad3") && !file_has(path, "nosh"),
          "refused imports never published");
    PASS();
}

static void test_bad_names_and_duplicates(void) {
    TEST("L8: invalid names / duplicates / unknown guests refused");
    CHECK(wubu_guest_import(g_tar, "../evil") == -1, "path-traversal name refused");
    CHECK(wubu_guest_import(g_tar, "a/b") == -1, "slash name refused");
    CHECK(wubu_guest_import(g_tar, "") == -1, "empty name refused");
    CHECK(wubu_guest_import(g_tar, NULL) == -1, "NULL name refused");
    CHECK(wubu_guest_import("/nonexistent/rootfs.tar.gz", "nx") == -1,
          "missing image refused");
    CHECK(wubu_guest_import(g_tar, "bb2") == -1, "duplicate import refused");
    CHECK(wubu_guest_launch("never-imported", "/bin/sh") == -1,
          "launch of unknown guest refused");
    CHECK(wubu_guest_remove("never-imported") == -1,
          "remove of unknown guest refused");
    PASS();
}

static void test_cli_dispatch(void) {
    TEST("L8: terminal 'guest' dispatch (import/list/launch/remove)");
    char out[4096], args[700];
    CHECK(wubu_guest_cli("ls -la", out, sizeof(out)) == 0,
          "non-guest line falls through to the pty");
    CHECK(wubu_guest_cli("guestbogus x", out, sizeof(out)) == 0,
          "guest-prefixed other command falls through");
    CHECK(wubu_guest_cli("guest list", out, sizeof(out)) == 1, "list handled");
    CHECK(strstr(out, "bb2") != NULL, "list output shows bb2");

    snprintf(args, sizeof(args), "guest import %s cli1", g_tar);
    CHECK(wubu_guest_cli(args, out, sizeof(out)) == 1, "import handled");
    CHECK(strstr(out, "imported") != NULL, "import reports success");
    CHECK(wubu_guest_cli("guest launch cli1 /bin/sh -c 'echo CLI > /m'",
                         out, sizeof(out)) == 1, "launch handled");
    CHECK(wait_guest_exit("cli1", 10000) == 0, "cli-launched guest exits");
    char root[512], path[768], buf[64];
    CHECK(wubu_guest_import_root("cli1", root, sizeof(root)) == 0, "cli1 root");
    snprintf(path, sizeof(path), "%s/m", root);
    CHECK(read_text(path, buf, sizeof(buf)) > 0 && strcmp(buf, "CLI\n") == 0,
          "cli-launched guest wrote its marker");
    CHECK(wubu_guest_cli("guest remove cli1", out, sizeof(out)) == 1,
          "remove handled");
    CHECK(wubu_guest_import_root("cli1", root, sizeof(root)) == -1,
          "cli1 gone after remove");
    PASS();
}

static void test_path_with_spaces(void) {
    TEST("L8: image path with spaces imports + launches");
    char spaced[320];
    snprintf(spaced, sizeof(spaced), "%s/with space.tar.gz", g_base);
    char *cp[] = { "/bin/cp", g_tar, spaced, NULL };
    CHECK(run_argv(cp) == 0, "copy tarball to a spaced path");
    CHECK(wubu_guest_import(spaced, "spaced") == 0, "spaced-path import succeeds");
    int rc = launch_and_check("spaced",
        "/bin/sh -c 'echo SPACED > /m'", "m", "SPACED\n");
    CHECK(rc == 0, "spaced-path guest executes");
    CHECK(wubu_guest_remove("spaced") == 0, "spaced guest removed");
    PASS();
}

int main(void) {
    printf("== L8 guest import gate (real rootfs, real exec) ==\n");
    if (setup() != 0) {
        fprintf(stderr, "FATAL: fixture setup failed (busybox missing?)\n");
        return 1;
    }

    test_import_tar_gz();
    test_launch_executes_in_guest();
    test_relaunch_after_exit();
    test_9p_launch_write();
    test_9p_import_write();
    test_directory_import();
    test_tar_zst_import();
    test_cpio_gz_import();
    test_running_state_and_remove_refusal();
    test_remove();
    test_bad_archives_refused();
    test_bad_names_and_duplicates();
    test_cli_dispatch();
    test_path_with_spaces();

    /* Cleanup: remove remaining guests through the API, then the fixture. */
    wubu_guest_remove("bb2");
    wubu_guest_remove("dirg");
    wubu_guest_remove("zg");
    wubu_guest_remove("cg");
    rm_rf(g_base);

    printf("\n%d tests: %d passed, %d failed\n", g_tests, g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
