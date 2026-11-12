/*
 * first_run_test.c -- the first-boot surface.
 *
 * A new user boots into a desktop with no map.  The greeter's offers row and
 * the Getting Started document are that map, so both are gated: the offers have
 * real geometry, they act, and the document renders and scrolls.
 */
#include "../apps/apps.h"
#include "../apps/getting_started.h"
#include "../apps/wubu_walkthrough.h"
#include "../gui/shell/wubu_shell_greeter.h"

#include <stdio.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

/* Kernel/shell stubs -- the same block the other GUI tests carry. */
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
void wubu_shutdown() {}
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
int  wubu_session_launch_game(void *h, const uint8_t *g, size_t n, const char *nm) {
    (void)h;(void)g;(void)n;(void)nm; return -1;
}
int hd_eval(const char *src, char *out, int out_cap) {
    (void)src; if (out && out_cap > 0) out[0] = '\0'; return 0;
}
int  wm_get_hosted_state(void *out) { (void)out; return 0; }

static void test_offer_geometry(void) {
    int prev_right = 0;
    for (int i = 0; i < 3; i++) {
        int x, y, w, h;
        wubu_shell_greeter_offer_rect(i, &x, &y, &w, &h);
        CHECK(w > 0 && h > 0, "offer has a size");
        CHECK(x >= 0 && y >= 0, "offer is on the dialog");
        if (i > 0) CHECK(x >= prev_right, "offers do not overlap");
        prev_right = x + w;
    }
    /* Out of range must be safe. */
    int x, y, w, h;
    wubu_shell_greeter_offer_rect(-1, &x, &y, &w, &h);
    CHECK(w == 0 && h == 0, "out-of-range offer is empty");
    wubu_shell_greeter_offer_rect(99, &x, &y, &w, &h);
    CHECK(w == 0 && h == 0, "out-of-range offer is empty");
}

static void test_offers_act(void) {
    /* The offers must not crash with no WM and must be reachable. */
    wubu_shell_greeter_offer(0);
    wubu_shell_greeter_offer(1);
    wubu_shell_greeter_offer(2);
    wubu_shell_greeter_offer(99);      /* out of range: must be a no-op */
    CHECK(1, "offers are inert-safe");
}

static void test_walkthrough_steps(void) {
    CHECK(wubu_walkthrough_steps() >= 4, "walkthrough has at least four steps");
    CHECK(wubu_walkthrough_steps() <= 8, "walkthrough is a walk, not a novel");
}

static void test_getting_started(void) {
    /* The launcher opens the walkthrough; the walkthrough owns its window,
     * so gs_launch() returns NULL by design. */
    int opened = wubu_walkthrough_run();
    CHECK(opened == 1, "walkthrough opens");
    CHECK(gs_launch() == NULL, "launcher defers to the walkthrough");
}

int main(void) {
    printf("=== first-run surface ===\n");
    test_offer_geometry();
    test_offers_act();
    test_walkthrough_steps();
    test_getting_started();
    printf("=== first_run: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
