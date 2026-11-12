/*
 * wm_test_stub.c  --  Stubs for WM unit tests without full desktop
 * integration.
 *
 * Provides minimal implementations of start-menu / launch / shutdown hooks
 * that the WM and context-menu engine call.  Most are no-ops for unit tests.
 * The start-menu functions provide a MINIMAL testable implementation (not
 * the full programs-db + MIME registry) so that desktop integration tests
 * in wubu_desktop_shot.c can verify the Companion toggle end-to-end.
 *
 * IMPORTANT: wm_handle_mouse is deliberately NOT defined here. The real
 * implementation in wm_input.c must bind, so that input (and the wubu_ui
 * automation layer) drives the genuine window manager rather than a no-op.
 */

#include "wm.h"
#include "shell/startmenu.h"
#include "wubu_theme.h"
#include "wubu_bonzi.h"
#include "../hosted/hosted.h"
#include "../kernel/vbe.h"
#include <stdbool.h>
#include <string.h>

/* ---- Minimal no-op stubs for symbols the start-menu test path needs but
 *      the WM/bonzi/vbe real modules are not linked into test_holyd.
 *
 *      These mirror startmenu_test_stub.c's approach: provide the
 *      handful of WM geometry + bonzi + VBE entry points the menu click
 *      handler touches, so the format layer proves the dispatch machinery
 *      without dragging in the entire GUI surface.
 *
 *      Sibling binaries (desktop_shot, a11y_shot, test_wm ...) link
 *      the REAL definitions AND this stub; they pass -Wl,--allow-multiple-
 *      definition, so the linker keeps the real TU. When only this stub is
 *      linked, these definitions satisfy the references. ---- */
#if defined(WUBD_TEST_STUB_WM_NOOPS)
int  taskbar_height(void) { return 28; }
int  wm_screen_h(void)     { return 768; }
int  wm_screen_w(void)     { return 1024; }
void wm_set_focus(DosGuiWindow *w) { (void)w; }
DosGuiWindow *wm_spawn_holyc_term(int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h; return NULL;
}
void vbe_draw_text(int x, int y, const char *s, uint32_t c, int scale) {
    (void)x; (void)y; (void)s; (void)c; (void)scale;
}
#endif /* WUBD_TEST_STUB_WM_NOOPS */

/* --- Start menu: minimal testable implementation --- */

/* Menu geometry (must match startmenu.c constants). */
#define TSM_MENU_X    4
#define TSM_MENU_W    200
#define TSM_SIDEBAR_W 48
#define TSM_ITEM_H    24

static int tsm_open = 0;
/* Minimal main-menu items for testing: the real app catalog is not linked. */
typedef struct { const char *label; int type; } tsm_item_t;
static tsm_item_t tsm_items[] = {
    { "Programs",  1 },  /* submenu */
    { "Documents", 0 },
    { "Settings",  0 },
    { "",          2 },  /* separator */
    { "Find",      0 },
    { "Help",      0 },
    { "Run...",    0 },
    { "",          2 },  /* separator */
    { "Shut Down", 3 },
    { "WuBu Buddy", 4 },  /* toggle */
};
#define TSM_N_ITEMS (int)(sizeof(tsm_items)/sizeof(tsm_items[0]))

/* Weak: binaries that link the REAL startmenu.c (desktop_shot,
 * a11y_shot, gui_shot, test_wm ...) get the genuine menu regardless
 * of link order.  Targets that link ONLY this stub keep the minimal
 * implementation.  (Previously these were strong, so a recipe that forgot
 * the real TU silently measured the stub — the desktop_shot Start button
 * toggled a dead variable and the captured "menu open" frame was empty.) */
__attribute__((weak)) void startmenu_toggle(void) { tsm_open = !tsm_open; }
__attribute__((weak)) void startmenu_open(void)   { tsm_open = 1; }
__attribute__((weak)) void startmenu_close(void)  { tsm_open = 0; }
__attribute__((weak)) int  startmenu_is_open(void) { return tsm_open; }

