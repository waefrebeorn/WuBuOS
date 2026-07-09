#include "wubu_screenshot.h"
#include "wubu_theme.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

/* Parse big-endian uint32 from a PNG buffer offset. */
static uint32_t rd_be32(const uint8_t *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  | (uint32_t)b[3];
}

int main(void) {
    wubu_theme_init();
    wubu_screenshot_init();

    printf("Testing screenshot subsystem...\n");

    /* Test directory creation */
    const char *dir = wubu_screenshot_get_dir();
    printf("Screenshot dir: %s\n", dir);
    assert(dir && dir[0] != '\0');

    /* Test filename generation */
    char filename[256];
    wubu_screenshot_gen_filename(filename, sizeof(filename));
    printf("Generated filename: %s\n", filename);
    assert(strstr(filename, "Screenshot_") == filename);
    assert(strstr(filename, ".png") != NULL);

    /* Test annotation style */
    wubu_annot_style_t style = {
        .color = 0xFFFF0000,
        .fill_color = 0x8000FF00,
        .thickness = 3,
        .font_size = 12
    };
    assert(style.color == 0xFFFF0000);
    assert(style.thickness == 3);

    /* Test sshot allocation */
    wubu_sshot_t *sshot = calloc(1, sizeof(wubu_sshot_t));
    assert(sshot);
    sshot->width = 100;
    sshot->height = 100;
    sshot->stride = 400;
    sshot->pixels = calloc(100 * 100, 4);
    assert(sshot->pixels);
    /* Fill with a recognizable non-zero pattern so PNG output is non-trivial. */
    for (int i = 0; i < 100 * 100; i++)
        sshot->pixels[i] = 0xFF0000FFu; /* opaque red */

    /* Test annotation add */
    wubu_screenshot_add_annotation(sshot, WUBU_ANNOT_RECT, 10, 10, 50, 50, NULL, &style);
    assert(sshot->annotations != NULL);
    assert(sshot->annotations->tool == WUBU_ANNOT_RECT);
    assert(sshot->annotations->x1 == 10);
    assert(sshot->annotations->x2 == 50);

    /* Test annotation clear */
    wubu_screenshot_clear_annotations(sshot);
    assert(sshot->annotations == NULL);

    /* -- Clipboard: real PNG encode (was a no-op placeholder) ---------- */
    assert(wubu_screenshot_to_clipboard(NULL) == false);          /* NULL rejected */

    int ok = wubu_screenshot_to_clipboard(sshot);
    printf("to_clipboard returned: %d\n", ok);
    assert(ok == true);

    size_t clip_len = 0;
    const uint8_t *clip = wubu_screenshot_clipboard_data(&clip_len);
    assert(clip != NULL);
    assert(clip_len > 8);
    /* PNG magic */
    assert(clip[0] == 0x89 && clip[1] == 'P' && clip[2] == 'N' &&
           clip[3] == 'G' && clip[4] == 0x0D && clip[5] == 0x0A &&
           clip[6] == 0x1A && clip[7] == 0x0A);
    /* IHDR width/height match the screenshot (IHDR data starts at offset 16) */
    uint32_t w = rd_be32(clip + 16);
    uint32_t h = rd_be32(clip + 20);
    printf("clipboard PNG: %u x %u, %zu bytes\n", w, h, clip_len);
    assert(w == 100 && h == 100);
    /* IHDR color type must be 6 (RGBA) */
    assert(clip[25] == 6);

    /* Test free */
    free(sshot->pixels);
    free(sshot);

    wubu_screenshot_shutdown();

    printf("✅ All screenshot tests passed\n");
    return 0;
}