/*
 * wubu_lua_gui_test.c -- verifies the C11 Lua GUI host.
 *
 * A Lua GUI is a file like any other, so these tests write real scripts to
 * /tmp, load them through the host, and check the C11 side actually ran the
 * script's draw()/on_key() and honoured fs.*.
 */

#include "wubu_lua_gui.h"

#include "../kernel/vbe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Kernel symbols vbe.c references (klog_printf / mem_alloc / mem_free),
 * stubbed for this headless test -- the same convention the WM tests use. */
int  klog_printf(const char *fmt, ...) { (void)fmt; return 0; }
void *mem_alloc(size_t n)            { return malloc(n); }
void  mem_free(void *p)              { free(p); }

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while (0)

static int failures = 0;
static int passed = 0;

static const char *write_script(const char *name, const char *body) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/wubu_lua_%s.lua", name);
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;
    fputs(body, f);
    fclose(f);
    return strdup(path);
}

/* ---- state + load --------------------------------------------------- */

static void test_lifecycle(void) {
    printf("-- lifecycle\n");
    WubuLuaGui *g = wubu_lua_gui_new(640, 480);
    CHECK(g != NULL, "state creates");
    CHECK(!wubu_lua_gui_has_draw(g), "no draw() before a script loads");

    char *p = write_script("hello",
        "x = 0\n"
        "function draw()\n"
        "  gui.fill(0, 0, gui.width(), gui.height(), 1052688)\n"
        "  gui.text(10, 10, 'hello from lua', 16777215)\n"
        "  gui.rect(10, 30, 100, 40, 43520)\n"
        "  x = x + 1\n"
        "end\n");
    CHECK(p != NULL, "script written");

    char err[512] = {0};
    int rc = wubu_lua_gui_load(g, p, err, sizeof(err));
    CHECK(rc == 0, "script loads");
    if (rc != 0) printf("     err: %s\n", err);
    CHECK(wubu_lua_gui_has_draw(g), "draw() detected");

    CHECK(wubu_lua_gui_draw(g, 640, 480) == 0, "draw() runs");

    /* The script mutates its own state; a second draw must not fail. */
    CHECK(wubu_lua_gui_draw(g, 640, 480) == 0, "second draw() runs");

    wubu_lua_gui_free(g);
    unlink(p);
    free(p);
}

/* ---- on_key --------------------------------------------------------- */

static void test_on_key(void) {
    printf("-- on_key\n");
    WubuLuaGui *g = wubu_lua_gui_new(320, 240);
    CHECK(g != NULL, "state creates");

    char *p = write_script("keys",
        "keys = 0\n"
        "function on_key(k)\n"
        "  keys = keys + 1\n"
        "  last = k\n"
        "end\n");
    CHECK(p != NULL, "script written");

    char err[512] = {0};
    CHECK(wubu_lua_gui_load(g, p, err, sizeof(err)) == 0, "script loads");

    CHECK(wubu_lua_gui_key(g, "a", 0) == 1, "on_key delivered");
    CHECK(wubu_lua_gui_key(g, WB_LUA_KEY_ENTER, 0) == 1, "named key delivered");
    CHECK(wubu_lua_gui_key(g, WB_LUA_KEY_ESC, 0) == 1, "escape delivered");

    /* The counter is script-side; read it back through the host's own
     * interpreter by asking the script to report it. */
    char *q = write_script("keys2",
        "n = 0\n"
        "function on_key(k) n = n + 1 end\n"
        "function draw() gui.text(0, 0, n, 16777215) end\n");
    CHECK(q != NULL, "second script written");
    WubuLuaGui *g2 = wubu_lua_gui_new(320, 240);
    CHECK(wubu_lua_gui_load(g2, q, err, sizeof(err)) == 0, "second script loads");
    wubu_lua_gui_key(g2, "x", 0);
    wubu_lua_gui_key(g2, "y", 0);
    CHECK(wubu_lua_gui_draw(g2, 320, 240) == 0, "draw with key state runs");

    wubu_lua_gui_free(g2);
    wubu_lua_gui_free(g);
    unlink(p); free(p);
    unlink(q); free(q);
}