__attribute__((weak)) void startmenu_handle_click(int x, int y) {
    if (!tsm_open) return;
    int task_h = taskbar_height();
    int mh = TSM_ITEM_H;
    int mw = TSM_MENU_W;
    int menu_y = wm_screen_h() - task_h - (TSM_N_ITEMS * mh + 4);
    /* Bounds check: click outside the menu closes it. */
    if (x < TSM_MENU_X || x >= TSM_MENU_X + mw ||
        y < menu_y || y >= menu_y + TSM_N_ITEMS * mh + 4) {
        tsm_open = 0;
        return;
    }
    int idx = (y - menu_y - 2) / mh;
    if (idx < 0 || idx >= TSM_N_ITEMS) return;
    if (tsm_items[idx].type == 4) {
        /* Companion toggle — bonzi lives in its own module + wubu_bonzi.h */
        wubu_bonzi_set_enabled(!wubu_bonzi_is_enabled());
        tsm_open = 0;
    } else if (tsm_items[idx].type == 3) {
        /* Shutdown */
        extern void wubu_shutdown();
        wubu_shutdown();
        tsm_open = 0;
    }
    /* Other item types: close menu (apps are stubbed). */
    if (tsm_items[idx].type == 0 || tsm_items[idx].type == 1)
        tsm_open = 0;
}

__attribute__((weak)) void startmenu_track_hover(int x, int y) { (void)x; (void)y; }

/* Run dialog: the WM routes Win+R to it.  Weak, so targets that link only
 * this stub still resolve the symbols; the real module wins where linked. */
__attribute__((weak)) void run_dialog_open(void) { }
__attribute__((weak)) void run_dialog_close(void) { }
__attribute__((weak)) bool run_dialog_is_open(void) { return false; }
__attribute__((weak)) void run_dialog_handle_key(uint32_t key, uint32_t mods)
{
    (void)key; (void)mods;
}
__attribute__((weak)) bool run_dialog_handle_click(int x, int y)
{
    (void)x; (void)y; return false;
}
__attribute__((weak)) void run_dialog_rect(int fb_w, int fb_h,
                                           int *x, int *y, int *w, int *h)
{
    (void)fb_w; (void)fb_h;
    if (x) *x = 0; if (y) *y = 0; if (w) *w = 0; if (h) *h = 0;
}
__attribute__((weak)) void run_dialog_render(uint32_t *fb, int fb_w, int fb_h)
{
    (void)fb; (void)fb_w; (void)fb_h;
}

/* Keyboard entry points: the WM routes keys to the Start menu while it is open.
 * Weak, so a target that links only this stub still resolves them. */
__attribute__((weak)) void startmenu_handle_key(int key, uint32_t mods) {
    (void)key; (void)mods;
}
__attribute__((weak)) void startmenu_handle_search_input(int key, uint32_t mods) {
    (void)key; (void)mods;
}

__attribute__((weak)) void startmenu_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!tsm_open || !fb) return;
    int task_h = taskbar_height();
    int mh = TSM_ITEM_H;
    int mw = TSM_MENU_W;
    int menu_y = fb_h - task_h - (TSM_N_ITEMS * mh + 4);
    int main_h = TSM_N_ITEMS * mh + 4;

    /* Menu background (Win98 silver). */
    uint32_t bg = 0x00C0C0C0;
    uint32_t border = 0x00808080;
    uint32_t text = 0x00000000;
    uint32_t sel_bg = 0x00000080;
    uint32_t sel_text = 0x00FFFFFF;

    /* Fill background */
    for (int yy = 0; yy < main_h; yy++)
        for (int xx = 0; xx < mw; xx++)
            fb[(menu_y + yy) * fb_w + (TSM_MENU_X + xx)] = bg;
    /* Border */
    for (int xx = 0; xx < mw; xx++) {
        fb[menu_y * fb_w + TSM_MENU_X + xx] = border;
        fb[(menu_y + main_h - 1) * fb_w + TSM_MENU_X + xx] = border;
    }
    for (int yy = 0; yy < main_h; yy++) {
        fb[(menu_y + yy) * fb_w + TSM_MENU_X] = border;
        fb[(menu_y + yy) * fb_w + TSM_MENU_X + mw - 1] = border;
    }

    /* Draw items */
    int content_x = TSM_MENU_X + TSM_SIDEBAR_W + 4;
    int content_w = mw - TSM_SIDEBAR_W - 8;
    for (int i = 0; i < TSM_N_ITEMS; i++) {
        int iy = menu_y + 2 + i * mh;
        if (tsm_items[i].type == 2) {
            /* Separator: horizontal line */
            int sep_y = iy + mh / 2;
            for (int xx = TSM_MENU_X + 4; xx < TSM_MENU_X + mw - 4; xx++)
                fb[sep_y * fb_w + xx] = border;
            continue;
        }
        /* Item background */
        uint32_t ibg = bg, ifg = text;
        /* Simulate: no hover in test render */
        for (int yy = 0; yy < mh - 1; yy++)
            for (int xx = 0; xx < content_w; xx++)
                fb[(iy + yy) * fb_w + content_x + xx] = ibg;
        /* Draw label (8x8 font) */
        vbe_draw_text(content_x + 4, iy + (mh - 8) / 2,
                      tsm_items[i].label, ifg, 1);
    }
}

