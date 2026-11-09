/*
 * wubu_lua_gui.c -- the C11 host for Lua-driven GUI.
 *
 * See wubu_lua_gui.h.  The interpreter is slerm-Lua (src/lua/www/~WuBu~/src/lv),
 * a C11 tree-walker with a deliberately small API: register_global, call,
 * table get/set.  That is enough for a GUI binding and keeps the boundary
 * narrow -- there is no metatable or coroutine surface for a script to abuse.
 *
 * Every binding draws through the VBE layer, so a Lua GUI is composited by the
 * same Win98 shell as a C one.  A script cannot reach a driver handle, a raw
 * framebuffer pointer or a kernel structure; it can only call gui.* and fs.*.
 */

#include "wubu_lua_gui.h"

#include "wb_lv.h"
#include "../kernel/vbe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The slerm-Lua sources live in the WWW submodule. */
#define WB_LV_DIR   "../../../lua/www/~WuBu~/src/lv"

/* Wrap a C function as a Lua value. */
static wb_lv_value cfn_value(wb_lv_cfn fn, void *ud) {
    wb_lv_value v = wb_lv_nil();
    v.t = WB_LV_CFUNCTION;
    v.as.cf.f = fn;
    v.as.cf.ud = ud;
    return v;
}

/* Wrap a table pointer as a Lua value. */
static wb_lv_value table_value(wb_lv_table *t) {
    wb_lv_value v = wb_lv_nil();
    v.t = WB_LV_TABLE;
    v.as.tab = t;
    return v;
}

struct WubuLuaGui {
    wb_lv   *L;
    int      fb_w, fb_h;
    char     err[512];
    bool     has_draw;
};

/* ---- helpers ------------------------------------------------------- */

static void set_err(WubuLuaGui *g, const char *msg) {
    if (!g || !msg) return;
    snprintf(g->err, sizeof(g->err), "%s", msg);
}

static int num_arg(const wb_lv_value *v, int *ok) {
    return (int)wb_lv_tonumber(*v, ok);
}

static const char *str_arg(const wb_lv_value *v) {
    return wb_lv_tostring(*v);
}

/* ---- gui.* bindings ------------------------------------------------ */

static wb_lv_value gui_text(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    int ok = 0;
    if (n < 4) return wb_lv_bool(0);
    int x = num_arg(&a[0], &ok); if (!ok) return wb_lv_bool(0);
    int y = num_arg(&a[1], &ok); if (!ok) return wb_lv_bool(0);
    const char *s = str_arg(&a[2]);
    int c = num_arg(&a[3], &ok);
    if (!ok) c = 0xFFFFFF;
    if (s) vbe_draw_text(x, y, s, (uint32_t)c, 1);
    return wb_lv_bool(1);
}

static wb_lv_value gui_rect(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    int ok = 0;
    if (n < 5) return wb_lv_bool(0);
    int x = num_arg(&a[0], &ok); if (!ok) return wb_lv_bool(0);
    int y = num_arg(&a[1], &ok); if (!ok) return wb_lv_bool(0);
    int w = num_arg(&a[2], &ok); if (!ok) return wb_lv_bool(0);
    int h = num_arg(&a[3], &ok); if (!ok) return wb_lv_bool(0);
    int c = num_arg(&a[4], &ok);
    if (!ok) c = 0xFFFFFF;
    vbe_rect(x, y, w, h, (uint32_t)c);
    return wb_lv_bool(1);
}

static wb_lv_value gui_fill(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    int ok = 0;
    if (n < 5) return wb_lv_bool(0);
    int x = num_arg(&a[0], &ok); if (!ok) return wb_lv_bool(0);
    int y = num_arg(&a[1], &ok); if (!ok) return wb_lv_bool(0);
    int w = num_arg(&a[2], &ok); if (!ok) return wb_lv_bool(0);
    int h = num_arg(&a[3], &ok); if (!ok) return wb_lv_bool(0);
    int c = num_arg(&a[4], &ok);
    if (!ok) c = 0xFFFFFF;
    vbe_fill_rect(x, y, w, h, (uint32_t)c);
    return wb_lv_bool(1);
}

