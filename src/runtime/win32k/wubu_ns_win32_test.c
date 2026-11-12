/*
 * wubu_ns_win32_test.c -- WuBuOS /n/win32 control-plane test.
 *
 * Drives the REAL win32k registry + the namespace FS primitives against a
 * temp g_ns_root: create station/desktop/class/window, post two messages,
 * wubu_ns_win32_sync(), then read back the /n/win32/* files from disk and
 * assert the view. Standalone (opt-in), mirroring test_ns_kernel's recipe.
 */
#include "../../egojar.h"
#include "wubu_ns_bridge.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_win32.h"
#include "wubu_win32k.h"

#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(c, m) do { if (c) { g_pass++; printf("  [PASS] %s\n", m); } \
                        else { g_fail++; printf("  [FAIL] %s\n", m); } } while (0)

/* Read a file fully into buf (NUL-terminated). Returns 1 on success. */
static int read_file(const char *path, char *buf, size_t cap) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return 1;
}

int main(void) {
    const char *root = "/tmp/wubu_ns_win32_root";
    mkdir(root, 0755);
    wubu_ns_bridge_create(root);
    wubu_win32k_init();
    printf("=== WuBuOS /n/win32 control-plane test ===\n");

    /* Stamp the registry: station -> desktop -> class -> window + 2 msgs. */
    wubu_cap_token_t st, dt, w;
    CHECK(wubu_win32k_station_create("winsta0", 100, &st) > 0, "station create");
    CHECK(wubu_win32k_desktop_create("default", st, 100, &dt) > 0, "desktop create");
    CHECK(wubu_win32k_class_register(dt, "Button", (uintptr_t)0x1c0de, 100) == 0, "class register");
    CHECK(wubu_win32k_window_create(dt, "Button", "w1", 100, &w) > 0, "window create");
    CHECK(wubu_win32k_post_message(w, 100, 0x0111, 1, 0) == 1, "post msg 1");
    CHECK(wubu_win32k_post_message(w, 100, 0x0112, 2, 0) == 2, "post msg 2");

    /* A GDI object so the /n/win32/gdi view is non-empty. */
    wubu_cap_token_t gdc;
    CHECK(wubu_win32k_gdi_create(WUBU_WIN32K_SUB_DC, "screenDC", 100, &gdc) > 0,
          "gdi dc create");

    /* Publish the /n/win32 view. */
    CHECK(wubu_ns_win32_sync() == 0, "sync publishes /n/win32");

    char buf[8192];
    char p[512];

    snprintf(p, sizeof(p), "%s/win32/registry", root);
    CHECK(read_file(p, buf, sizeof(buf)) && strstr(buf, "station winsta0"),
          "registry lists station");
    CHECK(strstr(buf, "class Button"), "registry lists class");
    CHECK(strstr(buf, "window w1"), "registry lists window");

    snprintf(p, sizeof(p), "%s/win32/stations", root);
    CHECK(read_file(p, buf, sizeof(buf)) && strstr(buf, "winsta0"),
          "stations file has winsta0");

    snprintf(p, sizeof(p), "%s/win32/windows", root);
    CHECK(read_file(p, buf, sizeof(buf)) && strstr(buf, "w1"),
          "windows file has w1");
    CHECK(strstr(buf, "queue=2/64"), "windows file shows queue=2/64");

    snprintf(p, sizeof(p), "%s/win32/gdi", root);
    CHECK(read_file(p, buf, sizeof(buf)) && strstr(buf, "gdi dc screenDC"),
          "gdi file has the DC");

    /* g_ns_root unset -> sync is a safe no-op, not a crash. */
    *(const char **)&g_ns_root = NULL;  /* null the extern, via the pointer */
    CHECK(wubu_ns_win32_sync() == 0, "sync no-ops when root unset");

    rmdir(root);
    printf("=== /n/win32: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
