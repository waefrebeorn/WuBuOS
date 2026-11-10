/*
 * wet_applet_test.c -- behaviour gates for the .wet applet bridge.
 *
 * Proves the seam: a .wet script loads through the window path, renders into
 * the content rect, and its handlers fire when the window is clicked.
 */
#include "../apps/wet_applet.h"

#include "../gui/wet/wet.h"
#include "../gui/wet/wet_lua.h"

#include <stdio.h>
#include <string.h>

/* VBE stubs: the framework's backend links against these in a hosted test. */
__attribute__((weak)) void vbe_clear(unsigned c){(void)c;}
__attribute__((weak)) void vbe_fill_rect(int x,int y,int w,int h,unsigned c){(void)x;(void)y;(void)w;(void)h;(void)c;}
__attribute__((weak)) void vbe_rect(int x,int y,int w,int h,unsigned c){(void)x;(void)y;(void)w;(void)h;(void)c;}
__attribute__((weak)) void vbe_hline(int a,int b,int y,unsigned c){(void)a;(void)b;(void)y;(void)c;}
__attribute__((weak)) void vbe_draw_text(int x,int y,const char*s,unsigned c,int sc){(void)x;(void)y;(void)s;(void)c;(void)sc;}
__attribute__((weak)) int  vbe_text_width(const char*s,int sc){(void)s;(void)sc;return 0;}

/* Kernel and shell symbols the WM's interrupt and desktop paths reference.
 * This test never runs an interrupt or paints a wallpaper, so a quiet stub is
 * honest -- the same pattern apps_test uses. */
void wdt_feed(void) {}
void handle_double_fault(void) {}
void handle_page_fault(void) {}
void handle_gpf(void) {}
void handle_invalid_opcode(void) {}
void interrupt_panic_dump(void) {}
void panic_dump_ring(void) {}
void syscall_handler(void) {}
void startmenu_handle_click(int x, int y) { (void)x;(void)y; }
void startmenu_toggle(void) {}
int  startmenu_is_open(void) { return 0; }
void wubu_trash_empty(void) {}
int  wubu_trash_move(const char *p) { (void)p; return 0; }
void wubu_notify_simple(const char *a, const char *t, const char *m,
                        void *i, int w, int d) { (void)a;(void)t;(void)m;(void)i;(void)w;(void)d; }
const char *wubu_compat_cache_dir(void) { return "/tmp"; }
int  wubu_compat_db_get(const char *n) { (void)n; return 0; }
void shutdown(void) {}
int  wubu_session_launch_game(void *h, const uint8_t *g, size_t n, const char *nm) {
    (void)h;(void)g;(void)n;(void)nm; return -1;
}
int  wm_get_hosted_state(void *out) { (void)out; return 0; }
/* The HolyC terminal's REPL evaluator; this test never opens it. */
int hd_eval(const char *src, char *out, int out_cap) {
    (void)src;
    if (out && out_cap > 0) out[0] = '\0';
    return 0;
}
void klog_printf(const char *f, ...) { (void)f; }
void klog_tx_poll(void) {}
unsigned long long wubu_agi_kernel_global(void) { return 0; }
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

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

static void test_demo_loads(void) {
    DosGuiWindow *win = wet_applet_open(NULL, "WET Test", 10, 10, 420, 320);
    CHECK(win != NULL, "demo window created");
    CHECK(wet_applet_ok(), "demo applet loaded");
    CHECK(wet_applet_error() == NULL, "no applet error");
    CHECK(strcmp(win->title, "WET Test") == 0, "title applied");
    CHECK(win->on_draw != NULL, "draw callback bound");
    CHECK(win->on_key != NULL, "key callback bound");
    CHECK(win->on_mouse != NULL, "mouse callback bound");
}

static void test_demo_draws(void) {
    DosGuiWindow *win = wet_applet_open(NULL, "Draw", 0, 0, 420, 320);
    CHECK(win != NULL, "window created");
    if (win) wet_applet_draw(win, NULL, 0, 0);
    CHECK(wet_applet_error() == NULL, "draw raised no applet error");
}

static void test_click_reaches_script(void) {
    DosGuiWindow *win = wet_applet_open(NULL, "Click", 0, 0, 420, 320);
    CHECK(win != NULL, "window created");
    if (!win) return;

    wet_applet_draw(win, NULL, 0, 0);

    /* The applet is static inside the launcher; the accessor reaches it. */
    WetApp *app = wet_applet_app();
    CHECK(app != NULL, "applet reachable");
    if (!app) return;

    WetWidget *cb = wet_app_find(app, "ok");
    CHECK(cb != NULL, "checkbox found by id");
    if (cb) {
        WetRect r = wet_widget_rect(cb);
        int cx = r.x + r.w / 2;
        int cy = r.y + r.h / 2;
        wet_applet_mouse(win, cx, cy, 1, 1);
        wet_applet_mouse(win, cx, cy, 1, 2);
    }
    CHECK(wet_checkbox_checked(cb), "script-driven checkbox toggled");
}

static void test_bad_path_is_reported(void) {
    DosGuiWindow *win = wet_applet_open("/nonexistent/nope.wet", "Bad", 0, 0, 300, 200);
    CHECK(win != NULL, "window still created for a bad path");
    CHECK(wet_applet_error() != NULL, "bad path reported");
}

int main(void) {
    printf("=== wet applet bridge ===\n");
    test_demo_loads();
    test_demo_draws();
    test_click_reaches_script();
    test_bad_path_is_reported();
    printf("=== wet_applet: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
