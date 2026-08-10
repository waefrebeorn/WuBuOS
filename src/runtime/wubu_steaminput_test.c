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

    remove("/tmp/si_cfg.bin");
    remove("/tmp/si_bad.bin");
    printf("=== ALL STEAMINPUT TESTS PASSED (the Steam Input layer) ===\n");
    return 0;
}