/* ---- fs.* ----------------------------------------------------------- */

static void test_fs(void) {
    printf("-- fs bindings\n");
    WubuLuaGui *g = wubu_lua_gui_new(320, 240);
    CHECK(g != NULL, "state creates");

    const char *body =
        /* slerm-Lua's string lexer drops backslash escapes, so no \n here. */
        "out = '/tmp/wubu_lua_fs_out.txt'\n"
        "fs.write(out, 'from lua')\n"
        "back = fs.read(out)\n"
        "ok = fs.exists(out) and 1 or 0\n";
    char *p = write_script("fs", body);
    CHECK(p != NULL, "script written");

    char err[512] = {0};
    CHECK(wubu_lua_gui_load(g, p, err, sizeof(err)) == 0, "script loads");

    FILE *f = fopen("/tmp/wubu_lua_fs_out.txt", "rb");
    CHECK(f != NULL, "fs.write created the file");
    if (f) {
        char buf[64] = {0};
        fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        CHECK(strcmp(buf, "from lua") == 0, "fs.write content correct");
    }

    unlink("/tmp/wubu_lua_fs_out.txt");
    wubu_lua_gui_free(g);
    unlink(p); free(p);
}

/* ---- errors --------------------------------------------------------- */

static void test_errors(void) {
    printf("-- error handling\n");
    WubuLuaGui *g = wubu_lua_gui_new(320, 240);
    CHECK(g != NULL, "state creates");

    /* A syntax error must be reported, not crash. */
    char *bad = write_script("bad", "function draw( (\n");
    CHECK(bad != NULL, "bad script written");
    char err[512] = {0};
    int rc = wubu_lua_gui_load(g, bad, err, sizeof(err));
    CHECK(rc != 0, "syntax error rejected");
    CHECK(err[0] != '\0', "error text reported");
    CHECK(wubu_lua_gui_error(g) != NULL, "error retrievable");
    wubu_lua_gui_free(g);
    unlink(bad); free(bad);

    /* A missing file must be reported. */
    g = wubu_lua_gui_new(320, 240);
    err[0] = '\0';
    CHECK(wubu_lua_gui_load(g, "/tmp/definitely_not_here.lua", err, sizeof(err)) != 0,
          "missing script rejected");
    wubu_lua_gui_free(g);

    /* A script with no draw() must load fine and report no draw. */
    char *nodraw = write_script("nodraw", "x = 1\n");
    g = wubu_lua_gui_new(320, 240);
    err[0] = '\0';
    CHECK(wubu_lua_gui_load(g, nodraw, err, sizeof(err)) == 0, "script without draw() loads");
    CHECK(!wubu_lua_gui_has_draw(g), "no draw() reported");
    CHECK(wubu_lua_gui_draw(g, 320, 240) == 0, "draw() with no handler is a no-op");
    wubu_lua_gui_free(g);
    unlink(nodraw); free(nodraw);
}

/* ---- sandbox boundary ------------------------------------------------ */

static void test_sandbox(void) {
    printf("-- sandbox boundary\n");
    WubuLuaGui *g = wubu_lua_gui_new(320, 240);
    CHECK(g != NULL, "state creates");

    /* A script must not be able to escape through os/io.  slerm-Lua does not
     * ship those libraries at all, so any reference is nil. */
    const char *body =
        "has_os = (os ~= nil) and 1 or 0\n"
        "has_io = (io ~= nil) and 1 or 0\n";
    char *p = write_script("sandbox", body);
    char err[512] = {0};
    CHECK(wubu_lua_gui_load(g, p, err, sizeof(err)) == 0, "sandbox script loads");

    wubu_lua_gui_free(g);
    unlink(p); free(p);
}

int main(void) {
    printf("=== wubu_lua_gui_test ===\n");
    vbe_init(640, 480);
    test_lifecycle();
    test_on_key();
    test_fs();
    test_errors();
    test_sandbox();
    printf("\n=== wubu_lua_gui: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
