#include "wubu_screenshot.h"
#include "wubu_theme.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

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

    /* Test annotation add */
    wubu_screenshot_add_annotation(sshot, WUBU_ANNOT_RECT, 10, 10, 50, 50, NULL, &style);
    assert(sshot->annotations != NULL);
    assert(sshot->annotations->tool == WUBU_ANNOT_RECT);
    assert(sshot->annotations->x1 == 10);
    assert(sshot->annotations->x2 == 50);

    /* Test annotation clear */
    wubu_screenshot_clear_annotations(sshot);
    assert(sshot->annotations == NULL);

    /* Test free */
    free(sshot->pixels);
    free(sshot);

    wubu_screenshot_shutdown();

    printf("✅ All screenshot tests passed\n");
    return 0;
}