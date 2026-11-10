/*
 * wubu_tutorial.c -- the tutorial host.
 *
 * A tutorial is a .wet applet written in Lua, so a tutorial about the GUI is
 * itself a GUI built from the same framework -- and the "Try it" buttons launch
 * the real tools, not screenshots of them.
 *
 * The host contributes tut.*:
 *   tut.count()          number of steps
 *   tut.step(i)          jump to a step (1-based)
 *   tut.next() / tut.back()
 *   tut.action(name)     launch a real app ("terminal", "designer", "editor"...)
 *   tut.open(name)       open another tutorial by name
 *   tut.done()           close, having completed
 */
#include "wubu_tutorial.h"

#include "../gui/wet/wet.h"
#include "../gui/wet/wet_lua.h"
#include "../gui/wet/externals/wua/src/wb_lv.h"
#include "../apps/apps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Cross-module launchers.  Weak, so a test or minimal build that does not link
 * the app registry still links; the "Try it" button is simply inert there. */
__attribute__((weak)) DosGuiWindow *launch_terminal(void);
__attribute__((weak)) DosGuiWindow *launch_vws_designer(void);
__attribute__((weak)) DosGuiWindow *launch_editor(void);
__attribute__((weak)) DosGuiWindow *launch_getting_started(void);
__attribute__((weak)) void startmenu_toggle(void);

/* The shipped tutorials.  A tutorial is a .wet file in this directory. */
static const char *const TUTORIALS[] = {
    "first_gui",       /* write and run a .wet applet */
    "design_a_window", /* VWS form designer */
    "compile_c",       /* HolyD, the in-tree compiler */
    "pin_an_app",      /* the taskbar pin model */
    NULL
};

int wubu_tutorial_count(void) {
    int n = 0;
    while (TUTORIALS[n]) n++;
    return n;
}

const char *wubu_tutorial_name(int i) {
    return (i >= 0 && i < wubu_tutorial_count()) ? TUTORIALS[i] : NULL;
}

static char g_err[256];
const char *wubu_tutorial_error(void) { return g_err[0] ? g_err : NULL; }

/* Where the .wet tutorial scripts live. */
static void script_path(const char *name, char *out, size_t n) {
    snprintf(out, n, "src/apps/tutorials/%s.wet", name);
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

/* ---- tut.* ------------------------------------------------------- */

typedef struct {
    WetApp *app;
    WetLua *wl;
    int     step;
    int     count;
} TutCtx;

static TutCtx g_tut;

static wb_lv_value tut_action(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return wb_lv_nil();
    const char *what = wb_lv_tostring(a[0]);
    if (!what) return wb_lv_nil();

    /* Launch the REAL tool.  A tutorial that says "open the designer" opens the
     * designer -- the user is looking at the actual thing before the step ends.
     */
    if (strcmp(what, "terminal") == 0)      launch_terminal();
    else if (strcmp(what, "designer") == 0) launch_vws_designer();
    else if (strcmp(what, "editor") == 0)   launch_editor();
    else if (strcmp(what, "getting_started") == 0) launch_getting_started();
    else if (strcmp(what, "startmenu") == 0) startmenu_toggle();
    return wb_lv_nil();
}

static wb_lv_value tut_open(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return wb_lv_bool(0);
    const char *name = wb_lv_tostring(a[0]);
    DosGuiWindow *w = wubu_tutorial_open(name);
    return wb_lv_bool(w != NULL);
}

static wb_lv_value tut_count(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)a; (void)n;
    return wb_lv_integer(g_tut.count);
}

static wb_lv_value tut_step(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return wb_lv_bool(0);
    int ok = 0;
    int v = (int)wb_lv_tointeger(a[0], &ok);
    if (!ok) return wb_lv_bool(0);
    if (v < 1 || v > g_tut.count) return wb_lv_bool(0);
    g_tut.step = v;
    return wb_lv_bool(1);
}

static wb_lv_value tut_next(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)a; (void)n; (void)ud;
    if (g_tut.step < g_tut.count) g_tut.step++;
    return wb_lv_integer(g_tut.step);
}

static wb_lv_value tut_back(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)a; (void)n; (void)ud;
    if (g_tut.step > 1) g_tut.step--;
    return wb_lv_integer(g_tut.step);
}

