/*
 * tutorial_test.c -- the tutorial host, actually running the shipped scripts.
 *
 * A tutorial that does not run is a wiki, so this gates the real thing: the
 * browser and every shipped tutorial must execute to completion against the
 * live WET stack.
 */
#include "wubu_tutorial.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

/* Kernel/shell stubs.  Types must match wm.h exactly -- the real header is
 * included through wubu_tutorial.h, so a mismatch is a compile error. */
void wdt_feed(void) {}
void handle_double_fault(void) {}
void handle_page_fault(void) {}
void handle_gpf(void) {}
void handle_invalid_opcode(void) {}
void interrupt_panic_dump(void) {}
void panic_dump_ring(void) {}
void syscall_handler(void) {}
void klog_printf(const char *f, ...) { (void)f; }
void klog_tx_poll(void) {}
void startmenu_handle_click(int x, int y) { (void)x;(void)y; }
void startmenu_toggle(void) {}
int  startmenu_is_open(void) { return 0; }
void shutdown_wm(void) {}
int  wm_get_hosted_state(void *out) { (void)out; return 0; }
void wm_taskbar_update_clock(time_t now) { (void)now; }
void wm_taskbar_render(uint32_t *fb, int w, int h) { (void)fb;(void)w;(void)h; }
void *wubu_agi_kernel_global(void) { return NULL; }
unsigned long long wubu_agi_kernel_promoted_total(void) { return 0; }
unsigned long long wubu_agi_kernel_tick(void) { return 0; }
unsigned long long wubu_agi_kernel_uptime_ms(void) { return 0; }
void wubu_vdso_update(void) {}
void wubu_bonzi_init(void) {}
void wubu_bonzi_draw(uint32_t *f, int fw, int fh, int x, int y) { (void)f;(void)fw;(void)fh;(void)x;(void)y; }
int  wubu_bonzi_is_enabled(void) { return 0; }
void wubu_bonzi_mouse(int x, int y) { (void)x;(void)y; }
void wubu_bonzi_set_bubble(const char *b) { (void)b; }
void wubu_bonzi_tick(unsigned long long ms) { (void)ms; }
void wubu_settings_load(void) {}
void wubu_settings_save(void) {}
const char *wubu_wallpaper_default_path(void) { return ""; }
int  wubu_wallpaper_load(const char *p) { (void)p; return 0; }
void wubu_wallpaper_rect(int *x, int *y, int *w, int *h) { (void)x;(void)y;(void)w;(void)h; }
void wubu_wallpaper_render(uint32_t *f, int fw, int fh) { (void)f;(void)fw;(void)fh; }
void wubu_trash_empty(void) {}
int  wubu_trash_move(const char *p) { (void)p; return 0; }
void wubu_notify_simple(const char *a, const char *t, const char *m, void *i, int w, int d) { (void)a;(void)t;(void)m;(void)i;(void)w;(void)d; }
const char *wubu_compat_cache_dir(void) { return "/tmp"; }
int  wubu_compat_db_get(const char *n) { (void)n; return 0; }
int  wubu_session_launch_game(void *h, const uint8_t *g, size_t n, const char *nm) { (void)h;(void)g;(void)n;(void)nm; return -1; }
int hd_eval(const char *src, char *out, int cap) { (void)src; if(out&&cap>0) out[0]=0; return 0; }
void settings_apply_live(void) {}
void a11y_init(void) {}
void a11y_announce(const char *s) { (void)s; }

static void test_catalog(void) {
    CHECK(wubu_tutorial_count() == 4, "four tutorials shipped");
    const char *want[] = { "first_gui", "design_a_window", "compile_c", "pin_an_app" };
    for (int i = 0; i < 4; i++)
        CHECK(strcmp(wubu_tutorial_name(i), want[i]) == 0, "tutorial name matches");
    CHECK(wubu_tutorial_name(-1) == NULL, "out of range name is NULL");
    CHECK(wubu_tutorial_name(99) == NULL, "out of range name is NULL");
}

static void test_unknown(void) {
    CHECK(wubu_tutorial_open("../etc/passwd") == NULL, "path traversal rejected");
    CHECK(wubu_tutorial_open("") == NULL, "empty name rejected");
    CHECK(wubu_tutorial_error() != NULL, "error reported");
}

static void test_browser_runs(void) {
    DosGuiWindow *w = wubu_tutorial_browser();
    CHECK(w != NULL, "browser window created");
    if (!w) return;
    CHECK(w->on_draw != NULL, "browser draws");
}

static void test_each_tutorial_runs(void) {
    for (int i = 0; i < wubu_tutorial_count(); i++) {
        const char *name = wubu_tutorial_name(i);
        DosGuiWindow *w = wubu_tutorial_open(name);
        if (!w) {
            g_fails++; g_checks++;
            printf("  FAIL tutorial '%s' did not run: %s\n", name,
                   wubu_tutorial_error() ? wubu_tutorial_error() : "?");
            continue;
        }
        CHECK(w->on_draw != NULL, "tutorial draws");
        CHECK(w->on_key  != NULL, "tutorial handles keys");
        CHECK(w->on_mouse != NULL, "tutorial handles the mouse");
        /* Steps must be navigable. */
        if (w->on_key) w->on_key(w, 8, 0);    /* backspace: must not crash */
        if (w->on_key) w->on_key(w, 27, 0);   /* escape */
    }
}

int main(void) {
    printf("=== tutorials ===\n");
    test_catalog();
    test_unknown();
    test_browser_runs();
    test_each_tutorial_runs();
    printf("=== tutorial: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}

/* Weak: the WM routes keys to the Start menu while it is open.  Targets that
 * link the real startmenu.c get the strong definitions. */
__attribute__((weak)) void startmenu_handle_key(int key, uint32_t mods) {
    (void)key; (void)mods;
}
__attribute__((weak)) void startmenu_handle_search_input(int key, uint32_t mods) {
    (void)key; (void)mods;
}
