/*
 * wubu_host_exec_test.c  --  Cell 203 Behavioral Test Suite
 *
 * Tests: fork+exec container creation, start, wait, kill.
 * All tests exercise REAL host process creation.
 */
#include "../egojar.h"
#include "../egojar_net.h"
#include "wubu_host_exec.h"
#include "styxfs.h"
#include "styx.h"
#include "wubu_ns_guests.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

/* wubu_ns_bridge_create lives in the FS core (wubu_ns_fs.c); declare it
 * directly to avoid pulling the full bridge god-header into this test. */
extern int wubu_ns_bridge_create(const char *);

/* -- Test Framework ------------------------------------------------ */

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

/* -- Tests --------------------------------------------------------- */

static void test_ct_create(void) {
    TEST("Cell203: wubu_ct_create with valid args");
    WubuCt *ct = wubu_ct_create("test", "/var/wubu/roots/arch", CT_NATIVE);
    CHECK(ct != NULL, "create returned non-NULL");
    CHECK(strcmp(ct->name, "test") == 0, "name set");
    CHECK(ct->state == CT_STOPPED, "initial state is stopped");
    CHECK(ct->runtime == CT_NATIVE, "runtime is native");
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_state_names(void) {
    TEST("Cell203: state and runtime name strings");
    CHECK(strcmp(wubu_ct_state_name(CT_STOPPED), "stopped") == 0, "stopped");
    CHECK(strcmp(wubu_ct_state_name(CT_RUNNING), "running") == 0, "running");
    CHECK(strcmp(wubu_ct_runtime_name(CT_STEAMOS), "SteamOS") == 0, "SteamOS");
    CHECK(strcmp(wubu_ct_runtime_name(CT_PROTON), "Proton") == 0, "Proton");
    PASS();
}

static void test_ct_set_cmd(void) {
    TEST("Cell203: set container command");
    WubuCt *ct = wubu_ct_create("test", "/", CT_NATIVE);
    CHECK(ct != NULL, "created");
    char *args[] = {"/bin/echo", "hello", NULL};
    CHECK(wubu_ct_set_cmd(ct, 2, args) == 0, "set_cmd");
    CHECK(ct->argv[0] != NULL, "argv[0] set");
    CHECK(strcmp(ct->argv[0], "/bin/echo") == 0, "argv[0] is /bin/echo");
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_add_env(void) {
    TEST("Cell203: add environment variables");
    WubuCt *ct = wubu_ct_create("test", "/", CT_NATIVE);
    CHECK(wubu_ct_add_env(ct, "FOO=bar") == 0, "add FOO");
    CHECK(wubu_ct_add_env(ct, "BAZ=qux") == 0, "add BAZ");
    CHECK(ct->envp[0] != NULL, "envp[0] set");
    CHECK(ct->envp[1] != NULL, "envp[1] set");
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_binds(void) {
    TEST("Cell203: add 9P bind mounts");
    WubuCt *ct = wubu_ct_create("test", "/", CT_NATIVE);
    CHECK(wubu_ct_add_bind(ct, "/dev/dri", "/dev/dri", false) == 0, "add /dev/dri");
    CHECK(wubu_ct_add_bind(ct, "/tmp/.X11-unix", "/tmp/.X11-unix", true) == 0, "add X11");
    CHECK(ct->n_binds == 2, "2 binds");
    CHECK(ct->binds[1].readonly, "X11 is readonly");
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_gpu_passthrough(void) {
    TEST("Cell203: GPU passthrough configuration");
    WubuCt *ct = wubu_ct_create("test", "/", CT_NATIVE);
    wubu_ct_set_gpu(ct, true);
    CHECK(ct->gpu_passthrough, "gpu enabled");
    wubu_ct_set_gpu(ct, false);
    CHECK(!ct->gpu_passthrough, "gpu disabled");
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_resource_limits(void) {
    TEST("Cell203: resource limits (mem + cpu)");
    WubuCt *ct = wubu_ct_create("test", "/", CT_NATIVE);
    wubu_ct_set_limits(ct, 4096, 4);
    CHECK(ct->mem_limit_mb == 4096, "4GB mem limit");
    CHECK(ct->cpu_limit == 4, "4 CPU limit");
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_fork_exec(void) {
    TEST("Cell203: fork+exec runs /bin/echo and exits");
    WubuCt *ct = wubu_ct_create("echo-test", "/", CT_NATIVE);
    CHECK(ct != NULL, "created");
    
    char *args[] = {"/bin/echo", "wubu-container-works", NULL};
    CHECK(wubu_ct_set_cmd(ct, 2, args) == 0, "set_cmd");
    
    /* Start: fork + chroot + exec */
    CHECK(wubu_ct_start(ct) == 0, "start");
    CHECK(ct->state == CT_RUNNING, "state is running");
    CHECK(ct->pid > 0, "has PID");
    
    /* Wait for exit */
    int exit_code = wubu_ct_wait(ct);
    CHECK(exit_code == 0, "exit code 0");
    CHECK(ct->state == CT_EXITED, "state is exited");
    
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_fork_exec_exitcode(void) {
    TEST("Cell203: fork+exec /bin/false returns exit code 1");
    WubuCt *ct = wubu_ct_create("false-test", "/", CT_NATIVE);
    CHECK(ct != NULL, "created");
    
    char *args[] = {"/bin/false", NULL};
    CHECK(wubu_ct_set_cmd(ct, 1, args) == 0, "set_cmd");
    CHECK(wubu_ct_start(ct) == 0, "start");
    
    int exit_code = wubu_ct_wait(ct);
    CHECK(exit_code == 1, "exit code 1");
    
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_kill(void) {
    TEST("Cell203: kill running container with SIGTERM");
    WubuCt *ct = wubu_ct_create("sleep-test", "/", CT_NATIVE);
    CHECK(ct != NULL, "created");
    
    char *args[] = {"/bin/sleep", "30", NULL};
    CHECK(wubu_ct_set_cmd(ct, 2, args) == 0, "set_cmd");
    CHECK(wubu_ct_start(ct) == 0, "start");
    CHECK(ct->state == CT_RUNNING, "running");
    
    /* Kill it */
    CHECK(wubu_ct_kill(ct, SIGTERM) == 0, "kill SIGTERM");
    
    int exit_code = wubu_ct_wait(ct);
    CHECK(ct->state == CT_EXITED, "exited after kill");
    /* Should be killed by signal: negative exit code */
    CHECK(exit_code != 0, "non-zero exit (killed)");
    
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_state_poll(void) {
    TEST("Cell203: wubu_ct_state polls running process");
    WubuCt *ct = wubu_ct_create("poll-test", "/", CT_NATIVE);
    char *args[] = {"/bin/sleep", "2", NULL};
    wubu_ct_set_cmd(ct, 2, args);
    CHECK(wubu_ct_start(ct) == 0, "start");
    
    /* Should still be running (hasn't exited yet) */
    CtState s = wubu_ct_state(ct);
    CHECK(s == CT_RUNNING, "still running");
    
    /* Wait a bit then poll again  --  it should exit */
    usleep(2500000);  /* 2.5s */
    s = wubu_ct_state(ct);
    CHECK(s == CT_EXITED, "exited after sleep");
    
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_steamos_preset(void) {
    TEST("Cell203: SteamOS container preset (GPU + env)");
    WubuCt *ct = wubu_ct_steamos("steam", "/var/wubu/roots/arch-steam");
    CHECK(ct != NULL, "created");
    CHECK(ct->runtime == CT_STEAMOS, "runtime is SteamOS");
    CHECK(ct->gpu_passthrough, "GPU passthrough enabled");
    CHECK(ct->envp[0] != NULL, "STEAM_RUNTIME env set");
    
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_native_preset(void) {
    TEST("Cell203: Native Linux container preset");
    WubuCt *ct = wubu_ct_native("native-ct", "/var/wubu/roots/arch");
    CHECK(ct != NULL, "created");
    CHECK(ct->runtime == CT_NATIVE, "runtime is native");
    CHECK(ct->net_enabled, "network enabled");
    
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_no_cmd_fails(void) {
    TEST("Cell203: start without command returns -1");
    WubuCt *ct = wubu_ct_create("no-cmd", "/", CT_NATIVE);
    CHECK(wubu_ct_start(ct) == -1, "start fails");
    CHECK(ct->state != CT_RUNNING, "not running");
    wubu_ct_destroy(ct);
    PASS();
}

static void test_ct_double_start_fails(void) {
    TEST("Cell203: double start returns -1");
    WubuCt *ct = wubu_ct_create("dbl", "/", CT_NATIVE);
    char *args[] = {"/bin/true", NULL};
    wubu_ct_set_cmd(ct, 1, args);
    CHECK(wubu_ct_start(ct) == 0, "first start ok");
    CHECK(wubu_ct_start(ct) == -1, "second start fails");
    wubu_ct_wait(ct);
    wubu_ct_destroy(ct);
    PASS();
}

/* -- L9: per-container 9P server + /n/guests control plane ---------- */

static uint8_t l9_tin[STYX_MAX_MSG], l9_tout[STYX_MAX_MSG];
static styxfs_server_t l9_ns_srv;

static int l9_write_all(int fd, const uint8_t *b, uint32_t n) {
    uint32_t done = 0;
    while (done < n) {
        ssize_t w = write(fd, b + done, n - done);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        done += (uint32_t)w;
    }
    return 0;
}

static int l9_read_all(int fd, uint8_t *b, uint32_t n) {
    uint32_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, b + got, n - got);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) return -1;
        got += (uint32_t)r;
    }
    return 0;
}

static int l9_connect(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un a;
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    strncpy(a.sun_path, path, sizeof(a.sun_path) - 1);
    for (int i = 0; i < 100; i++) {
        if (connect(fd, (struct sockaddr*)&a, sizeof(a)) == 0) return fd;
        usleep(10000);
    }
    close(fd);
    return -1;
}

/* Send a framed T-message (the styx_build_* helpers already frame it) and
 * receive one framed R-message into l9_tout. 0 on success. */
static int l9_rpc(int fd, uint32_t il, uint32_t *ol) {
    if (l9_write_all(fd, l9_tin, il) != 0) return -1;
    if (l9_read_all(fd, l9_tout, 4) != 0) return -1;
    uint32_t sz = styx_get32(l9_tout);
    if (sz < 7 || sz > STYX_MAX_MSG) return -1;
    if (l9_read_all(fd, l9_tout + 4, sz - 4) != 0) return -1;
    *ol = sz;
    return 0;
}

static void l9_rm_rf(const char *path) {
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    if (system(cmd) != 0) { /* best-effort cleanup */ }
}

static void test_ct_styx_9p_read(void) {
    TEST("L9: 9P walk+read container root over Styx socket");
    char root[] = "/tmp/wubu_l9_root_XXXXXX";
    CHECK(mkdtemp(root) != NULL, "mkdtemp container root");
    char marker[300];
    snprintf(marker, sizeof(marker), "%s/hello.txt", root);
    const char *content = "L9-9P-REAL-BYTES-0123456789\n";
    FILE *f = fopen(marker, "w");
    CHECK(f != NULL, "write marker file into container root");
    fputs(content, f); fclose(f);

    /* Preferred: a REAL chrooted container. The child unshares a user
     * namespace (gaining CAP_SYS_CHROOT), so an empty tmp root would trap
     * it with no binaries -- seed a static busybox as the rootfs payload.
     * Fallback (no busybox on the box): host-namespace container (root "/"),
     * marker reached through its 9P export of the host root. */
    const char *fileseg[3]; int nfileseg;
    const char *dirseg[2];  int ndirseg;
    char rootbase[64];
    snprintf(rootbase, sizeof(rootbase), "%s", root + 5);  /* after "/tmp/" */
    WubuCt *ct;
    char *args[4];
    int argc;
    if (access("/usr/bin/busybox", X_OK) == 0) {
        char bin[300], cp[600];
        snprintf(bin, sizeof(bin), "%s/bin", root);
        CHECK(mkdir(bin, 0755) == 0, "mkdir rootfs /bin");
        snprintf(cp, sizeof(cp), "cp /usr/bin/busybox '%s/busybox'", bin);
        CHECK(system(cp) == 0, "seed static busybox into rootfs");
        ct = wubu_ct_create("l9-styx", root, CT_NATIVE);
        args[0] = "/bin/busybox"; args[1] = "sleep"; args[2] = "1"; args[3] = NULL;
        argc = 3;
        fileseg[0] = "hello.txt"; nfileseg = 1;
        ndirseg = 0;   /* directory read = clone of the root fid */
    } else {
        ct = wubu_ct_create("l9-styx", "/", CT_NATIVE);
        args[0] = "/bin/sleep"; args[1] = "1"; args[2] = NULL;
        argc = 2;
        fileseg[0] = "tmp"; fileseg[1] = rootbase; fileseg[2] = "hello.txt";
        nfileseg = 3;
        dirseg[0] = "tmp"; dirseg[1] = rootbase; ndirseg = 2;
    }
    CHECK(ct != NULL, "created");
    CHECK(wubu_ct_set_cmd(ct, argc, args) == 0, "set_cmd");
    CHECK(wubu_ct_start(ct) == 0, "start");
    CHECK(ct->styx_fd >= 0, "styx listening socket up");
    CHECK(ct->styx_pid > 0, "parent-side 9P server process forked");
    int spid = ct->styx_pid;

    int fd = l9_connect(ct->styx_path);
    CHECK(fd >= 0, "connect to per-container Styx socket");
    uint32_t il, ol;

    styx_build_tversion(l9_tin, &il, 8192, "9P2000");
    CHECK(l9_rpc(fd, il, &ol) == 0 && l9_tout[4] == STX_RVERSION,
          "Tversion -> Rversion (real wire handshake)");
    styx_build_tattach(l9_tin, &il, 1, 1, 0, "/");
    CHECK(l9_rpc(fd, il, &ol) == 0 && l9_tout[4] == STX_RATTACH,
          "Tattach -> Rattach");
    CHECK(l9_tout[7] == STX_QTDIR, "attach qid is the root directory");

    styx_build_twalk(l9_tin, &il, 2, 1, 2, fileseg, nfileseg);
    CHECK(l9_rpc(fd, il, &ol) == 0 && l9_tout[4] == STX_RWALK,
          "Twalk -> Rwalk into container root");
    CHECK(styx_get16(l9_tout + 7) == (uint16_t)nfileseg,
          "walk returned one qid per component");
    CHECK(l9_tout[9 + (nfileseg - 1) * 13] == STX_QTFILE,
          "marker qid is a file");

    styx_build_topen(l9_tin, &il, 3, 2, STX_OREAD);
    CHECK(l9_rpc(fd, il, &ol) == 0 && l9_tout[4] == STX_ROPEN,
          "Topen -> Ropen (read)");
    styx_build_tread(l9_tin, &il, 4, 2, 0, 4096);
    CHECK(l9_rpc(fd, il, &ol) == 0 && l9_tout[4] == STX_RREAD,
          "Tread -> Rread");
    uint32_t n = styx_get32(l9_tout + 7);
    CHECK(n == strlen(content), "read returned the exact marker length");
    CHECK(memcmp(l9_tout + 11, content, n) == 0,
          "REAL marker bytes come back over 9P");

    styx_build_tread(l9_tin, &il, 5, 2, n, 4096);
    CHECK(l9_rpc(fd, il, &ol) == 0 && styx_get32(l9_tout + 7) == 0,
          "read at EOF returns 0 bytes");

    /* Directory read of the marker's parent must list the marker. */
    styx_build_twalk(l9_tin, &il, 6, 1, 3, dirseg, ndirseg);
    CHECK(l9_rpc(fd, il, &ol) == 0 && l9_tout[4] == STX_RWALK,
          "walk to marker's parent directory");
    CHECK(styx_get16(l9_tout + 7) == (uint16_t)(ndirseg ? ndirseg : 1),
          "parent directory walk qids");
    styx_build_topen(l9_tin, &il, 7, 3, STX_OREAD);
    CHECK(l9_rpc(fd, il, &ol) == 0 && l9_tout[4] == STX_ROPEN,
          "open root directory");
    styx_build_tread(l9_tin, &il, 8, 3, 0, 4096);
    CHECK(l9_rpc(fd, il, &ol) == 0 && l9_tout[4] == STX_RREAD,
          "read root directory entries");
    n = styx_get32(l9_tout + 7);
    int found = 0;
    uint32_t pos = 11;
    while (pos + 2 <= 11 + n) {
        uint16_t rs = styx_get16(l9_tout + pos);
        if (rs < 41 || pos + 2 + rs > 11 + n) break;
        /* record: size[2] type[2] dev[4] qid[13] mode[4] atime[4] mtime[4]
         * length[8] -> name length at record offset 41, name bytes at 43 */
        uint16_t nl = styx_get16(l9_tout + pos + 41);
        if (nl == 9 && memcmp(l9_tout + pos + 43, "hello.txt", 9) == 0)
            found = 1;
        pos += 2 + rs;
    }
    CHECK(found, "root directory read lists hello.txt");

    styx_build_tclunk(l9_tin, &il, 9, 2);  l9_rpc(fd, il, &ol);
    styx_build_tclunk(l9_tin, &il, 10, 3); l9_rpc(fd, il, &ol);
    styx_build_tclunk(l9_tin, &il, 11, 1); l9_rpc(fd, il, &ol);
    close(fd);

    CHECK(wubu_ct_wait(ct) == 0, "container exits 0");
    CHECK(ct->state == CT_EXITED, "state exited");
    /* kill(pid,0) SUCCEEDS on a zombie -- ESRCH proves it was reaped. */
    CHECK(kill(spid, 0) == -1 && errno == ESRCH,
          "9P server reaped on container exit (no zombie)");
    CHECK(ct->styx_fd == -1, "listening socket closed on exit");
    CHECK(access(ct->styx_path, F_OK) == -1, "socket path unlinked on exit");

    wubu_ct_destroy(ct);
    l9_rm_rf(root);
    PASS();
}

static void test_ct_styx_destroy_reaps(void) {
    TEST("L9: destroy reaps the 9P server (no wait called)");
    WubuCt *ct = wubu_ct_create("l9-destroy", "/", CT_NATIVE);
    CHECK(ct != NULL, "created");
    char *args[] = {"/bin/sleep", "30", NULL};
    CHECK(wubu_ct_set_cmd(ct, 2, args) == 0, "set_cmd");
    CHECK(wubu_ct_start(ct) == 0, "start");
    CHECK(ct->styx_pid > 0, "server running");
    int spid = ct->styx_pid;
    char path[256];
    strncpy(path, ct->styx_path, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    wubu_ct_destroy(ct);   /* must kill container AND stop/reap server */
    CHECK(kill(spid, 0) == -1 && errno == ESRCH,
          "9P server reaped on destroy (no zombie)");
    CHECK(access(path, F_OK) == -1, "socket path unlinked on destroy");
    PASS();
}

/* In-process 9P harness against the ns root (same shape as test_ns_9p):
 * proves /n/guests is a genuine 9P filesystem, not just host files. */
static int l9_ns_serve(uint32_t il, uint32_t *ol) {
    return styxfs_serve(&l9_ns_srv, l9_tin, il, l9_tout, ol);
}

static int l9_ns_read_path(uint32_t fid, uint32_t newfid,
                           const char **seg, int nseg,
                           char *buf, uint32_t cap) {
    uint32_t il, ol;
    styx_build_twalk(l9_tin, &il, 2, fid, newfid, seg, nseg);
    if (l9_ns_serve(il, &ol) != 0 || l9_tout[4] != STX_RWALK) return -1;
    if (styx_get16(l9_tout + 7) != (uint16_t)nseg) return -1;
    styx_build_topen(l9_tin, &il, 3, newfid, STX_OREAD);
    if (l9_ns_serve(il, &ol) != 0 || l9_tout[4] != STX_ROPEN) return -1;
    styx_build_tread(l9_tin, &il, 4, newfid, 0, cap - 1);
    if (l9_ns_serve(il, &ol) != 0 || l9_tout[4] != STX_RREAD) return -1;
    uint32_t n = styx_get32(l9_tout + 7);
    if (n >= cap) n = cap - 1;
    memcpy(buf, l9_tout + 11, n);
    buf[n] = '\0';
    styx_build_tclunk(l9_tin, &il, 5, newfid);
    l9_ns_serve(il, &ol);
    return (int)n;
}

static void test_ns_guests_list(void) {
    TEST("L9: /n/guests list live over 9P, gone after exit");
    char nsroot[] = "/tmp/wubu_l9_ns_XXXXXX";
    CHECK(mkdtemp(nsroot) != NULL, "mkdtemp ns root");
    CHECK(wubu_ns_bridge_create(nsroot) == 0, "bridge create");

    styxfs_init(&l9_ns_srv);
    CHECK(styxfs_mount(&l9_ns_srv, "/", nsroot, 0) == 0, "mount ns root at /");
    uint32_t il, ol;
    styx_build_tversion(l9_tin, &il, 8192, "9P2000");
    CHECK(l9_ns_serve(il, &ol) == 0 && l9_tout[4] == STX_RVERSION,
          "ns 9P version");
    styx_build_tattach(l9_tin, &il, 1, 1, 0, "/");
    CHECK(l9_ns_serve(il, &ol) == 0 && l9_tout[4] == STX_RATTACH,
          "ns 9P attach");

    WubuCt *ct = wubu_ct_create("guest-a", "/", CT_NATIVE);
    CHECK(ct != NULL, "created guest-a");
    char *args[] = {"/bin/sleep", "2", NULL};
    CHECK(wubu_ct_set_cmd(ct, 2, args) == 0, "set_cmd");
    CHECK(wubu_ct_start(ct) == 0, "start guest-a");
    /* start auto-published through the lifecycle sync -- no manual call */

    char buf[4096];
    const char *segl[] = {"guests", "list"};
    int n = l9_ns_read_path(1, 2, segl, 2, buf, sizeof(buf));
    CHECK(n > 0, "9P read guests/list");
    CHECK(strstr(buf, "guest-a") != NULL, "list shows guest-a while running");
    CHECK(strstr(buf, "running") != NULL, "list shows running state");
    CHECK(strstr(buf, "native") != NULL, "list shows personality");
    char pids[32];
    snprintf(pids, sizeof(pids), "%d", ct->pid);
    CHECK(strstr(buf, pids) != NULL, "list shows the live pid");

    const char *segs[] = {"guests", "guest-a", "state"};
    n = l9_ns_read_path(1, 3, segs, 3, buf, sizeof(buf));
    CHECK(n > 0, "9P read guests/guest-a/state");
    CHECK(strstr(buf, "state: running") != NULL, "state file says running");
    char pidline[64];
    snprintf(pidline, sizeof(pidline), "pid: %d", ct->pid);
    CHECK(strstr(buf, pidline) != NULL, "state file carries the pid");

    CHECK(wubu_ct_kill(ct, SIGTERM) == 0, "kill guest-a");
    CHECK(wubu_ct_wait(ct) != 0, "guest-a exited by signal");
    /* wait retired the guest and refreshed the tree */

    n = l9_ns_read_path(1, 4, segl, 2, buf, sizeof(buf));
    CHECK(n >= 0, "9P read guests/list after exit");
    CHECK(strstr(buf, "guest-a") == NULL,
          "list no longer shows the exited guest");
    char gdir[600];
    snprintf(gdir, sizeof(gdir), "%s/guests/guest-a", nsroot);
    CHECK(access(gdir, F_OK) == -1, "guest dir removed after exit");
    CHECK(wubu_ct_registry_find("guest-a") == NULL,
          "guest unregistered after exit");

    styx_build_tclunk(l9_tin, &il, 6, 1);
    l9_ns_serve(il, &ol);
    styxfs_unmount(&l9_ns_srv, "/");
    wubu_ct_destroy(ct);
    l9_rm_rf(nsroot);
    PASS();
}

static void test_ns_guests_launch_by_write(void) {
    TEST("L9: write-to-launch via /n/guests/<name>/launch");
    char nsroot[] = "/tmp/wubu_l9_launch_XXXXXX";
    CHECK(mkdtemp(nsroot) != NULL, "mkdtemp ns root");
    CHECK(wubu_ns_bridge_create(nsroot) == 0, "bridge create");

    styxfs_init(&l9_ns_srv);
    CHECK(styxfs_mount(&l9_ns_srv, "/", nsroot, 0) == 0, "mount ns root");
    uint32_t il, ol;
    styx_build_tversion(l9_tin, &il, 8192, "9P2000");
    CHECK(l9_ns_serve(il, &ol) == 0 && l9_tout[4] == STX_RVERSION, "version");
    styx_build_tattach(l9_tin, &il, 1, 1, 0, "/");
    CHECK(l9_ns_serve(il, &ol) == 0 && l9_tout[4] == STX_RATTACH, "attach");

    char marker[300];
    snprintf(marker, sizeof(marker), "%s/launched.marker", nsroot);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "/bin/sh -c 'echo L9-LAUNCHED > %s'", marker);

    /* Walk to a guest that is NOT running yet -- the probe seam must let
     * the client reach its launch file. */
    const char *seg[] = {"guests", "launch-test", "launch"};
    styx_build_twalk(l9_tin, &il, 2, 1, 2, seg, 3);
    CHECK(l9_ns_serve(il, &ol) == 0 && l9_tout[4] == STX_RWALK,
          "walk to unregistered guest's launch file");
    CHECK(styx_get16(l9_tout + 7) == 3, "walk reached all 3 components");
    styx_build_topen(l9_tin, &il, 3, 2, STX_OWRITE);
    CHECK(l9_ns_serve(il, &ol) == 0 && l9_tout[4] == STX_ROPEN,
          "open launch for write");
    styx_build_twrite(l9_tin, &il, 4, 2, 0, (uint32_t)strlen(cmd),
                      (const uint8_t *)cmd);
    CHECK(l9_ns_serve(il, &ol) == 0, "write dispatched");
    CHECK(l9_tout[4] == STX_RWRITE, "Rwrite -- launch accepted (not Rerror)");
    CHECK(styx_get32(l9_tout + 7) == strlen(cmd), "full command accepted");

    WubuCt *lc = wubu_ct_registry_find("launch-test");
    CHECK(lc != NULL, "launch-by-write registered the guest");
    CHECK(wubu_ct_state(lc) == CT_RUNNING, "launched guest is running");

    int seen = 0;
    for (int i = 0; i < 200; i++) {
        FILE *f = fopen(marker, "r");
        if (f) { fclose(f); seen = 1; break; }
        usleep(10000);
    }
    CHECK(seen, "launched command really executed (marker file)");
    char mb[64] = {0};
    FILE *f = fopen(marker, "r");
    if (f) { size_t r = fread(mb, 1, sizeof(mb) - 1, f); mb[r] = '\0'; fclose(f); }
    CHECK(strcmp(mb, "L9-LAUNCHED\n") == 0,
          "marker content proves real exec output");

    char buf[4096];
    const char *segl[] = {"guests", "list"};
    int n = l9_ns_read_path(1, 3, segl, 2, buf, sizeof(buf));
    CHECK(n > 0 && strstr(buf, "launch-test") != NULL,
          "guests/list shows the launched guest");

    CHECK(wubu_ct_wait(lc) == 0, "launched guest exits 0");
    /* wait retired it; the guests module reaped (destroyed) its own ct */
    CHECK(wubu_ct_registry_find("launch-test") == NULL,
          "launched guest unregistered after exit");
    n = l9_ns_read_path(1, 4, segl, 2, buf, sizeof(buf));
    CHECK(n >= 0 && strstr(buf, "launch-test") == NULL,
          "list no longer shows the exited launched guest");

    /* The name is free again: relaunch through the same write path. */
    CHECK(wubu_ns_guests_launch("launch-test", "/bin/sleep 1") == 0,
          "relaunch same name after exit (owned slot reaped)");
    WubuCt *lc2 = wubu_ct_registry_find("launch-test");
    CHECK(lc2 != NULL, "relaunched guest registered");
    CHECK(wubu_ct_wait(lc2) == 0, "relaunched guest exits 0");
    CHECK(wubu_ct_registry_count() == 0, "registry empty at end");

    styx_build_tclunk(l9_tin, &il, 5, 2); l9_ns_serve(il, &ol);
    styx_build_tclunk(l9_tin, &il, 6, 1); l9_ns_serve(il, &ol);
    styxfs_unmount(&l9_ns_srv, "/");
    l9_rm_rf(nsroot);
    PASS();
}

static void test_ns_guests_refusals(void) {
    TEST("L9: launch refuses bad names, empty cmds, duplicates");
    CHECK(wubu_ns_guests_launch("..", "/bin/true") == -1,
          "refuses traversal name (.. component)");
    CHECK(wubu_ns_guests_launch("a/b", "/bin/true") == -1,
          "refuses name with a slash");
    CHECK(wubu_ns_guests_launch("ok-name", "") == -1,
          "refuses empty command");
    CHECK(wubu_ns_guests_launch(NULL, "/bin/true") == -1,
          "refuses NULL name");
    uint32_t nw = 0;
    CHECK(wubu_ns_guests_write("guests/empty-test/launch",
                               (const uint8_t *)"", 0, &nw) == -1,
          "empty control write refused");
    CHECK(wubu_ns_guests_write("guests/../launch",
                               (const uint8_t *)"/bin/true", 9, &nw) == -1,
          "traversal control write refused");
    uint8_t qt = 0;
    CHECK(wubu_ns_guests_probe("guests/../evil/launch", &qt) == 0,
          "probe refuses .. path");
    CHECK(wubu_ns_guests_probe("/n/guests/good-name/launch", &qt) == 1 &&
          qt == STX_QTFILE, "probe accepts /n-mounted launch path");

    WubuCt *d1 = wubu_ct_create("dup-guest", "/", CT_NATIVE);
    CHECK(d1 != NULL, "created dup-guest");
    char *a[] = {"/bin/sleep", "2", NULL};
    CHECK(wubu_ct_set_cmd(d1, 2, a) == 0, "set_cmd");
    CHECK(wubu_ct_start(d1) == 0, "start dup-guest");
    CHECK(wubu_ns_guests_launch("dup-guest", "/bin/true") == -1,
          "refuses duplicate running name");
    WubuCt *d2 = wubu_ct_create("dup-guest", "/", CT_NATIVE);
    CHECK(d2 != NULL, "created second ct with same name");
    CHECK(wubu_ct_set_cmd(d2, 1, a) == 0, "set_cmd");
    CHECK(wubu_ct_start(d2) == -1,
          "ct_start itself refuses a duplicate running name");
    wubu_ct_destroy(d2);
    CHECK(wubu_ct_kill(d1, SIGTERM) == 0, "kill dup-guest");
    wubu_ct_wait(d1);
    wubu_ct_destroy(d1);
    PASS();
}

/* -- Main --------------------------------------------------------- */

int main(void) {
    printf("\n+==================================================+\n");
    printf("|  WuBuOS Host Container Execution Test Suite      |\n");
    printf("|  Cell 203: Fork+exec for .wubu containers        |\n");
    printf("+==================================================+\n\n");
    
    /* Config tests */
    test_ct_create();
    test_ct_state_names();
    test_ct_set_cmd();
    test_ct_add_env();
    test_ct_binds();
    test_ct_gpu_passthrough();
    test_ct_resource_limits();
    
    /* Behavioral tests  --  REAL fork+exec */
    printf("\n-- Behavioral (real process creation) --\n\n");
    test_ct_fork_exec();
    test_ct_fork_exec_exitcode();
    test_ct_kill();
    test_ct_state_poll();
    test_ct_no_cmd_fails();
    test_ct_double_start_fails();

    /* L9 gates -- per-container 9P server + /n/guests control plane */
    printf("\n-- L9: container 9P namespace + /n/guests --\n\n");
    test_ct_styx_9p_read();
    test_ct_styx_destroy_reaps();
    test_ns_guests_list();
    test_ns_guests_launch_by_write();
    test_ns_guests_refusals();
    
    /* Preset tests */
    printf("\n-- Presets --\n\n");
    test_ct_steamos_preset();
    test_ct_native_preset();
    
    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_passed, g_tests, g_failed);
    printf("==================================================\n");
    
    return g_failed > 0 ? 1 : 0;
}
