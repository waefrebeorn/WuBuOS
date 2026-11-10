/*
 * apps_weak_test.c -- the registry in a link WITHOUT VWS.
 *
 * launch_vws_designer is declared weak because VWS is optional at link time.
 * In a link that does not include vws_designer.c the symbol resolves to 0, so
 * a registry table holding that address directly crashes the moment the user
 * clicks "VWS Designer".  This gate builds exactly that link and asserts the
 * entry is a real, callable adapter that fails safely instead.
 *
 * Deliberately links apps.c and NOTHING from VWS.
 */
#include "../apps/apps.h"

#include <stdio.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

/* ---- the launcher graph apps.c references (all link-only stubs) -------- */

DosGuiWindow *wm_create(int x, int y, int w, int h, const char *t) {
    (void)x; (void)y; (void)w; (void)h; (void)t; return NULL;
}
void wm_spawn_holyc_term(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
void app_explorer_init(void) {}
void app_canvas_init(void) {}
DosGuiWindow *control_launch(void) { return NULL; }
DosGuiWindow *editor_launch(void) { return NULL; }
DosGuiWindow *calc_launch(void) { return NULL; }
DosGuiWindow *bonzi_launch(void) { return NULL; }
DosGuiWindow *comfy_launch(void) { return NULL; }
DosGuiWindow *tandem_launch(void) { return NULL; }
DosGuiWindow *taskmgr_launch(void) { return NULL; }
DosGuiWindow *notes_launch(void) { return NULL; }
DosGuiWindow *todo_launch(void) { return NULL; }
DosGuiWindow *music_launch(void) { return NULL; }
DosGuiWindow *edr_dash_launch(void) { return NULL; }

DosGuiWindow *notepad_draw(DosGuiWindow *w, uint32_t *f, int fw, int fh) {
    (void)w; (void)f; (void)fw; (void)fh; return NULL;
}

/* ---- the rest of the graph apps.c references --------------------------- */

void vbe_fill_rect(int x, int y, int w, int h, unsigned c) { (void)x;(void)y;(void)w;(void)h;(void)c; }
void vbe_rect(int x, int y, int w, int h, unsigned c)     { (void)x;(void)y;(void)w;(void)h;(void)c; }
void vbe_draw_text(int x, int y, const char *s, unsigned c, int sc) {
    (void)x;(void)y;(void)s;(void)c;(void)sc;
}

void notepad_key(DosGuiWindow *w, uint32_t k, uint32_t m)      { (void)w;(void)k;(void)m; }
void notepad_mouse(DosGuiWindow *w, int x, int y, int b, int k){ (void)w;(void)x;(void)y;(void)b;(void)k; }

void app_explorer_draw(DosGuiWindow *w, uint32_t *f, int fw, int fh) { (void)w;(void)f;(void)fw;(void)fh; }
void app_explorer_key(DosGuiWindow *w, uint32_t k, uint32_t m)      { (void)w;(void)k;(void)m; }
void app_explorer_mouse(DosGuiWindow *w, int x, int y, int b, int k) { (void)w;(void)x;(void)y;(void)b;(void)k; }

void app_canvas_draw(DosGuiWindow *w, uint32_t *f, int fw, int fh) { (void)w;(void)f;(void)fw;(void)fh; }
void app_canvas_key(DosGuiWindow *w, uint32_t k, uint32_t m)      { (void)w;(void)k;(void)m; }
void app_canvas_mouse(DosGuiWindow *w, int x, int y, int b, int k) { (void)w;(void)x;(void)y;(void)b;(void)k; }

struct WubuCmd;
struct WubuCmd *wubu_cmd_create(int cols, int rows) { (void)cols;(void)rows; return NULL; }
void wubu_cmd_spawn_shell(struct WubuCmd *c, const char *shell) { (void)c;(void)shell; }
void wubu_cmd_draw(struct WubuCmd *c, void *win, uint32_t *f, int fw, int fh) {
    (void)c;(void)win;(void)f;(void)fw;(void)fh;
}
void wubu_cmd_key(struct WubuCmd *c, int k) { (void)c;(void)k; }

struct WubuDosProc;
struct WubuDosProc *wubu_dos_proc_launch(const char *path, int fmt) { (void)path;(void)fmt; return NULL; }
DosGuiWindow *dos_window_spawn(struct WubuDosProc *p, int mount) { (void)p;(void)mount; return NULL; }

int main(void) {
    printf("=== apps registry in a VWS-less link ===\n");

    const DosGuiAppDef *d = app_find_by_name("VWS Designer");
    CHECK(d != NULL, "VWS Designer is registered");
    CHECK(d && d->launch != NULL,
          "the entry holds a real launcher, not the undefined weak symbol");

    /* Calling it must fail SAFELY (NULL), not jump to address 0. */
    if (d && d->launch) {
        DosGuiWindow *win = d->launch();
        CHECK(win == NULL, "launching without VWS returns NULL instead of crashing");
    }

    /* The other weak launchers must be guarded too. */
    const DosGuiAppDef *wet = app_find_by_name("WET Applet");
    CHECK(wet && wet->launch != NULL, "WET Applet entry is callable");
    if (wet && wet->launch) CHECK(wet->launch() == NULL, "WET Applet without a host is safe");

    printf("=== apps_weak: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
