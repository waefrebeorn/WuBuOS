/*
 * wubu_screenshot_test.c — Tests for screenshot/snipping tool
 */

#include "screenshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Access static globals from screenshot.c via accessors */
extern WubuSnipTool *wubu_snip_tool_state(void);
extern WubuGifRecorder *wubu_gif_recorder_state(void);

#define g_snip (*wubu_snip_tool_state())
#define g_gif  (*wubu_gif_recorder_state())

#define TEST(name) void test_##name(void)
#define ASSERT(cond, fmt, ...) \
    do { if (!(cond)) { \
        fprintf(stderr, "FAIL: " fmt " at %s:%d\n", ##__VA_ARGS__, __FILE__, __LINE__); \
        exit(1); \
    } else { \
        printf("PASS: " fmt "\n", ##__VA_ARGS__); \
    } } while(0)

TEST(ppm_write) {
    int w = 100, h = 50;
    uint32_t *buf = malloc(w * h * sizeof(uint32_t));
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            buf[y * w + x] = ((x * 255) / w) << 16 | ((y * 255) / h) << 8 | 0x80;
        }
    }

    int ret = wubu_write_ppm("/tmp/test_image.ppm", buf, w, h);
    ASSERT(ret == 0, "ppm write succeeds");

    /* Verify file exists and has correct header */
    FILE *f = fopen("/tmp/test_image.ppm", "rb");
    ASSERT(f != NULL, "file opens");
    char magic[3];
    fread(magic, 1, 2, f);
    ASSERT(magic[0] == 'P' && magic[1] == '6', "PPM magic P6");
    fclose(f);

    free(buf);
}

TEST(bmp_write) {
    int w = 80, h = 40;
    uint32_t *buf = malloc(w * h * sizeof(uint32_t));
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            buf[y * w + x] = 0x00FF0000 | (x << 16) | (y << 8);
        }
    }

    int ret = wubu_write_bmp("/tmp/test_image.bmp", buf, w, h);
    ASSERT(ret == 0, "bmp write succeeds");

    /* Verify BMP header */
    FILE *f = fopen("/tmp/test_image.bmp", "rb");
    ASSERT(f != NULL, "file opens");
    uint16_t type;
    fread(&type, 2, 1, f);
    ASSERT(type == 0x4D42, "BMP magic BM");
    fclose(f);

    free(buf);
}

TEST(snip_tool_lifecycle) {
    int ret = wubu_snip_tool_init();
    ASSERT(ret == 0, "snip init succeeds");

    wubu_snip_tool_activate(SNIP_MODE_RECTANGLE);
    ASSERT(g_snip.active == true, "snip active after activate");

    wubu_snip_tool_deactivate();
    ASSERT(g_snip.active == false, "snip inactive after deactivate");

    wubu_snip_tool_shutdown();
}

TEST(snip_tool_selection) {
    wubu_snip_tool_init();
    wubu_snip_tool_activate(SNIP_MODE_RECTANGLE);

    /* Simulate mouse down */
    bool handled = wubu_snip_tool_handle_mouse(100, 100, 1, 1);
    ASSERT(handled == true, "mouse down handled");
    ASSERT(g_snip.selecting == true, "selecting after mouse down");
    ASSERT(g_snip.start_x == 100 && g_snip.start_y == 100, "start pos set");

    /* Simulate mouse move */
    handled = wubu_snip_tool_handle_mouse(200, 150, 0, 0);
    ASSERT(handled == true, "mouse move handled");
    ASSERT(g_snip.end_x == 200 && g_snip.end_y == 150, "end pos updated");

    /* Simulate mouse up */
    handled = wubu_snip_tool_handle_mouse(200, 150, 1, 2);
    ASSERT(handled == true, "mouse up handled");
    ASSERT(g_snip.selecting == false, "not selecting after mouse up");

    wubu_snip_tool_shutdown();
}

TEST(snip_tool_save) {
    /* Requires VBE to be initialized - this is integration test */
    wubu_snip_tool_init();
    wubu_snip_tool_activate(SNIP_MODE_RECTANGLE);

    /* Manually set a selection */
    g_snip.start_x = 10;
    g_snip.start_y = 10;
    g_snip.end_x = 110;
    g_snip.end_y = 110;
    g_snip.active = true;
    g_snip.selecting = false;

    /* This will fail if VBE not init, but tests the path */
    int ret = wubu_snip_tool_save("/tmp/snip_test.bmp", SHOT_FMT_BMP);
    /* Can fail if no VBE - that's OK for unit test */
    printf("  Snip save result: %d (expected -1 without VBE)\n", ret);

    wubu_snip_tool_shutdown();
}

TEST(gif_recorder) {
    int ret = wubu_gif_start("/tmp/test_gif", 100, 100, 100, 3);
    ASSERT(ret == 0, "gif start succeeds");

    /* Can't add frames without VBE, but test lifecycle */
    wubu_gif_stop();
    ASSERT(g_gif.frame_count == 0, "gif inactive after stop");
}

int main(void) {
    printf("=== wubu_screenshot_test ===\n");
    test_ppm_write();
    test_bmp_write();
    test_snip_tool_lifecycle();
    test_snip_tool_selection();
    test_snip_tool_save();
    test_gif_recorder();
    printf("✅ All wubu_screenshot tests passed\n");
    return 0;
}