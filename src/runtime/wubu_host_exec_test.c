/*
 * wubu_host_exec_test.c  --  Cell 203 Behavioral Test Suite
 *
 * Tests: fork+exec container creation, start, wait, kill.
 * All tests exercise REAL host process creation.
 */
#include "wubu_host_exec.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

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
    
    /* Preset tests */
    printf("\n-- Presets --\n\n");
    test_ct_steamos_preset();
    test_ct_native_preset();
    
    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_passed, g_tests, g_failed);
    printf("==================================================\n");
    
    return g_failed > 0 ? 1 : 0;
}
