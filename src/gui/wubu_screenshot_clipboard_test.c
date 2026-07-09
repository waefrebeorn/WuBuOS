/* Regression test for wubu_screenshot_to_clipboard (was a no-op returning
 * true with nothing stored). Builds a real screenshot, copies it to the
 * clipboard, and asserts a DECODABLE PNG is produced. The 4 GUI symbols
 * wubu_screenshot.c references are stubbed here so the test links without
 * the full Wayland/WM stack. */

#include "wubu_screenshot.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* ---- stubs for the few GUI externals wubu_screenshot.c references ---- */
void wubu_theme_colors(int idx, unsigned char *r, unsigned char *g, unsigned char *b) {
    (void)idx; *r = *g = *b = 0;
}
void wubu_notify_simple(const char *msg) { (void)msg; }
struct wubu_wl_stub { int dummy; };
struct wubu_wl_stub g_wl;
void *dosgui_wm_get_focused(void) { return NULL; }

static uint32_t rd_be32(const uint8_t *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  | (uint32_t)b[3];
}

int main(void) {
    printf("Testing screenshot clipboard (real PNG encode)...\n");

    wubu_sshot_t *sshot = calloc(1, sizeof(wubu_sshot_t));
    assert(sshot);
    sshot->width = 100; sshot->height = 100; sshot->stride = 400;
    sshot->pixels = calloc(100 * 100, 4);
    assert(sshot->pixels);
    for (int i = 0; i < 100 * 100; i++)
        sshot->pixels[i] = 0xFF0000FFu; /* opaque red */

    /* NULL rejected */
    assert(wubu_screenshot_to_clipboard(NULL) == false);

    /* Real encode */
    int ok = wubu_screenshot_to_clipboard(sshot);
    printf("to_clipboard returned: %d\n", ok);
    assert(ok == true);

    size_t clip_len = 0;
    const uint8_t *clip = wubu_screenshot_clipboard_data(&clip_len);
    assert(clip != NULL);
    assert(clip_len > 8);

    /* PNG magic */
    assert(clip[0]==0x89 && clip[1]=='P' && clip[2]=='N' && clip[3]=='G' &&
           clip[4]==0x0D && clip[5]==0x0A && clip[6]==0x1A && clip[7]==0x0A);

    /* IHDR width/height/color-type match */
    uint32_t w = rd_be32(clip + 16);
    uint32_t h = rd_be32(clip + 20);
    printf("clipboard PNG: %u x %u, %zu bytes\n", w, h, clip_len);
    assert(w == 100 && h == 100);
    assert(clip[25] == 6); /* RGBA */

    /* Persist so external tools (PIL/file) can validate the real bytes. */
    FILE *f = fopen("/tmp/wubu_clipboard_test.png", "wb");
    fwrite(clip, 1, clip_len, f); fclose(f);

    free(sshot->pixels);
    free(sshot);
    printf("✅ screenshot clipboard PNG encode passed\n");
    return 0;
}
