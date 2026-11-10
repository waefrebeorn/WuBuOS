/*
 * vws_designer.c -- launch the VWS form designer as a WuBuOS window.
 *
 * The designer is a .wet applet written in Lua, so launching it means building
 * the same stack it designs for: a WetApp for the tree, WET's Lua binding for
 * wet.*, and VWS's document binding for vws.* -- both in ONE interpreter.
 *
 * That sharing is the point: the designer and the applet it designs run on the
 * same framework, in the same language, in the same process.
 */
#include "../gui/dosgui_wm.h"
#include "../gui/wet/wet.h"
#include "../gui/wet/wet_lua.h"
#include "../gui/vws/vws_lua.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Where the designer script lives.  The VWS submodule ships it; a deployed
 * install may also carry it under /usr/share. */
static const char *const SCRIPT_PATHS[] = {
    "src/gui/vws/designer.wet",      /* in-tree build (make test) */
    "../wubuos/src/gui/vws/designer.wet",
    "/usr/share/wubuos/designer.wet", /* installed */
    "designer.wet",
    NULL
};

typedef struct {
    WetApp  *app;
    WetLua  *wl;     /* wet.*  */
    VwsLua  *vl;     /* vws.*  */
} VwsDesigner;

static VwsDesigner g_vd;
static char g_err[256];

static char *read_script(void) {
    for (int i = 0; SCRIPT_PATHS[i]; i++) {
        FILE *f = fopen(SCRIPT_PATHS[i], "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n <= 0) { fclose(f); continue; }
        char *buf = (char *)malloc((size_t)n + 1);
        if (!buf) { fclose(f); continue; }
        size_t got = fread(buf, 1, (size_t)n, f);
        fclose(f);
        buf[got] = '\0';
        return buf;
    }
    return NULL;
}

/* ---- routing (the same seam as wet_applet.c) ---------------------- */

static void designer_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    ChromeContentRect c = dosgui_chrome_draw_window(win, fb, fb_w, fb_h);
    if (!g_vd.app) return;
    wet_app_set_origin(g_vd.app, c.x, c.y);
    wet_app_resize(g_vd.app, c.w, c.h);
    wet_app_render(g_vd.app);
}

static void designer_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win;
    if (!g_vd.app || key == 0) return;
    if (key == 8) key = '\b';
    wet_app_key(g_vd.app, WET_EVENT_KEYDOWN, key, mods);
}

static void designer_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    (void)win;
    if (!g_vd.app) return;
    WetEventKind ek = (kind == 1) ? WET_EVENT_MOUSEDOWN
                     : (kind == 2) ? WET_EVENT_MOUSEUP
                     :               WET_EVENT_MOUSEMOVE;
    wet_app_pointer(g_vd.app, ek, x, y, btn);
}

/* ---- public ----------------------------------------------------- */

static void designer_reset(void) {
    /* One interpreter, one document: free both faces, then the app. */
    if (g_vd.vl) { vws_lua_free(g_vd.vl); g_vd.vl = NULL; }
    if (g_vd.wl) { wet_lua_free(g_vd.wl);  g_vd.wl  = NULL; }
    if (g_vd.app) { wet_app_free(g_vd.app); g_vd.app = NULL; }
    g_err[0] = '\0';
}

DosGuiWindow *dosgui_launch_vws_designer(void) {
    designer_reset();

    char *src = read_script();
    if (!src) {
        snprintf(g_err, sizeof g_err, "designer.wet not found");
        return NULL;
    }

    g_vd.app = wet_app_new(900, 620);
    if (!g_vd.app) { free(src); snprintf(g_err, sizeof g_err, "app alloc failed"); return NULL; }

    g_vd.wl = wet_lua_new(g_vd.app, g_err, sizeof g_err);
    if (!g_vd.wl) { free(src); return NULL; }

    /* The VWS document shares the interpreter the .WET applet runs in. */
    g_vd.vl = vws_lua_open(wet_lua_state(g_vd.wl), "Form1", 400, 300,
                           g_err, sizeof g_err);
    if (!g_vd.vl) { free(src); return NULL; }

    const char *e = wet_lua_run(g_vd.wl, src, "designer.wet");
    free(src);
    if (e) {
        snprintf(g_err, sizeof g_err, "%s", e);
        return NULL;
    }

    DosGuiWindow *win = dosgui_wm_create(60, 60, 900, 620,
                                         "VWS Designer");
    if (!win) return NULL;
    win->on_draw  = designer_draw;
    win->on_key   = designer_key;
    win->on_mouse = designer_mouse;
    return win;
}

const char *vws_designer_error(void) { return g_err[0] ? g_err : NULL; }
