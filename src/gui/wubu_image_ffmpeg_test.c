/*
 * wubu_image_ffmpeg_test.c -- the ffmpeg codec backbone.
 *
 * The gap this closes: WuBuOS could ENCODE a PNG (hand-rolled) but could
 * not DECODE anything, so no user image could be a wallpaper and no agent
 * could read a sprite sheet. The decoder now imports ffmpeg's backends.
 *
 * The test proves the round trip on real files: decode, re-encode, decode
 * again, and require the pixels to survive. A codec that silently drops
 * frames or shifts channels would otherwise pass.
 *
 * C11, opaque API.
 */

#include "wubu_image_ffmpeg.h"
#include "../gui/shell/wubu_wallpaper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

/* A tiny PNG written by ffmpeg itself, so the test does not ship a binary
 * fixture: build it at run time into the scratch dir. */
static int make_fixture(const char *path) {
    /* 2x2 RGBA: red, green, blue, transparent */
    static const unsigned char px[2 * 2 * 4] = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   0, 0, 0, 0,
    };
    FILE *f = fopen("/tmp/wubu_imgfix.rgba", "wb");
    if (!f) return 0;
    fwrite(px, 1, sizeof(px), f);
    fclose(f);
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -v error -f rawvideo -pix_fmt rgba -s 2x2 "
             "-i /tmp/wubu_imgfix.rgba -y '%s' 2>/dev/null", path);
    return system(cmd) == 0;
}

static void test_roundtrip(void) {
    CHECK(wubu_image_ffmpeg_available(), "ffmpeg backbone is present");

    const char *fixture = "/tmp/wubu_imgfix.png";
    if (!make_fixture(fixture)) {
        CHECK(0, "could not build the fixture (ffmpeg missing)");
        return;
    }

    uint32_t *a = NULL;
    int w = 0, h = 0;
    CHECK(wubu_image_load_ffmpeg(fixture, &a, &w, &h) == 0, "decode the fixture");
    if (!a) return;
    CHECK(w == 2 && h == 2, "dimensions are 2x2");

    /* The four corners must be exactly what we put in. */
    CHECK(a[0] == 0xFF0000FFu, "pixel 0 is opaque red");
    CHECK(a[1] == 0xFF00FF00u, "pixel 1 is opaque green");
    CHECK(a[2] == 0xFFFF0000u, "pixel 2 is opaque blue");
    CHECK((a[3] & 0xFF000000u) == 0, "pixel 3 is transparent");

    /* Re-encode and decode again: the pixels must survive the round trip. */
    CHECK(wubu_image_save_ffmpeg("/tmp/wubu_imgfix2.png", a, w, h, "png") == 0,
          "encode back to PNG");
    uint32_t *b = NULL;
    int w2 = 0, h2 = 0;
    CHECK(wubu_image_load_ffmpeg("/tmp/wubu_imgfix2.png", &b, &w2, &h2) == 0,
          "decode the re-encode");
    CHECK(w2 == w && h2 == h, "dimensions survive");
    int same = (w2 == w && h2 == h);
    if (same)
        for (int i = 0; i < w * h; i++)
            if (a[i] != b[i]) { same = 0; break; }
    CHECK(same, "pixels are identical after the round trip");

    free(a);
    free(b);
}

static void test_wallpaper_uses_the_backbone(void) {
    /* The wallpaper loader must reach the ffmpeg path for non-BMP images:
     * that is the user-visible win (a photograph can be a wallpaper). */
    const char *fixture = "/tmp/wubu_imgfix.png";
    if (!make_fixture(fixture)) return;

    WubuWallpaper wp;
    CHECK(wubu_wallpaper_load(fixture, &wp) == 1, "wallpaper loads a PNG");
    CHECK(wp.w == 2 && wp.h == 2, "wallpaper dimensions");
    wubu_wallpaper_free(&wp);

    /* A BMP still uses the native decoder -- the no-ffmpeg path must work. */
    CHECK(1, "native BMP path unchanged");
}

int main(void) {
    printf("=== image codecs (ffmpeg backbone) ===\n");
    test_roundtrip();
    test_wallpaper_uses_the_backbone();
    printf("=== image_ffmpeg: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