static wb_lv_value gui_clear(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    int ok = 0;
    int c = (n >= 1) ? num_arg(&a[0], &ok) : 0;
    if (!ok) c = 0;
    vbe_clear((uint32_t)c);
    return wb_lv_bool(1);
}

static wb_lv_value gui_width(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)a; (void)n;
    WubuLuaGui *g = (WubuLuaGui *)ud;
    return wb_lv_number(g ? g->fb_w : vbe_width());
}

static wb_lv_value gui_height(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)a; (void)n;
    WubuLuaGui *g = (WubuLuaGui *)ud;
    return wb_lv_number(g ? g->fb_h : vbe_height());
}

static wb_lv_value gui_button(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    int ok = 0;
    if (n < 5) return wb_lv_bool(0);
    int x = num_arg(&a[0], &ok); if (!ok) return wb_lv_bool(0);
    int y = num_arg(&a[1], &ok); if (!ok) return wb_lv_bool(0);
    int w = num_arg(&a[2], &ok); if (!ok) return wb_lv_bool(0);
    int h = num_arg(&a[3], &ok); if (!ok) return wb_lv_bool(0);
    const char *label = str_arg(&a[4]);
    vbe_3d_raised_colors(x, y, w, h, 0xC0C0C0, 0xFFFFFF, 0x808080, 0x000000);
    if (label) {
        int tw = vbe_text_width(label, 1);
        vbe_draw_text(x + (w - tw) / 2, y + h / 2 - 4, label, 0x000000, 1);
    }
    /* Click state is delivered through on_click(); a button returns true so
     * a script can use `if gui.button(...) then`. */
    return wb_lv_bool(1);
}

/* ---- fs.* bindings ------------------------------------------------- */

static wb_lv_value fs_read(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return wb_lv_nil();
    const char *path = str_arg(&a[0]);
    if (!path || !path[0]) return wb_lv_nil();

    FILE *f = fopen(path, "rb");
    if (!f) return wb_lv_nil();
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return wb_lv_nil(); }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return wb_lv_nil(); }
    size_t got = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[got] = '\0';
    wb_lv_value v = wb_lv_str(buf);
    free(buf);
    return v;
}

static wb_lv_value fs_write(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 2) return wb_lv_bool(0);
    const char *path = str_arg(&a[0]);
    const char *text = str_arg(&a[1]);
    if (!path || !path[0]) return wb_lv_bool(0);

    FILE *f = fopen(path, "wb");
    if (!f) return wb_lv_bool(0);
    int ok = 1;
    if (text && fputs(text, f) < 0) ok = 0;
    if (fclose(f) != 0) ok = 0;
    return wb_lv_bool(ok);
}

static wb_lv_value fs_exists(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return wb_lv_bool(0);
    const char *path = str_arg(&a[0]);
    if (!path) return wb_lv_bool(0);
    FILE *f = fopen(path, "rb");
    if (!f) return wb_lv_bool(0);
    fclose(f);
    return wb_lv_bool(1);
}

/* The Wua coroutine workaround lives in wubu_lua_compat.c -- one owner. */

/* ---- lifecycle ----------------------------------------------------- */

