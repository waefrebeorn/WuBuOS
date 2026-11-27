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
#include "../../egojar.h"
#include "../wubu_settings.h"
#include "wubu_shell_greeter.h"
#include "wm.h"
#include "wubu_theme.h"
#include "../kernel/vbe.h"
#include "../../runtime/wubu_onboarding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Type a string through the greeter's key path (printable ASCII). */
static void type_str(const char *s) {
    for (; *s; s++) wubu_shell_greeter_key((uint32_t)(unsigned char)*s);
}

/* From the THEME page, fill the account form and reach the READY page:
 * theme ->(Enter) account, type user/pw/confirm, Enter to advance each field
 * (including the keyboard field), then network ->(Enter) ready.  If kbd_down is
 * nonzero, press Down on the keyboard field that many times to cycle the
 * keyboard layout before advancing. */
static void drive_account_form(const char *user, const char *pw, int kbd_down) {
    wubu_shell_greeter_key(0x1C);          /* theme page -> account page */
    type_str(user);
    wubu_shell_greeter_key(0x1C);          /* -> password field */
    type_str(pw);
    wubu_shell_greeter_key(0x1C);          /* -> confirm field */
    type_str(pw);
    wubu_shell_greeter_key(0x1C);          /* confirm -> keyboard field */
    for (int i = 0; i < kbd_down; i++) wubu_shell_greeter_key(0xE050);  /* cycle layout */
    wubu_shell_greeter_key(0x1C);          /* keyboard -> network page */
    wubu_shell_greeter_key(0x1C);          /* network -> ready page */
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
    /* NOT modal: a modal wizard put the Start menu UNDER itself and Escape only stepped back, so a fresh
     * system could not reach its own apps until the account form was filled. */
    {
        DosGuiWindow *gw = wm_get_focused();
        CHECK(gw != NULL, "the greeter takes focus");
        CHECK(gw != NULL && !wm_is_modal(gw), "and it does NOT swallow the desktop (Start opens above it)");
    }
    CHECK(wubu_shell_greeter_selected_theme() ==
          wubu_settings_get()->theme.theme_id,
          "initial theme selection is the product default (not hardcoded)");
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

    /* -- 4. account: type the form, then click Start -> account record persisted -- */
    drive_account_form("wubu", "hunter2", 0);
    int bx, by, bw, bh;
    wubu_shell_greeter_button_rect(&bx, &by, &bw, &bh);
    wubu_shell_greeter_click(bx + bw / 2, by + bh / 2, 1);
    CHECK(!wubu_shell_greeter_is_open(), "Start closes the greeter");
    CHECK(access(sent, F_OK) == 0, "onboarding sentinel file written");
    const char *sig = wubu_onboarding_signature();
    CHECK(sig && strstr(sig, "theme=") != NULL,
          "sentinel carries the chosen theme signature");
    CHECK(sig && strstr(sig, "user=wubu") != NULL,
          "sentinel records the OOBE username");
    CHECK(sig && strstr(sig, "pw=$6$") != NULL,
          "sentinel records a $6$ SHA-512 password hash");
    CHECK(sig && strstr(sig, "kbd=us") != NULL,
          "sentinel records the keyboard layout");
    printf("        signature: %s\n", sig ? sig : "(null)");

    /* -- 4b. finishing must APPLY the theme, not just record it.
     * The sentinel used to be written while wubu_theme_set() was never called,
     * so picking "Zune" left the desktop on whatever settings.conf held and the
     * signature claimed a theme that was not in effect. */
    int chosen = wubu_shell_greeter_selected_theme();
    CHECK(wubu_theme_current() == chosen,
          "finishing applies the chosen theme (live palette changed)");
    CHECK(wubu_settings_get()->theme.theme_id == chosen,
          "the settings object owns the theme and was updated");

    /* -- 5. reload: same sentinel -> DESKTOP -- */
    wubu_onboarding_load(sent);
    CHECK(!wubu_onboarding_first_boot(), "sentinel read back on reload");
    CHECK(wubu_onboarding_target() == WUBU_ONBOARD_DESKTOP,
          "second boot targets the DESKTOP (greeter not relaunched)");



    wubu_onboarding_load(sent);
    wubu_shell_greeter_run();
    CHECK(wubu_shell_greeter_is_open(), "greeter reopened for validation path");
    wubu_shell_greeter_key(0x1C);                 /* theme -> account */
    type_str("wubu");
    wubu_shell_greeter_key(0x1C);                 /* -> password */
    type_str("hunter2");
    wubu_shell_greeter_key(0x1C);                 /* -> confirm */
    type_str("hunt3r2");                          /* mismatch */
    wubu_shell_greeter_key(0x1C);
    CHECK(wubu_shell_greeter_step() == 1,
          "mismatched confirmation keeps the wizard on the account page");
    wubu_shell_greeter_key(0x1C);                 /* -> confirm? no: mismatch stays */
    wubu_shell_greeter_key(27);                   /* escape to dismiss without signing */
    wubu_shell_greeter_key(27);
    wubu_shell_greeter_key(27);
    CHECK(!wubu_shell_greeter_is_open(), "escape dismisses the greeter");
    unlink(sent);

    /* -- 6. keyboard path: full wizard via keys on a fresh first boot -- */
    unlink(sent);
    wubu_onboarding_load(sent);
    wubu_shell_greeter_run();
    CHECK(wubu_shell_greeter_is_open(), "greeter reopened for keyboard path");
    while (wubu_shell_greeter_selected_theme() > 0)
        wubu_shell_greeter_key(0xE048);        /* Up (WM scancode) */
    wubu_shell_greeter_key(0xE050);            /* Down */
    CHECK(wubu_shell_greeter_selected_theme() == 1, "Down arrow moves selection");
    wubu_shell_greeter_key(0xE048);            /* Up */
    CHECK(wubu_shell_greeter_selected_theme() == 0, "Up arrow moves selection back");
    drive_account_form("alice", "correcthorse", 1);   /* cycle us->uk */
    wubu_shell_greeter_key(0x1C);              /* ready -> Start */
    CHECK(!wubu_shell_greeter_is_open(), "Enter completes the greeter");
    CHECK(access(sent, F_OK) == 0, "keyboard path also persists the sentinel");
    sig = wubu_onboarding_signature();
    CHECK(sig && strstr(sig, "user=alice") != NULL, "keyboard path records the user");
    CHECK(sig && strstr(sig, "pw=$6$") != NULL, "keyboard path records the $6$ hash");
    CHECK(sig && strstr(sig, "kbd=uk") != NULL,
          "cycling the layout on the keyboard field records kbd=uk");

    wubu_onboarding_load(sent);
    CHECK(wubu_onboarding_target() == WUBU_ONBOARD_DESKTOP,
          "keyboard path also flips the next boot to DESKTOP");

    unlink(sent);
    printf("\n=== greeter gate: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
