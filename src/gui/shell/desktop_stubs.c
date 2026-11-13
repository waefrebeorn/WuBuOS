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

__attribute__((weak)) void desktop_launch_icon(int icon_id) { (void)icon_id; }
