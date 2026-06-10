/*
 * input_test.c — Kernel Input Subsystem Test Suite
 *
 * Cell 202: Tests for input queue (keyboard/mouse circular buffers),
 * push/poll/wait functionality, modifier tracking.
 */

#include "input.h"
#include <stdio.h>
#include <string.h>

/* ── Test Framework ──────────────────────────────────────────────── */

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) printf("  TEST %-50s", name); g_total++
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ── Initialization Tests ────────────────────────────────────────── */

static void test_input_init(void) {
    TEST("input_init returns 0");
    int rc = input_init();
    CHECK(rc == 0, "init should succeed");
    input_shutdown();
    PASS();
}

static void test_input_init_shutdown(void) {
    TEST("input_init -> input_shutdown no crash");
    input_init();
    input_shutdown();
    PASS();
}

/* ── Keyboard Queue Tests ────────────────────────────────────────── */

static void test_key_push_poll(void) {
    TEST("input_key_push + input_key_poll roundtrip");
    input_init();
    
    KeyEvent ev1 = { .scancode = 0x1C, .keycode = 0x1C, .kind = KEY_EVENT_DOWN, .modifiers = 0 };
    KeyEvent ev2 = { .scancode = 0x1C, .keycode = 0x1C, .kind = KEY_EVENT_UP, .modifiers = 0 };
    
    input_key_push(ev1);
    input_key_push(ev2);
    
    KeyEvent out;
    CHECK(input_key_poll(&out) == 1, "should poll first event");
    CHECK(out.scancode == 0x1C, "scancode matches");
    CHECK(out.kind == KEY_EVENT_DOWN, "kind matches");
    
    CHECK(input_key_poll(&out) == 1, "should poll second event");
    CHECK(out.kind == KEY_EVENT_UP, "kind matches");
    
    CHECK(input_key_poll(&out) == 0, "queue should be empty");
    
    input_shutdown();
    PASS();
}

static void test_key_modifiers(void) {
    TEST("key modifiers (shift/ctrl/alt) preserved in queue");
    input_init();
    
    KeyEvent ev = { 
        .scancode = 0x1E,  /* 'a' */
        .keycode = 0x1E,
        .kind = KEY_EVENT_DOWN,
        .modifiers = MOD_SHIFT | MOD_CTRL
    };
    
    input_key_push(ev);
    
    KeyEvent out;
    CHECK(input_key_poll(&out) == 1, "should poll event");
    CHECK(out.modifiers == (MOD_SHIFT | MOD_CTRL), "modifiers preserved");
    
    input_shutdown();
    PASS();
}

static void test_key_queue_wraparound(void) {
    TEST("key queue handles wraparound (256 capacity)");
    input_init();
    
    /* Push QUEUE_SIZE + 10 events */
    for (int i = 0; i < 266; i++) {
        KeyEvent ev = { .scancode = i, .keycode = i, .kind = KEY_EVENT_DOWN, .modifiers = 0 };
        input_key_push(ev);
    }
    
    /* Should only keep last QUEUE_SIZE (256) events */
    KeyEvent out;
    int count = 0;
    while (input_key_poll(&out)) {
        count++;
        /* Last 256 events should have scancodes 10 to 265 */
        CHECK(out.scancode == (uint32_t)(count + 9), "wraparound preserves order");
    }
    CHECK(count == 256, "queue maintains capacity limit");
    
    input_shutdown();
    PASS();
}

static void test_key_pressed(void) {
    TEST("input_key_pressed detects held keys");
    input_init();
    
    /* Push a key down event */
    KeyEvent ev_down = { .scancode = 0x1C, .keycode = 0x1C, .kind = KEY_EVENT_DOWN, .modifiers = 0 };
    KeyEvent ev_up = { .scancode = 0x1C, .keycode = 0x1C, .kind = KEY_EVENT_UP, .modifiers = 0 };
    
    input_key_push(ev_down);
    
    /* Should detect as pressed (checks last 16 events for down) */
    CHECK(input_key_pressed(0x1C) == 1, "key_down should be detected as pressed");
    
    input_key_push(ev_up);
    
    /* After up event, should not be detected as pressed */
    CHECK(input_key_pressed(0x1C) == 0, "key_up should not be detected as pressed");
    
    input_shutdown();
    PASS();
}

/* ── Mouse Queue Tests ───────────────────────────────────────────── */

