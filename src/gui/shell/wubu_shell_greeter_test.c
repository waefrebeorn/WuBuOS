/*
 * wubu_shell_greeter_test.c -- GAP F behaviour gate.
 *
 * Proves the first-boot greeter is a REAL screen on the * stack, not a
 * label:
 *   1. first boot (no sentinel) targets the GREETER;
 *   2. opening it draws real pixels through the WM/theme/VBE (frame changes);
 *   3. a mouse click on a theme row changes the selection;
 *   4. clicking Start persists the onboarding sentinel with the chosen theme;
 *   5. a reload of the same sentinel path targets the DESKTOP (not the greeter);
 *   6. the keyboard path (Down + Enter) drives the same completion.
 *
 * C11, hosted.  Exit 0 = all gates passed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "wubu_shell_greeter.h"
#include "wm.h"
#include "wubu_theme.h"
#include "../kernel/vbe.h"
#include "../../runtime/wubu_onboarding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  [PASS] %s\n", msg); } \
    else      { g_fail++; printf("  [FAIL] %s\n", msg); } \
} while (0)

#define SHOT_W 1024
#define SHOT_H 768

/* Count non-background pixels in a rect of the live VBE buffer. */
static int count_pixels(int x0, int y0, int x1, int y1, uint32_t bg) {
    int n = 0;
    for (int y = y0; y < y1; y += 2)
        for (int x = x0; x < x1; x += 2)
            if (vbe_get_pixel(x, y) != bg) n++;
    return n;
}

static void write_sentinel(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (f) { fputs(content, f); fclose(f); }
}

int main(void) {
    printf("=== WuBuOS first-boot greeter behaviour gate ===\n");

    /* -- 1. first boot targets the GREETER -- */
    const char *sent = "/tmp/wubu_greeter_test.sentinel";
    unlink(sent);
    wubu_onboarding_load(sent);
    CHECK(wubu_onboarding_first_boot(), "first boot (no sentinel) detected");
    CHECK(wubu_onboarding_target() == WUBU_ONBOARD_GREETER,
          "first boot targets the GREETER");

    if (vbe_init(SHOT_W, SHOT_H) != 0) { printf("vbe_init failed\n"); return 1; }
    wubu_theme_init();
    wubu_theme_set(THEME_WIN98_CLASSIC);
    wm_init(SHOT_W, SHOT_H);

    /* Baseline frame: desktop only. */
    wm_render_desktop(NULL, SHOT_W, SHOT_H);
    uint32_t before[40][40];
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 40; x++)
            before[y][x] = vbe_get_pixel(240 + x * 12, 240 + y * 10);

    /* -- 2. open the greeter: it must draw real pixels -- */
    wubu_shell_greeter_run();
    CHECK(wubu_shell_greeter_is_open(), "greeter dialog is open");
    wm_render_desktop(NULL, SHOT_W, SHOT_H);
    int delta = 0;
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 40; x++)
            if (vbe_get_pixel(240 + x * 12, 240 + y * 10) != before[y][x]) delta++;
    CHECK(delta > 40,
          "greeter renders real pixels over the desktop (frame changed)");
    printf("        changed pixels in dialog region: %d/1600\n", delta);

    /* -- 3. mouse: click a theme row -> selection changes -- */
    int rx, ry, rw, rh;
    wubu_shell_greeter_theme_row(2, &rx, &ry, &rw, &rh);
    int sel0 = wubu_shell_greeter_selected_theme();
    wubu_shell_greeter_click(rx + 8, ry + rh / 2, 1);
    CHECK(wubu_shell_greeter_selected_theme() == 2,
          "clicking theme row 2 selects it");
    (void)sel0;

    /* -- 4. mouse: click Start -> sentinel persisted -- */
    int bx, by, bw, bh;
    wubu_shell_greeter_button_rect(&bx, &by, &bw, &bh);
    wubu_shell_greeter_click(bx + bw / 2, by + bh / 2, 1);
    CHECK(!wubu_shell_greeter_is_open(), "Start closes the greeter");
    CHECK(access(sent, F_OK) == 0, "onboarding sentinel file written");
    const char *sig = wubu_onboarding_signature();
    CHECK(sig && strstr(sig, "theme=") != NULL,
          "sentinel carries the chosen theme signature");
    printf("        signature: %s\n", sig ? sig : "(null)");

    /* -- 5. reload: same sentinel -> DESKTOP -- */
    wubu_onboarding_load(sent);
    CHECK(!wubu_onboarding_first_boot(), "sentinel read back on reload");
    CHECK(wubu_onboarding_target() == WUBU_ONBOARD_DESKTOP,
          "second boot targets the DESKTOP (greeter not relaunched)");

    /* -- 6. keyboard path: Down + Enter on a fresh first boot -- */
    unlink(sent);
    wubu_onboarding_load(sent);
    wubu_shell_greeter_run();
    CHECK(wubu_shell_greeter_is_open(), "greeter reopened for keyboard path");
    while (wubu_shell_greeter_selected_theme() > 0)
        wubu_shell_greeter_key(30);            /* Up to the first theme */
    wubu_shell_greeter_key(31);                /* Down */
    CHECK(wubu_shell_greeter_selected_theme() == 1, "Down arrow moves selection");
    wubu_shell_greeter_key(30);                /* Up */
    CHECK(wubu_shell_greeter_selected_theme() == 0, "Up arrow moves selection back");
    wubu_shell_greeter_key('\r');          /* Enter = Start */
    CHECK(!wubu_shell_greeter_is_open(), "Enter completes the greeter");
    CHECK(access(sent, F_OK) == 0, "keyboard path also persists the sentinel");

    wubu_onboarding_load(sent);
    CHECK(wubu_onboarding_target() == WUBU_ONBOARD_DESKTOP,
          "keyboard path also flips the next boot to DESKTOP");

    unlink(sent);
    printf("\n=== greeter gate: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