/* Stub for launch app (weak: the real registry in apps.c wins when
 * a target links it, so a menu click actually opens an app). */
__attribute__((weak)) void launch_app(const char *name) { (void)name; }

/* ---- Era-apps exec backends (weak) ---------------------------------------
 * era_apps.c is a pure registry: it only calls these when the user
 * LAUNCHES an era app.  Render harnesses (desktop_shot) never launch, so a
 * weak no-op satisfies the link; any target that links the real runtime
 * (test_era_apps, gui_shot, the desktop) gets the strong definitions. */
#include "wubu_exec.h"
#include "wubu_dos_proc.h"
__attribute__((weak)) WubuDosProc *wubu_dos_proc_launch(const char *dos_path, int fmt) {
    (void)dos_path; (void)fmt; return NULL;
}
__attribute__((weak)) int64_t wubu_exec_win_pe(const void *pe_data, size_t pe_size) {
    (void)pe_data; (void)pe_size; return -1;
}
__attribute__((weak)) int64_t wubu_exec_linux_elf(const void *elf_data, size_t elf_size) {
    (void)elf_data; (void)elf_size; return -1;
}
__attribute__((weak)) int64_t wubu_exec_holyc(const char *source, size_t source_size) {
    (void)source; (void)source_size; return -1;
}
__attribute__((weak)) int64_t wubu_exec_macho(const void *macho, size_t macho_size) {
    (void)macho; (void)macho_size; return -1;
}

/* Stub for shutdown */
__attribute__((weak)) void wubu_shutdown() { }
__attribute__((weak)) void platform_shutdown(void) { }

/* Hosted-state getter (Play action): no hosted binary in the test harness,
 * so return NULL -- ctx_action_play treats NULL as "no launch". */
hosted_state_t *wm_get_hosted_state(void) {
    return NULL;
}

/* Weak bonzi stubs: test binaries that do NOT link wubu_bonzi.c (test_control,
 * desktop_shot, a11y_shot) still resolve the start-menu Companion toggle and
 * the WM render/input calls to the mascot. Targets that DO link wubu_bonzi.c
 * get the real strong definitions (linker drops the weak copies). */
__attribute__((weak))
void wubu_bonzi_set_enabled(bool on) { (void)on; }
__attribute__((weak))
bool wubu_bonzi_is_enabled(void)    { return false; }
__attribute__((weak))
bool wubu_bonzi_init(int x, int y)  { (void)x; (void)y; return false; }
__attribute__((weak))
int  wubu_bonzi_x(void)             { return 0; }
__attribute__((weak))
int  wubu_bonzi_y(void)             { return 0; }
__attribute__((weak))
int  wubu_bonzi_w(void)             { return 0; }
__attribute__((weak))
int  wubu_bonzi_h(void)             { return 0; }
__attribute__((weak))
void wubu_bonzi_tick(int dt_ms)    { (void)dt_ms; }
__attribute__((weak))
void wubu_bonzi_draw(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
}
__attribute__((weak))
bool wubu_bonzi_mouse(int x, int y, int btn, int kind) {
    (void)x; (void)y; (void)btn; (void)kind; return false;
}
__attribute__((weak))
void wubu_bonzi_open_agi(void) { }
__attribute__((weak))
void wubu_bonzi_set_bubble(const char *l1, const char *l2) {
    (void)l1; (void)l2;
}

/* Weak: the WM routes Win+X to the power-user menu.  Targets that link the
 * real power_menu.c get the strong definitions. */
__attribute__((weak)) void power_menu_open(void) { }
__attribute__((weak)) void power_menu_close(void) { }
__attribute__((weak)) bool power_menu_is_open(void) { return false; }
__attribute__((weak)) bool power_menu_handle_click(int x, int y) {
    (void)x; (void)y; return false;
}
__attribute__((weak)) void power_menu_handle_key(uint32_t key, uint32_t mods) {
    (void)key; (void)mods;
}
__attribute__((weak)) void power_menu_rect(int fb_w, int fb_h,
                                           int *x, int *y, int *w, int *h) {
    (void)fb_w; (void)fb_h;
    if (x) *x = 0; if (y) *y = 0; if (w) *w = 0; if (h) *h = 0;
}
__attribute__((weak)) void power_menu_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
}
