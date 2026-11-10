/*
 * daemon_panel_test.c  --  Daemon Panel Unit Test Suite
 *
 * Tests daemon panel logic with stubbed GUI dependencies.
 * We provide weak stubs for all GUI functions the panel calls,
 * then test the panel's public API and internal state management.
 */

#define _POSIX_C_SOURCE 200809L
#include "daemon_panel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* -- Weak stubs for all GUI/VBE functions the panel depends on -- */

struct DosGuiWindow;
typedef struct DosGuiWindow DosGuiWindow;

__attribute__((weak)) int systray_add(const char *n, uint32_t c,
    void (*on_click)(void), void (*on_right_click)(void)) {
    (void)n; (void)c; (void)on_click; (void)on_right_click; return 0; }
__attribute__((weak)) void systray_remove(const char *n) { (void)n; }
__attribute__((weak)) void systray_set_notification_count(const char *n, int c) {
    (void)n; (void)c; }
__attribute__((weak)) int notif_center_add(const char *a, const char *s,
    const char *b, int u) { (void)a; (void)s; (void)b; (void)u; return 0; }
__attribute__((weak)) DosGuiWindow *wm_create(int x, int y, int w, int h,
    const char *t) { (void)x; (void)y; (void)w; (void)h; (void)t; return NULL; }
__attribute__((weak)) void wm_destroy(DosGuiWindow *w) { (void)w; }

/* VBE stubs */
__attribute__((weak)) void vbe_fill_rect(int x, int y, int w, int h, uint32_t c) {
    (void)x; (void)y; (void)w; (void)h; (void)c; }
__attribute__((weak)) void vbe_draw_text(int x, int y, const char *t, uint32_t c, int s) {
    (void)x; (void)y; (void)t; (void)c; (void)s; }

/* Theme stub */
typedef struct { uint32_t startmenu_bg; uint32_t win_title_active; uint32_t win_title_text; uint32_t win_face; uint32_t border_dark; } WubuThemeColors;
static WubuThemeColors g_stub_theme = { 0x000000, 0x0000AA, 0xFFFFFF, 0xC0C0C0, 0x808080 };
__attribute__((weak)) const WubuThemeColors *wubu_theme_colors(void) { return &g_stub_theme; }

static int g_run = 0, g_pass = 0;

#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s (line %d)\n", msg, __LINE__); } \
} while(0)

int main(void) {
    printf("=== Daemon Panel Unit Test Suite ===\n\n");

    /* -- Lifecycle ------------------------------------------------ */
    printf("[Lifecycle]\n");
    {
        T(daemon_panel_init() == 0, "init succeeds");
        T(daemon_panel_archd_state() >= 0, "archd state valid");
        T(daemon_panel_holyd_state() >= 0, "holyd state valid");
        daemon_panel_shutdown();
        T(1, "shutdown completes");
    }

    /* -- Initial state -------------------------------------------- */
    printf("\n[Initial State]\n");
    {
        daemon_panel_init();
        T(daemon_panel_container_count() == 0, "0 containers");
        T(daemon_panel_holyd_session_count() == 0, "0 sessions");
        T(daemon_panel_container_name(0) == NULL, "name[0] NULL");
        T(daemon_panel_container_state(0) == NULL, "state[0] NULL");
        T(daemon_panel_holyd_session_name(0) == NULL, "session[0] NULL");
        daemon_panel_shutdown();
    }

    /* -- Tick stability ------------------------------------------- */
    printf("\n[Tick Stability]\n");
    {
        daemon_panel_init();
        for (int i = 0; i < 100; i++) daemon_panel_tick();
        T(1, "100 ticks no crash");
        T(daemon_panel_archd_state() >= 0, "archd valid after ticks");
        T(daemon_panel_holyd_state() >= 0, "holyd valid after ticks");
        daemon_panel_shutdown();
        daemon_panel_tick();
        T(1, "tick after shutdown safe");
    }

    /* -- Reconnect cycle ------------------------------------------ */
    printf("\n[Reconnect]\n");
    {
        daemon_panel_init();
        for (int i = 0; i < 310; i++) daemon_panel_tick();
        T(daemon_panel_archd_state() >= 0, "archd valid after reconnect");
        T(daemon_panel_holyd_state() >= 0, "holyd valid after reconnect");
        daemon_panel_shutdown();
    }

    /* -- Multiple cycles ------------------------------------------ */
    printf("\n[Multiple Cycles]\n");
    {
        for (int c = 0; c < 3; c++) {
            T(daemon_panel_init() == 0, "init ok");
            daemon_panel_tick();
            daemon_panel_shutdown();
        }
        T(1, "3 cycles complete");
    }

    /* -- Negative tests ------------------------------------------- */
    printf("\n[Negative]\n");
    {
        daemon_panel_init();
        daemon_panel_init();
        T(1, "double init safe");
        daemon_panel_shutdown();
        daemon_panel_shutdown();
        T(1, "double shutdown safe");
    }

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
