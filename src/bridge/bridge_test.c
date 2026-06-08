/*
 * bridge_test.c — WuBuOS Bridge/DOS Flip Test Suite
 *
 * Cell 103: Tests DOS flip bridge wiring (Ctrl+Alt+T),
 * mode switching, clipboard, and IPC.
 */

#include "bridge.h"
#include <stdio.h>
#include <string.h>

/* ── Test Framework ──────────────────────────────────────────── */

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) printf("  TEST %-50s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ── Mode Switch Tests ──────────────────────────────────────── */

static void test_init_default_gui(void) {
    TEST("bridge_init defaults to GUI mode");
    bridge_init();
    CHECK(bridge_get_mode() == MODE_GUI, "should start in GUI mode");
    bridge_shutdown();
    PASS();
}

static void test_enter_temple(void) {
    TEST("bridge_enter_temple switches to TEMPLE mode");
    bridge_init();
    bridge_enter_temple();
    CHECK(bridge_get_mode() == MODE_TEMPLE, "should be in TEMPLE mode");
    bridge_shutdown();
    PASS();
}

static void test_exit_temple(void) {
    TEST("bridge_exit_temple returns to GUI mode");
    bridge_init();
    bridge_enter_temple();
    bridge_exit_temple();
    CHECK(bridge_get_mode() == MODE_GUI, "should be back in GUI mode");
    bridge_shutdown();
    PASS();
}

static void test_toggle_mode(void) {
    TEST("bridge_toggle_mode: GUI->TEMPLE->GUI cycle");
    bridge_init();
    CHECK(bridge_get_mode() == MODE_GUI, "start GUI");
    bridge_toggle_mode();  /* Ctrl+Alt+T: GUI -> TEMPLE */
    CHECK(bridge_get_mode() == MODE_TEMPLE, "toggle to TEMPLE");
    bridge_toggle_mode();  /* Ctrl+Alt+T: TEMPLE -> GUI */
    CHECK(bridge_get_mode() == MODE_GUI, "toggle back to GUI");
    bridge_shutdown();
    PASS();
}

static void test_toggle_triple(void) {
    TEST("triple toggle: GUI->TEMPLE->GUI->TEMPLE");
    bridge_init();
    bridge_toggle_mode();
    bridge_toggle_mode();
    bridge_toggle_mode();
    CHECK(bridge_get_mode() == MODE_TEMPLE, "odd toggles = TEMPLE");
    bridge_shutdown();
    PASS();
}

/* ── Clipboard Tests ────────────────────────────────────────── */

static void test_clipboard_set_get(void) {
    TEST("clipboard set and get round-trip");
    bridge_init();
    const char *data = "Hello WuBuOS";
    bridge_clipboard_set(data, strlen(data));
    char buf[256];
    int len = bridge_clipboard_get(buf, sizeof(buf));
    CHECK(len == (int)strlen(data), "length should match");
    CHECK(memcmp(buf, data, len) == 0, "content should match");
    bridge_shutdown();
    PASS();
}

static void test_clipboard_overflow(void) {
    TEST("clipboard truncates on overflow");
    bridge_init();
    char big[CLIPBOARD_MAX + 100];
    memset(big, 'X', sizeof(big));
    big[0] = 'O'; big[1] = 'K';
    bridge_clipboard_set(big, sizeof(big));
    char buf[CLIPBOARD_MAX + 100];
    int len = bridge_clipboard_get(buf, sizeof(buf));
    CHECK(len < CLIPBOARD_MAX, "should be truncated");
    CHECK(buf[0] == 'O' && buf[1] == 'K', "start should be preserved");
    bridge_shutdown();
    PASS();
}

static void test_clipboard_empty(void) {
    TEST("clipboard get returns 0 when empty");
    bridge_init();
    char buf[64];
    int len = bridge_clipboard_get(buf, sizeof(buf));
    CHECK(len == 0, "should be 0 when no data set");
    bridge_shutdown();
    PASS();
}

/* ── IPC Tests ──────────────────────────────────────────────── */

static void test_ipc_send_poll(void) {
    TEST("IPC send and poll round-trip");
    bridge_init();
    BridgeMessage msg = {1, 2, 100, "hello"};
    bridge_send_msg(&msg);
    BridgeMessage out;
    int got = bridge_poll_msg(2, &out);
    CHECK(got == 1, "should receive message");
    CHECK(out.from_pid == 1, "from_pid should match");
    CHECK(out.to_pid == 2, "to_pid should match");
    CHECK(out.type == 100, "type should match");
    CHECK(strcmp(out.payload, "hello") == 0, "payload should match");
    bridge_shutdown();
    PASS();
}

static void test_ipc_broadcast(void) {
    TEST("IPC broadcast (to_pid=0) received by any pid");
    bridge_init();
    BridgeMessage msg = {1, 0, 200, "broadcast"};
    bridge_send_msg(&msg);
    BridgeMessage out;
    int got = bridge_poll_msg(42, &out);
    CHECK(got == 1, "any pid should receive broadcast");
    CHECK(out.type == 200, "type should match");
    bridge_shutdown();
    PASS();
}

static void test_ipc_no_match(void) {
    TEST("IPC poll with wrong pid returns 0");
    bridge_init();
    BridgeMessage msg = {1, 99, 300, "private"};
    bridge_send_msg(&msg);
    BridgeMessage out;
    int got = bridge_poll_msg(42, &out);
    CHECK(got == 0, "wrong pid should not receive msg");
    bridge_shutdown();
    PASS();
}

static void test_ipc_empty_queue(void) {
    TEST("IPC poll on empty queue returns 0");
    bridge_init();
    BridgeMessage out;
    int got = bridge_poll_msg(1, &out);
    CHECK(got == 0, "empty queue should return 0");
    bridge_shutdown();
    PASS();
}

/* ── DOS Flip Hotkey Wiring Test ────────────────────────────── */

static void test_ctrl_alt_t_wiring(void) {
    TEST("Ctrl+Alt+T triggers bridge_toggle_mode via hotkey sim");
    bridge_init();
    CHECK(bridge_get_mode() == MODE_GUI, "start GUI");
    /* Simulate the Ctrl+Alt+T hotkey action */
    bridge_toggle_mode();
    CHECK(bridge_get_mode() == MODE_TEMPLE, "Ctrl+Alt+T -> TEMPLE");
    bridge_toggle_mode();
    CHECK(bridge_get_mode() == MODE_GUI, "Ctrl+Alt+T -> GUI");
    bridge_shutdown();
    PASS();
}

/* ── Main ───────────────────────────────────────────────────── */

int main(void) {
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS Bridge/DOS Flip Test Suite (Cell 103)          ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");

    test_init_default_gui();
    test_enter_temple();
    test_exit_temple();
    test_toggle_mode();
    test_toggle_triple();
    test_clipboard_set_get();
    test_clipboard_overflow();
    test_clipboard_empty();
    test_ipc_send_poll();
    test_ipc_broadcast();
    test_ipc_no_match();
    test_ipc_empty_queue();
    test_ctrl_alt_t_wiring();

    printf("\n════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("════════════════════════════════════════════════════════\n");

    return g_fail > 0 ? 1 : 0;
}
