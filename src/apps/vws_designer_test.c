/*
 * vws_designer_test.c -- launch the VWS designer the way the shell does.
 *
 * The claim under test is the circular one: the designer runs on the same
 * WET + WUA stack it designs for, in one interpreter, inside a WuBuOS window.
 * If that holds, "Development -> VWS Designer" is not a slogan.
 */
#include "../apps/vws_designer.h"

#include "../gui/wet/wet.h"
#include "../gui/vws/vws_lua.h"

#include <stdio.h>
#include <string.h>

/* Kernel/shell symbols the WM's interrupt and desktop paths reference.  The
 * designer never runs an interrupt or paints a wallpaper, so a quiet stub is
 * honest -- the same block wet_applet_test.c carries. */
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
void wubu_trash_empty(void) {}
int  wubu_trash_move(const char *p) { (void)p; return 0; }
void wubu_notify_simple(const char *a, const char *t, const char *m,
                        void *i, int w, int d) { (void)a;(void)t;(void)m;(void)i;(void)w;(void)d; }
const char *wubu_compat_cache_dir(void) { return "/tmp"; }
int  wubu_compat_db_get(const char *n) { (void)n; return 0; }
void dosgui_shutdown(void) {}
int  wubu_session_launch_game(void *h, const uint8_t *g, size_t n, const char *nm) {
    (void)h;(void)g;(void)n;(void)nm; return -1;
}
int  dosgui_wm_get_hosted_state(void *out) { (void)out; return 0; }
void dosgui_startmenu_handle_click(int x, int y) { (void)x;(void)y; }
void dosgui_startmenu_toggle(void) {}
int  dosgui_startmenu_is_open(void) { return 0; }
int hd_eval(const char *src, char *out, int out_cap) {
    (void)src; if (out && out_cap > 0) out[0] = '\0'; return 0;
}

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

int main(void) {
    printf("=== vws designer launcher ===\n");

    DosGuiWindow *win = dosgui_launch_vws_designer();
    CHECK(win != NULL, vws_designer_error() ? vws_designer_error()
                                            : "designer window created");
    if (!win) return 1;

    CHECK(win->on_draw != NULL,  "draw bound");
    CHECK(win->on_key  != NULL,  "key bound");
    CHECK(win->on_mouse != NULL, "mouse bound");
    CHECK(strstr(win->title, "VWS") != NULL, "title names the designer");

    /* The designer's own chrome must exist in the applet tree. */
    /* (The launcher does not expose the app, so reach it through the WM.) */
    CHECK(dosgui_wm_window_count() >= 1, "window registered with the WM");

    /* A draw pass must run clean. */
    if (win->on_draw) win->on_draw(win, NULL, 0, 0);
    CHECK(vws_designer_error() == NULL, "draw raised no designer error");

    printf("=== vws_designer_launcher: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
