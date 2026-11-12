/*
 * wubu_evdev_test.c -- the evdev pipeline behavior gate.
 *
 * This is not an assert-nothing test: every case injects a real evdev code
 * through the same routing path a device takes and then reads the kernel
 * input queue to prove the mapping actually happened. If the evdev->Steam
 * Input table drifts, this fails.
 */
#include "../../egojar.h"

#include "../input.h"
#include "../wubu_evdev.h"
#include "../../runtime/wubu_steaminput.h"

#include <stdio.h>

static int g_fail;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); g_fail++; } \
} while (0)

/* Drain the queue and return the last key seen (or -1). */
static int drain_last_scancode(void)
{
    KeyEvent k;
    int last = -1;
    while (input_key_poll(&k) == 1)
        last = (int)k.scancode;
    return last;
}

static int queue_empty(void)
{
    KeyEvent k;
    return input_key_poll(&k) == 0;
}

int main(void)
{
    input_init();
    wubu_si_init();

    /* ---- 1. the A button maps to Space (0x39) --------------------- */
    wubu_evdev_inject(EV_KEY, BTN_SOUTH, 1);
    KeyEvent k;
    CHECK(input_key_poll(&k) == 1, "BTN_SOUTH emits a key");
    CHECK(k.scancode == 0x39, "BTN_SOUTH maps to Space (the default A binding)");
    CHECK(k.kind == KEY_EVENT_DOWN, "the press is a DOWN edge");

    /* ---- 2. releasing it is an UP edge ----------------------------- */
    wubu_evdev_inject(EV_KEY, BTN_SOUTH, 0);
    CHECK(input_key_poll(&k) == 1 && k.scancode == 0x39 &&
          k.kind == KEY_EVENT_UP, "releasing BTN_SOUTH emits Space UP");

    /* ---- 3. no edge when the state does not change ----------------- */
    wubu_evdev_inject(EV_KEY, BTN_SOUTH, 0);
    CHECK(queue_empty(), "a repeated release emits nothing");

    /* ---- 4. a second press is an edge again ------------------------ */
    wubu_evdev_inject(EV_KEY, BTN_SOUTH, 1);
    CHECK(input_key_poll(&k) == 1 && k.kind == KEY_EVENT_DOWN,
          "a second press emits DOWN again");

    /* ---- 5. the B button maps to Esc (0x01) ------------------------ */
    wubu_evdev_inject(EV_KEY, BTN_SOUTH, 0);          /* clear the queue */
    drain_last_scancode();
    wubu_evdev_inject(EV_KEY, BTN_EAST, 1);
    CHECK(input_key_poll(&k) == 1 && k.scancode == 0x01,
          "BTN_EAST maps to Esc (the default B binding)");

    /* ---- 6. the left stick: full right is D (0x1F) ----------------- */
    drain_last_scancode();
    wubu_evdev_inject(EV_ABS, ABS_X, 32767);
    CHECK(drain_last_scancode() == 0x1F,
          "ABS_X full right emits D (the left-stick X positive binding)");

    /* ---- 7. full left is A (0x11) ---------------------------------- */
    wubu_evdev_inject(EV_ABS, ABS_X, -32768);
    CHECK(drain_last_scancode() == 0x11,
          "ABS_X full left emits A (the left-stick X negative binding)");

    /* ---- 8. the center deadzone emits nothing ---------------------- */
    wubu_evdev_inject(EV_ABS, ABS_X, 0);
    CHECK(queue_empty(), "a centered stick is inside the deadzone and emits nothing");

    /* ---- 9. the d-pad maps to the arrow keys ----------------------- */
    wubu_evdev_inject(EV_KEY, BTN_DPAD_UP, 1);
    CHECK(input_key_poll(&k) == 1 && k.scancode == 0x48,
          "BTN_DPAD_UP maps to the Up arrow");

    /* ---- 10. no device is required for routing to work ------------- */
    CHECK(!wubu_evdev_present(),
          "no evdev device is open in the test environment");

    if (g_fail == 0) {
        printf("wubu_evdev_test: all checks passed\n");
        return 0;
    }
    printf("wubu_evdev_test: %d FAILED\n", g_fail);
    return 1;
}
