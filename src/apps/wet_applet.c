/*
 * wet_applet.c -- launch a .wet applet as a WuBuOS window.
 *
 * The framework owns the widget tree, the layout and the event routing; the
 * DosGui window owns the chrome.  This file is the seam between them:
 *
 *   draw   -- chrome first, then the applet into the content rect
 *   key    -- forwarded to the focused widget
 *   mouse  -- forwarded with window-relative coordinates
 *   resize -- the applet re-runs its measure/arrange pass
 *
 * The applet's coordinate space is its own content rect, so a .wet script
 * never learns whether it is fullscreen or in a window.
 */
#include "wet_applet.h"

#include "../gui/wet/wet.h"
#include "../gui/wet/wet_lua.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The built-in demo: proves the whole path works with no file on disk. */
static const char *DEMO =
    "wet.applet {\n"
    "  title = 'WET Demo',\n"
    "  content = wet.stack {\n"
    "    spacing = 8,\n"
    "    children = {\n"
    "      wet.label { text = 'The .wet framework is alive.' },\n"
    "      wet.label { text = 'C lays out, Lua decides.' },\n"
    "      wet.separator {},\n"
    "      wet.checkbox { id = 'ok', text = 'Click me',\n"
    "                     onchange = function(self, ev)\n"
    "                       wet.find('status').text = ev.value and 'checked' or 'unchecked'\n"
    "                     end },\n"
    "      wet.label { id = 'status', text = 'unchecked' },\n"
    "      wet.button { text = 'Greet',\n"
    "                   onclick = function(self)\n"
    "                     wet.messagebox('Hello from Lua', 'WET')\n"
    "                   end },\n"
    "    }\n"
    "  }\n"
    "}\n";

typedef struct {
    WetApp  *app;
    WetLua  *lua;
    char    *src;        /* owned copy of the script */
    char     path[256];
} WetApplet;

static WetApplet g_wa;

/* ---- routing --------------------------------------------------------- */

void wet_applet_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    ChromeContentRect c = dosgui_chrome_draw_window(win, fb, fb_w, fb_h);
    if (!g_wa.app) return;
    /* The applet lives in its content rect: position it, then reflow. */
    wet_app_set_origin(g_wa.app, c.x, c.y);
    wet_app_resize(g_wa.app, c.w, c.h);
    wet_app_render(g_wa.app);
}

void wet_applet_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win;
    if (!g_wa.app || key == 0) return;
    if (key == 8) key = '\b';            /* WuBuOS backspace convention */
    wet_app_key(g_wa.app, WET_EVENT_KEYDOWN, key, mods);
}

void wet_applet_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    (void)win;
    if (!g_wa.app) return;
    WetEventKind ek = (kind == 1) ? WET_EVENT_MOUSEDOWN
                     : (kind == 2) ? WET_EVENT_MOUSEUP
                     :               WET_EVENT_MOUSEMOVE;
    wet_app_pointer(g_wa.app, ek, x, y, btn);
}

static void wet_applet_resize(DosGuiWindow *win, int w, int h) {
    (void)win; (void)w; (void)h;
    /* The next draw reflows from the content rect, so nothing to do here. */
}

/* ---- loading --------------------------------------------------------- */

/* Read a whole file.  Returns NULL on failure; caller frees. */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

static void applet_reset(void) {
    if (g_wa.lua) { wet_lua_free(g_wa.lua); g_wa.lua = NULL; }
    /* The app owns the tree but not the Lua state; free the app after. */
    if (g_wa.app) { wet_app_free(g_wa.app); g_wa.app = NULL; }
    free(g_wa.src);
    g_wa.src = NULL;
    g_wa.path[0] = '\0';
}

/* Build the applet: state, script, tree.  Returns NULL on failure. */
static int applet_load(const char *path, char *err, size_t err_cap) {
    applet_reset();

    g_wa.src = path ? read_file(path) : (char *)strdup(DEMO);
    if (!g_wa.src) {
        char buf[320];
        snprintf(buf, sizeof buf, "wet: cannot read %s", path ? path : "(demo)");
        if (err) snprintf(err, err_cap, "%s", buf);
        wet_app_error(g_wa.app, buf);
        return -1;
    }
    if (path) {
        snprintf(g_wa.path, sizeof g_wa.path, "%s", path);
        g_wa.path[sizeof g_wa.path - 1] = '\0';
    }

    g_wa.app = wet_app_new(400, 300);
    if (!g_wa.app) {
        if (err) snprintf(err, err_cap, "applet alloc failed");
        return -1;
    }
    g_wa.lua = wet_lua_new(g_wa.app, err, err_cap);
    if (!g_wa.lua) return -1;

    const char *e = wet_app_load(g_wa.app, g_wa.src,
                                 path ? path : "demo.wet");
    if (e) {
        char buf[320];
        snprintf(buf, sizeof buf, "%s", e);
        if (err) snprintf(err, err_cap, "%s", buf);
        wet_app_error(g_wa.app, buf);
        return -1;
    }
    return 0;
}

/* ---- public ---------------------------------------------------------- */

DosGuiWindow *wet_applet_open(const char *path, const char *title,
                              int x, int y, int w, int h) {
    char err[256] = {0};
    if (applet_load(path, err, sizeof err) != 0) {
        fprintf(stderr, "wet_applet: %s\n", err);
    }

    DosGuiWindow *win = dosgui_wm_create(x, y, w, h,
                                         title ? title : (path ? path : "WET Applet"));
    if (!win) return NULL;
    win->on_draw   = wet_applet_draw;
    win->on_key    = wet_applet_key;
    win->on_mouse  = wet_applet_mouse;
    win->on_resize = wet_applet_resize;
    win->user_data = &g_wa;
    return win;
}

DosGuiWindow *dosgui_launch_wet_applet(const char *path) {
    return wet_applet_open(path, NULL, 120, 90, 420, 320);
}

/* ---- inspection (for tests) ------------------------------------------ */

int wet_applet_ok(void)              { return g_wa.app && g_wa.lua; }
WetApp *wet_applet_app(void)         { return g_wa.app; }
const char *wet_applet_error(void)   { return wet_app_last_error(g_wa.app); }
