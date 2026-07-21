/*
 * dosgui_apps_test_stubs.c -- no-op link support for dosgui_apps_test.
 *
 * dosgui_apps.c is the app dispatcher: it references many subsystem entry
 * points (explorer, cmd, edr, dos-proc, notify, wallpaper, session, compat,
 * holy-c eval, zlib compress, klog) that the apps test never exercises. To
 * keep the test self-contained without dragging in the full runtime/jit/etc
 * dependency tree, we provide minimal no-op implementations of exactly the
 * symbols the dispatcher references, matching their real headers.
 *
 * C11, minimal includes, self-contained. Return values chosen to be safe
 * defaults (0 / NULL / no-op) so any incidental call is benign.
 */

#include "dosgui_apps.h"
#include "wubu_settings.h"
#include "wubu_notify.h"
#include "wubu_wallpaper.h"
#include "wubu_compat_db.h"
#include <stdint.h>
#include <stddef.h>

/* -- Explorer (file explorer as a desktop-style app) ------------------ */
void app_explorer_init(void) { }
void app_explorer_draw(void *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h;
}
void app_explorer_mouse(void *win, int x, int y, int btn, int kind) {
    (void)win; (void)x; (void)y; (void)btn; (void)kind;
}
void app_explorer_key(void *win, uint32_t key, uint32_t mods) {
    (void)win; (void)key; (void)mods;
}

/* -- Notepad drawing/key dispatch (legacy alternate entry) ----------- */
void dosgui_notepad_draw(void *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h;
}
void dosgui_notepad_key(void *win, uint32_t key, uint32_t mods) {
    (void)win; (void)key; (void)mods;
}

/* -- Start menu toggle (WM UI hook) ---------------------------------- */
void dosgui_startmenu_toggle(void) { }

/* -- Hosted-state accessor (WM-host bridge) -------------------------- */
/* hosted_state_t is defined across host/WM headers; we only need a pointer
 * to return NULL. */
typedef struct hosted_state_opaque_stub { int _dummy; } hosted_state_stub_t;
hosted_state_stub_t *dosgui_wm_get_hosted_state(void) { return NULL; }

/* -- EDR dashboard launch -------------------------------------------- */
DosGuiWindow *edr_dash_launch(void) { return NULL; }

/* -- DOS window spawn -------------------------------------------------- */
DosGuiWindow *dosgui_dos_window_spawn(void *proc, int visible) {
    (void)proc; (void)visible; return NULL;
}

/* -- Command-line prompt ---------------------------------------------- */
typedef struct WubuCmd WubuCmd;
WubuCmd *wubu_cmd_create(int cols, int rows) { (void)cols; (void)rows; return NULL; }
void wubu_cmd_draw(WubuCmd *cmd, void *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)cmd; (void)win; (void)fb; (void)fb_w; (void)fb_h;
}
void wubu_cmd_key(WubuCmd *cmd, int key) { (void)cmd; (void)key; }
void wubu_cmd_spawn_shell(WubuCmd *cmd, const char *shell) { (void)cmd; (void)shell; }

/* -- DOS process launch ---------------------------------------------- */
void *wubu_dos_proc_launch(const char *path, int transparent) {
    (void)path; (void)transparent; return NULL;
}

/* -- Notify ---------------------------------------------------------- */
/* wubu_notify.h declares NotifyUrgency; wubu_notify_simple needs that enum type. */
uint32_t wubu_notify_simple(const char *app_name, const char *summary,
                            const char *body, const char *icon,
                            NotifyUrgency urgency, int timeout) {
    (void)app_name; (void)summary; (void)body; (void)icon;
    (void)urgency; (void)timeout;
    return 0;
}

/* -- Wallpaper ------------------------------------------------------- */
const char *wubu_wallpaper_default_path(void) { return NULL; }
int wubu_wallpaper_load(const char *path, WubuWallpaper *out) {
    (void)path; (void)out; return -1;
}
void wubu_wallpaper_rect(WubuWallpaperMode mode,
                         int img_w, int img_h,
                         int fb_w, int fb_h, int taskbar_h,
                         int *out_x, int *out_y, int *out_w, int *out_h) {
    (void)mode; (void)img_w; (void)img_h; (void)fb_w; (void)fb_h; (void)taskbar_h;
    if (out_x) *out_x = 0;
    if (out_y) *out_y = 0;
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
}

/* -- Compat DB (Bottles/OCI compatibility cache) --------------------- */
int wubu_compat_db_get(const char *title, WubuCompatEntry *out) {
    (void)title; (void)out; return -1;
}
int wubu_compat_cache_dir(const char *title, char *out_path, int path_len) {
    (void)title; (void)out_path; (void)path_len; return -1;
}

/* -- Settings (Control Panel persistence) ---------------------------- */
const WubuSettings *wubu_settings_get(void) { return NULL; }
WubuSettings *wubu_settings_mut(void) { return NULL; }
int wubu_settings_save(void) { return 0; }

/* -- Trash ----------------------------------------------------------- */
int wubu_trash_move(const char *path) { (void)path; return -1; }

/* -- Run program (JIT/gcc bridge) ----------------------------------- */
int wubu_run_program(const char *prog, char *const argv[], int capture) {
    (void)prog; (void)argv; (void)capture; return -1;
}

/* -- Session game launch --------------------------------------------- */
int wubu_session_launch_game(void *state, const char *title,
                             const char *cmd, int argc, char **argv) {
    (void)state; (void)title; (void)cmd; (void)argc; (void)argv;
    return -1;
}

/* -- HolyC eval ------------------------------------------------------ */
int hc_eval(const char *src) { (void)src; return -1; }

/* -- Kernel log ------------------------------------------------------ */
#include <stdarg.h>
int klog_printf(const char *fmt, ...) {
    (void)fmt; return 0;
}

/* -- zlib (compress2 / inflate / inflateInit_ / inflateEnd) ---------- */
/* The canvas PNG/ICO codec uses zlib; provide no-ops that report failure. */
int compress2(unsigned char *dst, unsigned long *dst_len,
              const unsigned char *src, unsigned long src_len, int level) {
    (void)dst; (void)dst_len; (void)src; (void)src_len; (void)level;
    return -1;
}
int inflate(unsigned char *dst, unsigned long *dst_len,
            const unsigned char *src, unsigned long src_len) {
    (void)dst; (void)dst_len; (void)src; (void)src_len; return -1;
}
int inflateEnd(void *strm) { (void)strm; return -1; }
int inflateInit_(void *strm, const char *version, int stream_size) {
    (void)strm; (void)version; (void)stream_size; return -1;
}