static void test_mouse_push_poll(void) {
    TEST("input_mouse_push + input_mouse_poll roundtrip");
    input_init();
    
    /* Use dx=dy=0 to indicate absolute position */
    MouseEvent ev1 = { .x = 100, .y = 200, .dx = 0, .dy = 0, .buttons = 1, .scroll = 0 };
    MouseEvent ev2 = { .x = 105, .y = 210, .dx = 0, .dy = 0, .buttons = 0, .scroll = 0 };
    
    input_mouse_push(ev1);
    input_mouse_push(ev2);
    
    MouseEvent out;
    CHECK(input_mouse_poll(&out) == 1, "should poll first event");
    CHECK(out.x == 100 && out.y == 200, "absolute position preserved");
    CHECK(out.buttons == 1, "buttons preserved");
    
    CHECK(input_mouse_poll(&out) == 1, "should poll second event");
    CHECK(out.x == 105 && out.y == 210, "absolute position preserved");
    CHECK(out.buttons == 0, "buttons cleared");
    
    CHECK(input_mouse_poll(&out) == 0, "queue should be empty");
    
    input_shutdown();
    PASS();
}

static void test_mouse_get_pos(void) {
    TEST("input_mouse_get_pos returns last known position");
    input_init();
    
    int x, y;
    input_mouse_get_pos(&x, &y);
    CHECK(x == 0 && y == 0, "initial position is 0,0");
    
    /* Use dx=dy=0 to indicate absolute position */
    MouseEvent ev = { .x = 300, .y = 400, .dx = 0, .dy = 0, .buttons = 0, .scroll = 0 };
    input_mouse_push(ev);
    
    input_mouse_get_pos(&x, &y);
    CHECK(x == 300 && y == 400, "position updated after push");
    
    input_shutdown();
    PASS();
}

static void test_mouse_queue_wraparound(void) {
    TEST("mouse queue handles wraparound");
    input_init();
    
    for (int i = 0; i < 266; i++) {
        /* Use dx=dy=0 for absolute positioning */
        MouseEvent ev = { .x = i, .y = i, .dx = 0, .dy = 0, .buttons = 0, .scroll = 0 };
        input_mouse_push(ev);
    }
    
    MouseEvent out;
    int count = 0;
    while (input_mouse_poll(&out)) {
        count++;
        CHECK(out.x == (count + 9), "wraparound preserves position");
    }
    CHECK(count == 256, "queue maintains capacity limit");
    
    input_shutdown();
    PASS();
}

/* ── Mixed Tests ─────────────────────────────────────────────────── */

static void test_key_and_mouse_independent(void) {
    TEST("key and mouse queues operate independently");
    input_init();
    
    KeyEvent kev = { .scancode = 0x1C, .keycode = 0x1C, .kind = KEY_EVENT_DOWN, .modifiers = 0 };
    MouseEvent mev = { .x = 100, .y = 100, .dx = 0, .dy = 0, .buttons = 1, .scroll = 0 };
    
    input_key_push(kev);
    input_mouse_push(mev);
    
    KeyEvent kout;
    MouseEvent mout;
    
    CHECK(input_key_poll(&kout) == 1, "key queue has event");
    CHECK(input_mouse_poll(&mout) == 1, "mouse queue has event");
    
    CHECK(input_key_poll(&kout) == 0, "key queue empty");
    CHECK(input_mouse_poll(&mout) == 0, "mouse queue empty");
    
    input_shutdown();
    PASS();
}

/* ── Stress Test ─────────────────────────────────────────────────── */

static void test_mixed_burst(void) {
    TEST("burst of mixed key/mouse events");
    input_init();
    
    for (int i = 0; i < 100; i++) {
        KeyEvent kev = { .scancode = i, .keycode = i, .kind = KEY_EVENT_DOWN, .modifiers = 0 };
        /* Use dx=dy=0 for absolute positioning */
        MouseEvent mev = { .x = i, .y = i, .dx = 0, .dy = 0, .buttons = i & 1, .scroll = 0 };
        input_key_push(kev);
        input_mouse_push(mev);
    }
    
    int key_count = 0, mouse_count = 0;
    KeyEvent kout;
    MouseEvent mout;
    
    while (input_key_poll(&kout)) key_count++;
    while (input_mouse_poll(&mout)) mouse_count++;
    
    CHECK(key_count == 100, "all 100 key events retrieved");
    CHECK(mouse_count == 100, "all 100 mouse events retrieved");
    
    input_shutdown();
    PASS();
}

/* ── Main ────────────────────────────────────────────────────────── */

int main(void) {
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS Kernel Input Subsystem Test Suite      ║\n");
    printf("║  Cell 202: GUI input dispatch (input.c queue)   ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");
    
    test_input_init();
    test_input_init_shutdown();
    test_key_push_poll();
    test_key_modifiers();
    test_key_queue_wraparound();
    test_key_pressed();
    test_mouse_push_poll();
    test_mouse_get_pos();
    test_mouse_queue_wraparound();
    test_key_and_mouse_independent();
    test_mixed_burst();
    
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("═══════════════════════════════════════════════════\n");
    
    return g_fail > 0 ? 1 : 0;
}