static void register_tut(wb_lv *L) {
    static const struct { const char *name; wb_lv_cfn fn; } fns[] = {
        { "action", tut_action },
        { "open",   tut_open },
        { "count",  tut_count },
        { "step",   tut_step },
        { "next",   tut_next },
        { "back",   tut_back },
        { NULL, NULL }
    };
    /* `tut` must be a REAL table, not a global named "tut.action". */
    wb_lv_table *tut = wb_lv_table_new(L, 0, 8);
    if (!tut) return;
    for (int i = 0; fns[i].name; i++) {
        wb_lv_value v; v.t = WB_LV_CFUNCTION; v.as.cf.f = fns[i].fn; v.as.cf.ud = NULL;
        wb_lv_table_set(L, tut,
                        wb_lv_str(wb_lv_intern(L, fns[i].name, strlen(fns[i].name))), v);
    }
    wb_lv_value tv; tv.t = WB_LV_TABLE; tv.as.tab = tut;
    wb_lv_set_global(L, "tut", tv);
    for (int i = 0; fns[i].name; i++)
        wb_lv_register_global(L, fns[i].name, fns[i].fn, NULL);
}

/* ---- lifecycle --------------------------------------------------- */

static void tutorial_reset(void) {
    if (g_tut.wl) { wet_lua_free(g_tut.wl); g_tut.wl = NULL; }
    if (g_tut.app) { wet_app_free(g_tut.app); g_tut.app = NULL; }
    g_tut.step = 1;
    g_tut.count = 1;
    g_err[0] = '\0';
}

static void tut_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    ChromeContentRect c = chrome_draw_window(win, fb, fb_w, fb_h);
    if (!g_tut.app) return;
    wet_app_set_origin(g_tut.app, c.x, c.y);
    wet_app_resize(g_tut.app, c.w, c.h);
    wet_app_render(g_tut.app);
}

static void tut_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win;
    if (!g_tut.app || key == 0) return;
    if (key == 8) key = '\b';
    wet_app_key(g_tut.app, WET_EVENT_KEYDOWN, key, mods);
}

static void tut_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    (void)win;
    if (!g_tut.app) return;
    WetEventKind ek = (kind == 1) ? WET_EVENT_MOUSEDOWN
                     : (kind == 2) ? WET_EVENT_MOUSEUP
                     :               WET_EVENT_MOUSEMOVE;
    wet_app_pointer(g_tut.app, ek, x, y, btn);
}

static DosGuiWindow *run_tutorial(const char *name, const char *title) {
    tutorial_reset();

    char path[320];
    script_path(name, path, sizeof path);
    char *src = read_file(path);
    if (!src) {
        snprintf(g_err, sizeof g_err, "tutorial '%s' not found at %s", name, path);
        return NULL;
    }

    g_tut.app = wet_app_new(640, 460);
    if (!g_tut.app) { free(src); snprintf(g_err, sizeof g_err, "app alloc failed"); return NULL; }
    g_tut.wl = wet_lua_new(g_tut.app, g_err, sizeof g_err);
    if (!g_tut.wl) { free(src); return NULL; }
    register_tut(wet_lua_state(g_tut.wl));

    const char *e = wet_lua_run(g_tut.wl, src, path);
    free(src);
    if (e) { snprintf(g_err, sizeof g_err, "%s", e); return NULL; }

    /* The tutorial declares its own step count; the host reads it back. */
    wb_lv_value sc = wb_lv_get_global(wet_lua_state(g_tut.wl), "tut_step_count");
    int ok = 0;
    int n = (int)wb_lv_tointeger(sc, &ok);
    if (ok && n > 0) g_tut.count = n;

    DosGuiWindow *win = wm_create(120, 90, 640, 460, title);
    if (!win) return NULL;
    win->on_draw  = tut_draw;
    win->on_key   = tut_key;
    win->on_mouse = tut_mouse;
    return win;
}

DosGuiWindow *wubu_tutorial_open(const char *name) {
    if (!name || !*name) return NULL;
    /* Only shipped names, so a path cannot escape the tutorials directory. */
    bool known = false;
    for (int i = 0; TUTORIALS[i]; i++)
        if (strcmp(TUTORIALS[i], name) == 0) { known = true; break; }
    if (!known) { snprintf(g_err, sizeof g_err, "unknown tutorial '%s'", name); return NULL; }
    return run_tutorial(name, name);
}

DosGuiWindow *wubu_tutorial_browser(void) {
    return run_tutorial("index", "Tutorials - WuBuOS");
}
