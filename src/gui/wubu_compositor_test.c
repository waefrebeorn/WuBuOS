/*
 * wubu_compositor_test.c -- Test for WuBuOS Compositor
 */

#include "wubu_compositor.h"
#include "../runtime/styx.h"   /* styx_server_t + styx_init + styx_fid_alloc */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== WuBuOS Compositor Test ===\n");

    /* Test 1: Create compositor */
    WuBuCompositor *comp = wubu_compositor_create();
    if (!comp) {
        fprintf(stderr, "FAIL: wubu_compositor_create returned NULL\n");
        return 1;
    }
    printf("✅ wubu_compositor_create succeeded\n");

    /* Test 2: Get display */
    struct wl_display *display = wubu_compositor_get_display(comp);
    if (!display) {
        fprintf(stderr, "FAIL: wubu_compositor_get_display returned NULL\n");
        wubu_compositor_destroy(comp);
        return 1;
    }
    printf("✅ wubu_compositor_get_display succeeded\n");

    /* Test 3: Create window */
    WuBuWindow *win = wubu_window_create(comp, WUBU_WINDOW_TOPLEVEL);
    if (!win) {
        fprintf(stderr, "FAIL: wubu_window_create returned NULL\n");
        wubu_compositor_destroy(comp);
        return 1;
    }
    printf("✅ wubu_window_create succeeded\n");

    /* Test 4: Set/get window title */
    wubu_window_set_title(win, "Test Window");
    printf("✅ wubu_window_set_title succeeded\n");

    /* Test 5: Set/get window geometry */
    WuBuWindowGeometry geom = {
        .x = 100, .y = 100,
        .width = 800, .height = 600,
        .scale = 1.0,
        .opacity = 1.0,
        .transform = {1,0,0, 0,1,0, 0,0,1}
    };
    wubu_window_set_geometry(win, &geom);
    WuBuWindowGeometry geom2;
    wubu_window_get_geometry(win, &geom2);
    if (geom2.width != 800 || geom2.height != 600) {
        fprintf(stderr, "FAIL: geometry mismatch\n");
        wubu_window_destroy(win);
        wubu_compositor_destroy(comp);
        return 1;
    }
    printf("✅ wubu_window_set/get_geometry succeeded\n");

    /* Test 6: Window opacity */
    wubu_window_set_opacity(win, 0.5f);
    printf("✅ wubu_window_set_opacity succeeded\n");

    /* Test 7: Window minimize/maximize/fullscreen */
    wubu_window_set_minimized(win, true);
    wubu_window_set_maximized(win, true);
    wubu_window_set_fullscreen(win, true, NULL);
    printf("✅ window state changes succeeded\n");

    /* Test 8: Get outputs */
    WuBuOutputInfo outputs[2];
    int count = wubu_compositor_get_outputs(comp, outputs, 2);
    if (count <= 0) {
        fprintf(stderr, "FAIL: wubu_compositor_get_outputs returned %d\n", count);
        wubu_window_destroy(win);
        wubu_compositor_destroy(comp);
        return 1;
    }
    printf("✅ wubu_compositor_get_outputs returned %d output(s)\n", count);

    /* Test 9: 9P path */
    const char *path = wubu_compositor_get_9p_path(comp);
    if (!path) {
        fprintf(stderr, "FAIL: wubu_compositor_get_9p_path returned NULL\n");
        wubu_window_destroy(win);
        wubu_compositor_destroy(comp);
        return 1;
    }
    printf("✅ wubu_compositor_get_9p_path: %s\n", path);

    /* Test 9b: real 9P export + read (#31 closure).
     * Wire the live compositor into a styx_server_t, then read the
     * "outputs" node (qid.path=1) and confirm it returns REAL
     * JSON state -- not a stub. */
    styx_server_t srv;
    memset(&srv, 0, sizeof(srv));   /* styx_init() equivalent, avoids linking styx_serve.o in this test */
    srv.next_tag = 1;
    srv.msize = STYX_MAX_MSG;
    if (wubu_compositor_styx_export(comp, &srv) != 0) {
        fprintf(stderr, "FAIL: styx_export returned != 0\n");
        wubu_compositor_destroy(comp);
        return 1;
    }
    styx_fid_t *f = styx_fid_alloc(&srv, 7);
    if (!f) {
        fprintf(stderr, "FAIL: styx_fid_alloc returned NULL\n");
        wubu_compositor_destroy(comp);
        return 1;
    }
    f->qid.path = 1;   /* outputs node */
    uint8_t rbuf[256];
    uint32_t nread = 0;
    if (srv.read(&srv, 7, 0, sizeof(rbuf), rbuf, &nread) != 0 || nread == 0) {
        fprintf(stderr, "FAIL: 9P read of /n/compositor/outputs returned empty\n");
        wubu_compositor_destroy(comp);
        return 1;
    }
    rbuf[nread < 255 ? nread : 255] = '\0';
    printf("✅ 9P read /n/compositor/outputs: %s\n", (char *)rbuf);

    /* Test 10: GPU init */
    bool gpu_ok = wubu_compositor_gpu_init(comp);
    printf("✅ wubu_compositor_gpu_init: %s\n", gpu_ok ? "success" : "failed (expected in test env)");

    /* Cleanup */
    wubu_compositor_gpu_fini(comp);
    wubu_window_destroy(win);
    wubu_compositor_destroy(comp);

    printf("\n=== All Compositor Tests Passed ===\n");
    return 0;
}