WubuLuaGui *wubu_lua_gui_new(int fb_w, int fb_h) {
    WubuLuaGui *g = (WubuLuaGui *)calloc(1, sizeof(WubuLuaGui));
    if (!g) return NULL;
    g->fb_w = fb_w;
    g->fb_h = fb_h;
    g->L = wb_lv_new();
    if (!g->L) { free(g); return NULL; }

    /* Base library comes with wb_lv_new(); add string/table/math so a script
     * can do real label work. */
    wb_lv_openlibs(g->L);

    /* gui.* -- a table, so scripts call gui.text(...) not a flat name. */
    /* Table keys must be INTERNED: the interpreter hashes by string identity,
     * so a plain wb_lv_str("text") would never match the script's own
     * "text" literal. */
    wb_lv_table *gui = wb_lv_table_new(g->L, 0, 8);
    if (gui) {
        wb_lv_table_set(g->L, gui, wb_lv_str(wb_lv_intern(g->L, "text", 4)),   cfn_value(gui_text,   g));
        wb_lv_table_set(g->L, gui, wb_lv_str(wb_lv_intern(g->L, "rect", 4)),   cfn_value(gui_rect,   g));
        wb_lv_table_set(g->L, gui, wb_lv_str(wb_lv_intern(g->L, "fill", 4)),   cfn_value(gui_fill,   g));
        wb_lv_table_set(g->L, gui, wb_lv_str(wb_lv_intern(g->L, "clear", 5)),  cfn_value(gui_clear,  g));
        wb_lv_table_set(g->L, gui, wb_lv_str(wb_lv_intern(g->L, "width", 5)),  cfn_value(gui_width,  g));
        wb_lv_table_set(g->L, gui, wb_lv_str(wb_lv_intern(g->L, "height", 6)), cfn_value(gui_height, g));
        wb_lv_table_set(g->L, gui, wb_lv_str(wb_lv_intern(g->L, "button", 6)), cfn_value(gui_button, g));
        wb_lv_set_global(g->L, "gui", table_value(gui));
    }

    wb_lv_table *fs = wb_lv_table_new(g->L, 0, 4);
    if (fs) {
        wb_lv_table_set(g->L, fs, wb_lv_str(wb_lv_intern(g->L, "read", 4)),   cfn_value(fs_read,   g));
        wb_lv_table_set(g->L, fs, wb_lv_str(wb_lv_intern(g->L, "write", 5)),  cfn_value(fs_write,  g));
        wb_lv_table_set(g->L, fs, wb_lv_str(wb_lv_intern(g->L, "exists", 6)), cfn_value(fs_exists, g));
        wb_lv_set_global(g->L, "fs", table_value(fs));
    }

    return g;
}

void wubu_lua_gui_free(WubuLuaGui *g) {
    if (!g) return;
    wb_lv_free(g->L);
    free(g);
}

int wubu_lua_gui_load(WubuLuaGui *g, const char *path, char *err, size_t err_cap) {
    if (!g || !path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) {
        set_err(g, "cannot open script");
        if (err) snprintf(err, err_cap, "cannot open %s", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); set_err(g, "cannot read script"); return -1; }

    char *src = (char *)malloc((size_t)len + 1);
    if (!src) { fclose(f); set_err(g, "out of memory"); return -1; }
    size_t got = fread(src, 1, (size_t)len, f);
    fclose(f);
    src[got] = '\0';

    const char *e = wb_lv_run(g->L, src, path);
    free(src);
    if (e) {
        set_err(g, e);
        if (err) snprintf(err, err_cap, "%s", e);
        return -1;
    }

    /* Does the script define draw()? */
    wb_lv_value fn = wb_lv_get_global(g->L, "draw");
    g->has_draw = (fn.t == WB_LV_FUNCTION || fn.t == WB_LV_CFUNCTION);
    return 0;
}

bool wubu_lua_gui_has_draw(const WubuLuaGui *g) {
    return g && g->has_draw;
}

int wubu_lua_gui_draw(WubuLuaGui *g, int fb_w, int fb_h) {
    if (!g || !g->L) return -1;
    if (fb_w > 0) g->fb_w = fb_w;
    if (fb_h > 0) g->fb_h = fb_h;

    wb_lv_value fn = wb_lv_get_global(g->L, "draw");
    if (fn.t != WB_LV_FUNCTION && fn.t != WB_LV_CFUNCTION) return 0;

    wb_lv_value r = wb_lv_call(g->L, fn, NULL, 0);
    (void)r;
    return 0;
}

int wubu_lua_gui_key(WubuLuaGui *g, const char *key, uint32_t mods) {
    if (!g || !g->L || !key) return 0;
    (void)mods;

    wb_lv_value fn = wb_lv_get_global(g->L, "on_key");
    if (fn.t != WB_LV_FUNCTION && fn.t != WB_LV_CFUNCTION) return 0;

    wb_lv_value arg = wb_lv_str(key);
    wb_lv_value r = wb_lv_call(g->L, fn, &arg, 1);
    (void)r;
    return 1;
}

const char *wubu_lua_gui_error(const WubuLuaGui *g) {
    return g ? g->err : NULL;
}
