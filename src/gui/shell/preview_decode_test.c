/* preview_decode_test.c -- the gate for the preview's decode path.
 *
 * Fixtures are written by tools/make_jpeg_fixtures.py (Pillow) and by this test itself for
 * BMP/GIF; the expected pixels come from the files the oracle wrote, not from our decoder.
 */
#include "preview_decode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass, fail;
static void check(int c, const char *w) { if (c) { pass++; return; } printf("  [FAIL] %s\n", w); fail++; }

int main(int argc, char **argv) {
    printf("=== preview_decode (gate for the Explorer image path) ===\n");
    const char *dir = (argc > 1) ? argv[1] : "/tmp/jfx";
    char path[512];
    uint32_t *px = NULL; int w = 0, h = 0;

    snprintf(path, sizeof path, "%s/colour_420.jpg", dir);
    int rc = wubu_preview_decode_file(path, &px, &w, &h, 0);
    check(rc == WUBU_PREVIEW_OK && px && w == 64 && h == 48, "a JPEG decodes to 64x48 packed pixels");
    if (px && w == 64 && h == 48) {
        /* the red square at (24,18) must be red in the PACKED form too: 0x00RRGGBB */
        uint32_t p = px[18 * 64 + 24];
        check(((p >> 16) & 255) > 180 && ((p >> 8) & 255) < 80 && (p & 255) < 80,
              "packed order is 0x00RRGGBB (red square reads red)");
    }
    free(px); px = NULL;

    snprintf(path, sizeof path, "%s/grey_400.jpg", dir);
    rc = wubu_preview_decode_file(path, &px, &w, &h, 0);
    check(rc == WUBU_PREVIEW_OK && w == 48 && h == 40, "a greyscale JPEG decodes");
    if (px) { uint32_t p = px[0]; check(((p >> 16) & 255) == (p & 255), "greyscale stays grey after packing"); }
    free(px); px = NULL;

    /* a BMP written by the oracle (Pillow), because the hand-built one this replaced had a
     * header that disagreed with its own length -- the decoder refused it and the fixture was
     * wrong, which is exactly the kind of "bug" a hand-rolled header manufactures. */
    snprintf(path, sizeof path, "%s/rows.bmp", dir);
    rc = wubu_preview_decode_file(path, &px, &w, &h, 0);
    check(rc == WUBU_PREVIEW_OK && w == 2 && h == 2, "a 24-bit BMP decodes");
    if (px && w == 2 && h == 2) {
        /* BMP rows are BOTTOM-UP: a missing flip shows red on top, a double flip is identical
         * to no flip, so this pair catches both. */
        check(px[0] == 0x0000FF00u, "row order is flipped (top row is green)");
        check(px[2] == 0x00FF0000u, "BGR becomes RGB (bottom row is red)");
    }
    free(px); px = NULL;

    /* refusals: a format with no decoder, a missing file, and the size cap */
    snprintf(path, sizeof path, "%s/not-an-image.webp", dir);
    check(wubu_preview_decode_file(path, &px, &w, &h, 0) == WUBU_PREVIEW_FORMAT, "webp is refused as a format, not a broken image");
    snprintf(path, sizeof path, "%s/definitely-missing.png", dir);
    check(wubu_preview_decode_file(path, &px, &w, &h, 0) == WUBU_PREVIEW_OPEN, "a missing file is refused as open");
    snprintf(path, sizeof path, "%s/notjpeg.jpg", dir);
    check(wubu_preview_decode_file(path, &px, &w, &h, 0) == WUBU_PREVIEW_DECODE, "bytes that are not an image are refused");
    snprintf(path, sizeof path, "%s/colour_420.jpg", dir);
    check(wubu_preview_decode_file(path, &px, &w, &h, 100) == WUBU_PREVIEW_DECODE ||
          wubu_preview_decode_file(path, &px, &w, &h, 100) == WUBU_PREVIEW_TOO_BIG, "a cap below the image size refuses");

    printf("\n=== %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
