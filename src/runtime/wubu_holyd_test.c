/*
 * wubu_holyd_test.c  --  Test suite for wubu_holyd (TempleOS HolyC DOS Daemon)
 *
 * Tests daemon init, session lifecycle, window management, input routing,
 * 9P namespace, auto-save, and event publishing.
 */

#define _POSIX_C_SOURCE 200809L

#include "wubu_holyd.h"
#include "../jit/jit.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) do { \
    printf("  TEST %-55s", name); g_total++; \
} while(0)
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Daemon Config Tests ------------------------------------------ */

static void test_config_defaults(void) {
    TEST("holyd config defaults");
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, "/tmp/wubu-test-sessions", sizeof(config.sessions_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test-holyd.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test-holyd.log", sizeof(config.log_path) - 1);
    config.max_sessions = 16;
    config.default_width = 800;
    config.default_height = 600;
    config.save_interval_sec = 300;
    config.log_level = 2;
    config.daemonize = false;

    WubuHoly d;
    int ret = wubu_holyd_init(&d, &config);
    CHECK(ret == 0, "holyd_init returns 0");
    PASS();

    TEST("holyd initial session count is 0");
    CHECK(d.session_count == 0, "no sessions initially");
    PASS();

    TEST("holyd version string");
    CHECK(strcmp(wubu_holyd_version(), "0.1.0") == 0, "version is 0.1.0");
    PASS();

    wubu_holyd_shutdown(&d);
}

/* -- Session State/String Tests ----------------------------------- */

static void test_session_state_strings(void) {
    TEST("session state string: active");
    CHECK(strcmp(wubu_holyd_session_state_str(SESSION_STATE_ACTIVE), "active") == 0, "active");
    PASS();

    TEST("session state string: running");
    CHECK(strcmp(wubu_holyd_session_state_str(SESSION_STATE_RUNNING), "running") == 0, "running");
    PASS();

    TEST("session state string: error");
    CHECK(strcmp(wubu_holyd_session_state_str(SESSION_STATE_ERROR), "error") == 0, "error");
    PASS();

    TEST("window type string: term");
    CHECK(strcmp(wubu_holyd_window_type_str(HOLY_WINDOW_TERM), "term") == 0, "term");
    PASS();

    TEST("window type string: editor");
    CHECK(strcmp(wubu_holyd_window_type_str(HOLY_WINDOW_EDITOR), "editor") == 0, "editor");
    PASS();
}

/* -- Cmd String Tests --------------------------------------------- */

static void test_cmd_strings(void) {
    TEST("cmd string: session_create");
    CHECK(strcmp(wubu_holyd_cmd_str(HOLYD_CMD_SESSION_CREATE), "session_create") == 0, "session_create");
    PASS();

    TEST("cmd string: eval");
    CHECK(strcmp(wubu_holyd_cmd_str(HOLYD_CMD_EVAL), "eval") == 0, "eval");
    PASS();

    TEST("cmd string: window_create");
    CHECK(strcmp(wubu_holyd_cmd_str(HOLYD_CMD_WINDOW_CREATE), "window_create") == 0, "window_create");
    PASS();
}

/* -- Session Lifecycle Tests -------------------------------------- */

static void test_session_create(void) {
    TEST("session create");
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, "/tmp/wubu-test-sessions2", sizeof(config.sessions_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test2.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test2.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuHoly d;
    wubu_holyd_init(&d, &config);

    int ret = wubu_holyd_session_create(&d, "test-session", 800, 600);
    CHECK(ret == 0, "session_create returns 0");
    PASS();

    TEST("session count is 1");
    CHECK(d.session_count == 1, "one session");
    PASS();

    TEST("session is active");
    CHECK(d.sessions[0].state == SESSION_STATE_ACTIVE, "state is active");
    PASS();

    TEST("session has 1 window");
    CHECK(d.sessions[0].window_count == 1, "one window");
    PASS();

    TEST("session window is visible");
    CHECK(d.sessions[0].windows[0].visible == true, "window visible");
    PASS();

    TEST("session window has framebuffer");
    CHECK(d.sessions[0].windows[0].framebuffer != NULL, "framebuffer allocated");
    PASS();

    wubu_holyd_shutdown(&d);
}

static void test_session_destroy(void) {
    TEST("session destroy");
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, "/tmp/wubu-test-sessions3", sizeof(config.sessions_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test3.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test3.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuHoly d;
    wubu_holyd_init(&d, &config);
    wubu_holyd_session_create(&d, "destroy-me", 800, 600);

    int ret = wubu_holyd_session_destroy(&d, "destroy-me");
    CHECK(ret == 0, "session_destroy returns 0");
    CHECK(d.session_count == 0, "no sessions after destroy");
    PASS();

    wubu_holyd_shutdown(&d);
}

static void test_session_duplicate(void) {
    TEST("duplicate session name rejected");
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, "/tmp/wubu-test-sessions4", sizeof(config.sessions_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test4.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test4.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuHoly d;
    wubu_holyd_init(&d, &config);
    wubu_holyd_session_create(&d, "dup-test", 800, 600);

    int ret = wubu_holyd_session_create(&d, "dup-test", 800, 600);
    CHECK(ret != 0, "duplicate rejected");
    PASS();

    wubu_holyd_shutdown(&d);
}

/* -- Window Management Tests -------------------------------------- */

static void test_window_create(void) {
    TEST("window create");
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, "/tmp/wubu-test-sessions5", sizeof(config.sessions_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test5.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test5.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuHoly d;
    wubu_holyd_init(&d, &config);
    wubu_holyd_session_create(&d, "win-test", 800, 600);

    int wid = -1;
    int ret = wubu_holyd_window_create(&d, "win-test", HOLY_WINDOW_EDITOR,
                                        100, 100, 640, 480, "Editor", &wid);
    CHECK(ret == 0, "window_create returns 0");
    CHECK(wid >= 0, "window id assigned");
    CHECK(d.sessions[0].window_count == 2, "two windows");
    PASS();

    TEST("window destroy");
    ret = wubu_holyd_window_destroy(&d, "win-test", wid);
    CHECK(ret == 0, "window_destroy returns 0");
    CHECK(d.sessions[0].window_count == 1, "back to one window");
    PASS();

    wubu_holyd_shutdown(&d);
}

/* -- Input Routing Tests ------------------------------------------ */

static void test_input_key(void) {
    TEST("input key routing");
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, "/tmp/wubu-test-sessions6", sizeof(config.sessions_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test6.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test6.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuHoly d;
    wubu_holyd_init(&d, &config);
    wubu_holyd_session_create(&d, "input-test", 800, 600);

    int ret = wubu_holyd_input_key(&d, "input-test", 'A', 0);
    CHECK(ret == 0, "input_key returns 0");
    CHECK(d.sessions[0].input_tail > 0, "input queued");
    PASS();

    wubu_holyd_shutdown(&d);
}

/* -- Auto-Save Tests ---------------------------------------------- */

static void test_session_save(void) {
    TEST("session save");
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, "/tmp/wubu-test-sessions7", sizeof(config.sessions_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test7.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test7.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuHoly d;
    wubu_holyd_init(&d, &config);
    wubu_holyd_session_create(&d, "save-test", 800, 600);

    int ret = wubu_holyd_session_save(&d, "save-test");
    CHECK(ret == 0, "session_save returns 0");
    CHECK(d.sessions[0].last_save > 0, "last_save timestamp set");
    PASS();

    /* Check save file exists */
    char save_path[512];
    snprintf(save_path, sizeof(save_path), "%s/save-test/session.sav", config.sessions_path);
    CHECK(access(save_path, F_OK) == 0, "save file exists");
    PASS();

    wubu_holyd_shutdown(&d);
}

/* -- 9P Namespace Tests ------------------------------------------- */

static void test_mount(void) {
    TEST("session mount");
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, "/tmp/wubu-test-sessions8", sizeof(config.sessions_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test8.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test8.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuHoly d;
    wubu_holyd_init(&d, &config);
    wubu_holyd_session_create(&d, "mount-test", 800, 600);

    int ret = wubu_holyd_mount(&d, "mount-test", "/wubu/holyc/mount-test");
    CHECK(ret == 0, "mount returns 0");
    CHECK(d.sessions[0].mounted == true, "mounted flag set");
    PASS();

    TEST("session unmount");
    ret = wubu_holyd_unmount(&d, "mount-test");
    CHECK(ret == 0, "unmount returns 0");
    CHECK(d.sessions[0].mounted == false, "mounted flag cleared");
    PASS();

    wubu_holyd_shutdown(&d);
}

/* -- Event Publishing Test ---------------------------------------- */

static void test_event_publish(void) {
    TEST("event publish creates event file");
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, "/tmp/wubu-test-sessions9", sizeof(config.sessions_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test9.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test9.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuHoly d;
    wubu_holyd_init(&d, &config);

    mkdir(config.sessions_path, 0755);
    int ret = wubu_holyd_publish_event(&d, "test_event", "test_session", "{\"test\":true}");
    CHECK(ret == 0, "publish returns 0");
    PASS();

    wubu_holyd_shutdown(&d);
    unlink("/tmp/wubu-test-sessions9/events");
    rmdir("/tmp/wubu-test-sessions9");
}

/* -- HolyC JIT Eval Tests ----------------------------------------- */

static void test_holyd_eval(void) {
    TEST("holyd eval: simple integer literal");
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, "/tmp/wubu-test-eval1", sizeof(config.sessions_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test-eval1.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test-eval1.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuHoly d;
    wubu_holyd_init(&d, &config);
    wubu_holyd_session_create(&d, "eval-test", 800, 600);

    char output[1024];
    int ret = wubu_holyd_eval(&d, "eval-test", "42", output, sizeof(output));
    CHECK(ret == 0, "eval returns 0");
    CHECK(strcmp(output, "42") == 0, "eval 42 returns 42");
    PASS();

    TEST("holyd eval: arithmetic expression");
    ret = wubu_holyd_eval(&d, "eval-test", "1+2+3", output, sizeof(output));
    CHECK(ret == 0, "eval 1+2+3 returns 0");
    CHECK(strcmp(output, "6") == 0, "eval 1+2+3 returns 6");
    PASS();

    TEST("holyd eval: session not found");
    ret = wubu_holyd_eval(&d, "no-such-session", "1", output, sizeof(output));
    CHECK(ret != 0, "eval on missing session fails");
    PASS();

    TEST("holyd compile: valid code produces binary");
    void *binary = NULL;
    size_t bin_size = 0;
    ret = wubu_holyd_compile(&d, "eval-test", "40+2", &binary, &bin_size);
    CHECK(ret == 0, "compile returns 0");
    CHECK(binary != NULL, "binary is not NULL");
    CHECK(bin_size > 0, "binary size > 0");
    PASS();

    if (binary) jit_free_exec(binary, bin_size);

    wubu_holyd_shutdown(&d);
}

/* -- HolyC JIT Persistent State Tests ----------------------------- */

static void test_holyd_persistent_state(void) {
    TEST("holyd eval: persistent variable across evals");
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, "/tmp/wubu-test-persist1", sizeof(config.sessions_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test-persist1.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test-persist1.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuHoly d;
    wubu_holyd_init(&d, &config);
    /* Disable file logging, force stderr for debug */
    d.config.log_level = 3; /* Enable debug logging for this test */
    wubu_holyd_session_create(&d, "persist-test", 800, 600);

    char output[1024];
    int ret = wubu_holyd_eval(&d, "persist-test", "I64 x = 123;", output, sizeof(output));
    printf("DEBUG: First eval output: '%s'\n", output);
    CHECK(ret == 0, "eval variable declaration returns 0");

    /* Variable x should persist to next eval */
    ret = wubu_holyd_eval(&d, "persist-test", "x + 10", output, sizeof(output));
    printf("DEBUG: Second eval output: '%s'\n", output);
    CHECK(ret == 0, "eval using persistent variable returns 0");
    CHECK(strcmp(output, "133") == 0, "eval x+10 returns 133");
    PASS();

    TEST("holyd eval: persistent function across evals");
    ret = wubu_holyd_eval(&d, "persist-test", "I64 square(I64 n) { return n * n; }", output, sizeof(output));
    CHECK(ret == 0, "eval function declaration returns 0");

    /* Function should persist to next eval */
    ret = wubu_holyd_eval(&d, "persist-test", "square(7)", output, sizeof(output));
    CHECK(ret == 0, "eval calling persistent function returns 0");
    CHECK(strcmp(output, "49") == 0, "eval square(7) returns 49");
    PASS();

    wubu_holyd_shutdown(&d);
}

/* -- Main --------------------------------------------------------- */

int main(void) {
    printf("\n==================================================\n");
    printf("  WuBuOS TempleOS HolyC DOS Daemon Test Suite\n");
    printf("==================================================\n\n");

    test_config_defaults();
    test_session_state_strings();
    test_cmd_strings();
    test_session_create();
    test_session_destroy();
    test_session_duplicate();
    test_window_create();
    test_input_key();
    test_session_save();
    test_mount();
    test_event_publish();

    /* -- HolyC JIT Eval Tests ------------------------------------- */

    test_holyd_eval();

    /* -- HolyC JIT Persistent State Tests ------------------------ */

    test_holyd_persistent_state();

    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("==================================================\n");

    return g_fail > 0 ? 1 : 0;
}
