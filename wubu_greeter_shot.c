/*
 * wubu_greeter_shot.c -- GAP F capture harness: the visible first-boot greeter.
 *
 * Renders the real greeter dialog through the WM/theme/VBE stack and writes
 * PPM frames so the first-boot screen can be inspected like the desktop:
 *   00: first-boot desktop before the greeter
 *   01: greeter open (theme 0 selected)
 *   02: a different theme selected (Down x2)
 *   03: after Start -> sentinel persisted, desktop only
 *
 * Exit 0 on success.
 */
#include "src/gui/shell/wubu_shell_greeter.h"
#include "src/gui/dosgui_wm.h"
#include "src/gui/wubu_theme.h"
#include "src/kernel/vbe.h"
#include "src/runtime/wubu_onboarding.h"
#include "src/tools/screenshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SHOT_W 1024
#define SHOT_H 768
#define OUT_DIR "/tmp/wubu_greeter_shots"

static void frame(const char *label) {
    char path[512];
    snprintf(path, sizeof(path), OUT_DIR "/%s.ppm", label);
    if (wubu_shot_fullscreen(path, SHOT_FMT_PPM) == 0)
        printf("  frame: %s -> %s\n", label, path);
    else
        printf("  frame: %s -> WRITE FAILED\n", label);
}

/* Render a frame: WM draws into the VBE back buffer, then swap so the
 * screenshot (which reads the front buffer) sees it -- same as desktop_shot. */
static void tick(void) {
    dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
    vbe_swap();
}

int main(void) {
    printf("WuBuOS first-boot greeter capture: %dx%d\n", SHOT_W, SHOT_H);
    system("rm -rf " OUT_DIR);
    system("mkdir -p " OUT_DIR);

    if (vbe_init(SHOT_W, SHOT_H) != 0) { printf("vbe_init failed\n"); return 1; }
    wubu_theme_init();
    wubu_theme_set(THEME_WIN98_CLASSIC);
    dosgui_wm_init(SHOT_W, SHOT_H);

    /* First boot: no sentinel -> the greeter is the target. */
    const char *sent = "/tmp/wubu_greeter_shot.sentinel";
    unlink(sent);
    wubu_onboarding_load(sent);
    printf("  onboarding target: %s\n",
           wubu_onboarding_target() == WUBU_ONBOARD_GREETER ? "GREETER" : "DESKTOP");

    tick();
    frame("00_desktop_before");

    if (wubu_shell_greeter_run() != 0) { printf("greeter failed to open\n"); return 1; }
    tick();
    frame("01_greeter_open");

    wubu_shell_greeter_key(31);   /* Down */
    wubu_shell_greeter_key(31);   /* Down -> theme 2 */
    tick();
    frame("02_theme_selected");
    printf("  selected theme: %d\n", wubu_shell_greeter_selected_theme());

    wubu_shell_greeter_key('\r'); /* Start */
    tick();
    frame("03_after_start");
    printf("  signature: %s\n", wubu_shell_greeter_signature());

    wubu_onboarding_load(sent);
    printf("  reload target: %s\n",
           wubu_onboarding_target() == WUBU_ONBOARD_GREETER ? "GREETER" : "DESKTOP");

    unlink(sent);
    printf("Done. Frames in " OUT_DIR "\n");
    return 0;
}
