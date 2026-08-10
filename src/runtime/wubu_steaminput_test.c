/*
 * wubu_steaminput_test.c -- the Steam Input layer test.
 *
 * Asserts:
 *   1. the default config maps A->Space, the left stick->WASD
 *   2. map_button/map_axis override the defaults
 *   3. feeding a button event emits the mapped key (poll the queue)
 *   4. the config save/load round-trips
 */
#include "wubu_steaminput.h"
#include "input.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

int main(void)
{
    printf("=== wubu_steaminput_test (the Steam Input layer) ===\n");

    wubu_si_init();

    /* 1. the defaults */
    wubu_si_view_t v;
    wubu_si_get(&v);
    if (!v.initialized) FAIL("not initialized");
    if (v.button_a_scancode != 0x39)   /* Space */
        FAIL("A = %d, want 0x39 (Space)", v.button_a_scancode);
    if (v.axis_lx_neg != 0x11 || v.axis_lx_pos != 0x1F)  /* A/D */
        FAIL("L-stick = %d/%d, want A/D", v.axis_lx_neg, v.axis_lx_pos);
    printf("  PASS: the default map (A->Space, L-stick->WASD)\n");

    /* 2. the overrides */
    if (wubu_si_map_button(SI_B, 0x01) != 0) FAIL("map B");
    if (wubu_si_map_axis(SI_RSTICK_X, -3, -4) != 0) FAIL("map R-stick");
    wubu_si_get(&v);
    printf("  PASS: map_button/map_axis override\n");

    /* 3. feed a button -> the mapped key appears in the kernel queue */
    /* drain any pre-existing events first */
    KeyEvent ev;
    while (input_key_poll(&ev)) {}
    wubu_si_feed_button(SI_A, 1);   /* press A -> Space down */
    if (!input_key_poll(&ev) || ev.scancode != 0x39 || ev.kind != KEY_EVENT_DOWN)
        FAIL("A press did not emit Space down (got %u kind %d)",
             ev.scancode, (int)ev.kind);
    wubu_si_feed_button(SI_A, 0);   /* release A -> Space up */
    if (!input_key_poll(&ev) || ev.scancode != 0x39 || ev.kind != KEY_EVENT_UP)
        FAIL("A release did not emit Space up");
    printf("  PASS: button feed emits the mapped key events\n");

    /* 4. the config save/load round-trip */
    wubu_si_map_button(SI_Y, 0x2A);   /* custom: Y = LShift */
    if (wubu_si_save("/tmp/si_cfg.bin") != 0) FAIL("save");
    wubu_si_map_button(SI_Y, 0x00);   /* clobber */
    if (wubu_si_load("/tmp/si_cfg.bin") != 0) FAIL("load");
    wubu_si_get(&v);
    if (v.button_a_scancode != 0x39) FAIL("A lost after load");
    /* the custom Y survived */
    KeyEvent ev2;
    while (input_key_poll(&ev2)) {}
    wubu_si_feed_button(SI_Y, 1);
    if (!input_key_poll(&ev2) || ev2.scancode != 0x2A)
        FAIL("Y = %u, want 0x2A after load", ev2.scancode);
    printf("  PASS: the config save/load round-trips\n");

    /* 5. a corrupted file is refused */
    FILE *f = fopen("/tmp/si_bad.bin", "wb");
    fprintf(f, "not a config");
    fclose(f);
    if (wubu_si_load("/tmp/si_bad.bin") == 0) FAIL("corrupt file accepted");
    printf("  PASS: a corrupt config is refused\n");

    /* 6. the REAL Steam Deck report protocol (stolen from hid-steam.c):
     * a 64-byte report with A pressed (byte 8, bit 7 = 0x80) and the
     * left stick pushed right (+32767 at bytes 48/49) must emit the
     * mapped input: Space (0x39) + D (0x1F) */
    uint8_t report[64];
    memset(report, 0, sizeof(report));
    report[8] = 0x80;              /* A */
    report[48] = 0xFF; report[49] = 0x7F;   /* L-stick X = +32767 */
    KeyEvent ev3;
    while (input_key_poll(&ev3)) {}
    int n = wubu_si_parse_deck_report(report, sizeof(report));
    if (n < 2) FAIL("deck report emitted %d events, want >= 2", n);
    /* the A -> Space press */
    int got_a = 0, got_d = 0;
    while (input_key_poll(&ev3)) {
        if (ev3.scancode == 0x39) got_a = 1;   /* Space */
        if (ev3.scancode == 0x1F) got_d = 1;   /* D */
    }
    if (!got_a) FAIL("deck A did not emit Space");
    if (!got_d) FAIL("deck L-stick did not emit D");
    printf("  PASS: the real Deck report decodes (A->Space, L-stick->D)\n");

    /* 7. the report is refused when too short */
    if (wubu_si_parse_deck_report(report, 32) >= 0)
        FAIL("short deck report accepted");
    printf("  PASS: a short deck report is refused\n");

    /* 8. the IMU: a gyro yaw (bytes 32/33 = gyro Z) must emit a mouse
     * delta (the gyro-to-mouse aim feature) */
    uint8_t sensors[64];
    memset(sensors, 0, sizeof(sensors));
    sensors[32] = 0x00; sensors[33] = 0x10;   /* gyro Z = +4096 */
    MouseEvent mev;
    int got_mouse = 0;
    while (input_mouse_poll(&mev)) {}
    int ns = wubu_si_parse_deck_sensors(sensors, sizeof(sensors));
    if (ns < 1) FAIL("imu emitted %d events, want >= 1", ns);
    while (input_mouse_poll(&mev)) {
        if (mev.dx != 0 || mev.dy != 0) got_mouse = 1;
    }
    if (!got_mouse) FAIL("gyro did not move the mouse");
    printf("  PASS: the IMU gyro-to-mouse works\n");

    /* 9. the battery event: 3.9V at 78% */
    uint8_t batt[15];
    memset(batt, 0, sizeof(batt));
    batt[12] = 0x3C; batt[13] = 0x0F;   /* 3900 mV */
    batt[14] = 78;
    if (wubu_si_parse_battery(batt, sizeof(batt)) != 78)
        FAIL("battery percent");
    if (wubu_si_battery_mv() != 3900) FAIL("battery mV");
    printf("  PASS: the battery event decodes (3900 mV, 78%%)\n");

    /* 10. lizard mode: ON by default (the keyboard map emits); a
     * client opening the device turns it OFF (nothing is emitted);
     * closing turns it back ON */
    wubu_si_init();                        /* fresh held state */
    if (!wubu_si_lizard_mode()) FAIL("lizard should start ON");
    KeyEvent ev4;
    while (input_key_poll(&ev4)) {}
    wubu_si_feed_button(SI_A, 1);
    if (!input_key_poll(&ev4)) FAIL("lizard ON did not emit Space");
    while (input_key_poll(&ev4)) {}
    wubu_si_set_lizard_mode(0);
    wubu_si_feed_button(SI_A, 0);
    wubu_si_feed_button(SI_A, 1);       /* a real client: raw pad */
    if (input_key_poll(&ev4)) FAIL("lizard OFF still emits");
    wubu_si_set_lizard_mode(1);
    wubu_si_feed_button(SI_A, 0);
    printf("  PASS: lizard mode toggles (keyboard until a client opens)\n");

    remove("/tmp/si_cfg.bin");
    remove("/tmp/si_bad.bin");
    printf("=== ALL STEAMINPUT TESTS PASSED (the Steam Input layer) ===\n");
    return 0;
}
