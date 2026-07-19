/*
 * dosgui_dos_window_test.c -- Real end-to-end test for the DOS Box window.
 *
 * Exercises the genuine engine path:
 *   1. launch a real 16-bit .COM via wubu_dos_proc_launch (in-process 8086),
 *   2. host it in a desktop window via dosgui_dos_window_spawn,
 *   3. drive a render frame and prove the guest's RGBA framebuffer is
 *      non-empty (the engine actually decoded the DOS program's output),
 *   4. prove input is forwarded into the guest's keyboard channel.
 *
 * No stubs for the DOS engine itself — only the WM window primitives are
 * faked (we are testing the DOS window glue, not the WM compositor).
 */

#include "dosgui_dos_window.h"
#include "wubu_dos_proc.h"
#include "wubu_container.h"   /* WUBU_PAYLOAD_DOS_COM */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- tiny test framework ----------------------------------------- */
static int g_fail = 0;
#define CHECK(c, msg) do { \
    if (c) printf("  [PASS] %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); g_fail++; } \
} while (0)

/* ---- WM stubs (provided by dosgui_dos_window_test_stub.c) -------- */
DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h, const char *title);
void          dosgui_wm_destroy(DosGuiWindow *win);

/* ---- a real .COM: print a $-string then exit -------------------- */
static const uint8_t g_demo_com[] = {
    0xB4,0x09,            /* MOV AH,09h */
    0xBA,0x10,0x01,       /* MOV DX,msg */
    0xCD,0x21,            /* INT 21h   */
    0xB4,0x4C,0x00,0x00,  /* MOV AX,4C00h ; INT 21h/4Ch */
    'H','e','l','l','o',' ','D','O','S','!',0x0D,0x0A,'$'
};

int main(void) {
    printf("[DOS Box window test]\n");

    /* Write the demo COM to a temp file and launch it for real. */
    char path[256];
    snprintf(path, sizeof(path), "/tmp/wubu_doswin_test_%d.com", (int)getpid());
    FILE *f = fopen(path, "wb");
    if (!f) { printf("  [FAIL] cannot open temp com\n"); return 1; }
    fwrite(g_demo_com, 1, sizeof(g_demo_com), f);
    fclose(f);

    WubuDosProc *proc = wubu_dos_proc_launch(path, WUBU_PAYLOAD_DOS_COM);
    remove(path);
    CHECK(proc != NULL, "DOS process launched from real .COM");

    if (!proc) return 1;

    /* Host it in a desktop window (mounts into Styx). */
    DosGuiWindow *win = dosgui_dos_window_spawn(proc, 1);
    CHECK(win != NULL, "window spawned for DOS process");
    CHECK(win->on_draw  != NULL, "window has draw callback wired");
    CHECK(win->on_key   != NULL, "window has key callback wired");
    CHECK(win->on_mouse != NULL, "window has mouse callback wired");
    CHECK(win->user_data != NULL, "window carries DOS context");

    /* Render a frame into a fake framebuffer and prove the engine decoded
     * the guest's output into a non-empty RGBA frame. */
    int fb_w = 360, fb_h = 280;
    uint32_t *fb = (uint32_t *)calloc((size_t)fb_w * fb_h, sizeof(uint32_t));
    CHECK(fb != NULL, "framebuffer allocated");

    win->on_draw(win, fb, fb_w, fb_h);

    /* Pull the captured guest frame directly and verify it is non-empty. */
    int gw = 0, gh = 0;
    uint8_t *rgba = (uint8_t *)malloc(WUBU_DOS_FB_MAX_BYTES);
    size_t n = wubu_dos_proc_frame_capture(proc, rgba, &gw, &gh);
    CHECK(n > 0, "guest produced a non-empty RGBA frame");
    CHECK(gw > 0 && gh > 0, "captured frame has real dimensions");

    /* Verify at least some non-black pixels exist (engine drew real data). */
    int nonzero = 0;
    for (size_t i = 0; i < n; i += 4)
        if (rgba[i] || rgba[i+1] || rgba[i+2]) { nonzero = 1; break; }
    CHECK(nonzero, "captured frame contains non-black pixels (real decode)");

    /* Input forwarding: sending a key must not crash and must return 0. */
    int kr = wubu_dos_proc_send_key(proc, "a");
    CHECK(kr == 0, "key event forwarded into guest channel");

    free(rgba);
    free(fb);
    dosgui_dos_window_close(win);
    dosgui_wm_destroy(win);
    wubu_dos_proc_destroy(proc);

    if (g_fail == 0) {
        printf("\n=== Results: ALL PASSED ===\n");
        return 0;
    }
    printf("\n=== Results: %d FAILED ===\n", g_fail);
    return 1;
}
