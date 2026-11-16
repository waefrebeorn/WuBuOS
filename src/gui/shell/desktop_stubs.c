/*
 * desktop_stubs.c -- weak glue so WM-bearing tests can link without the shell.
 *
 * wm_input.c dispatches icon clicks to desktop_launch_icon(), which lives in
 * shell/desktop.c together with the whole shell (egojar, session manager, ns
 * bridge). Tests that compile wm_input.c directly cannot link that, so they get
 * this no-op instead. Same convention as wm_test_stub.c and apps_test_stubs.c.
 *
 * There is exactly ONE stub here on purpose: if a second shell dependency shows
 * up, it belongs in this file rather than being copied into another test's
 * private stub list, where it would silently diverge.
 */

#include <stdbool.h>
#include <stddef.h>

__attribute__((weak)) void desktop_launch_icon(int icon_id) { (void)icon_id; }

/* wm.c (WM render/geometry path) calls wubu_bonzi_place() to park the mascot.
 * The real bonzi lives in gui/wubu_bonzi.c (the full AGI/tasking stack), which
 * massing GUI tests must not drag in. Weak no-op: no placement. */
__attribute__((weak))
bool wubu_bonzi_place(int screen_w, int screen_h, int taskbar_h)
{
    (void)screen_w; (void)screen_h; (void)taskbar_h;
    return false;
}

/* wm_layout.c asks for the wallpaper logo path; the real resolver is in
 * gui/shell/wubu_wallpaper.c which pulls the image loader. Tests link without
 * it, so return NULL (no logo overlay) rather than forcing the loader. */
__attribute__((weak))
const char *wubu_wallpaper_logo_path(void) { return NULL; }
