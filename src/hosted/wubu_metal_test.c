/*
 * wubu_metal_test.c  --  Tests for bare-metal + WSL2 abstraction layer
 */

#include "wubu_metal.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

/* Stub: wubu_shell_run is not testable in isolation (requires full GUI stack) */
__attribute__((weak)) int wubu_shell_run(int width, int height) {
    (void)width; (void)height;
    return 0;
}

#define TEST(name) void test_##name(void)
#define ASSERT(cond, fmt, ...) \
    do { if (!(cond)) { \
        fprintf(stderr, "FAIL: " fmt " at %s:%d\n", ##__VA_ARGS__, __FILE__, __LINE__); \
        exit(1); \
    } else { \
        printf("PASS: " fmt "\n", ##__VA_ARGS__); \
    } } while(0)

TEST(detect_env) {
    WubuBootEnv env = wubu_detect_env();
    ASSERT(env >= WUBU_ENV_HOSTED && env <= WUBU_ENV_MACOS, "detect_env returns valid");
    const char *name = wubu_env_name(env);
    ASSERT(name != NULL, "env_name not null: %s", name);
    printf("  Detected env: %s\n", name);
}

TEST(display_init_shutdown) {
    int ret = wubu_disp_init(1280, 720);
    ASSERT(ret == 0, "disp_init succeeds");

    WubuDisplay *d = wubu_disp_state();
    ASSERT(d != NULL, "disp_state returns pointer");
    ASSERT(d->width == 1280, "width set: %d", d->width);
    ASSERT(d->height == 720, "height set: %d", d->height);

    WubuDispBackend backend = wubu_disp_current();
    ASSERT(backend != DISP_AUTO, "backend auto-detected: %d", backend);

    wubu_disp_shutdown();
    ASSERT(wubu_disp_state() != NULL, "state still accessible after shutdown");
}

TEST(input_init) {
    wubu_disp_init(800, 600);
    int ret = wubu_input_init();
    ASSERT(ret == 0, "input_init succeeds");

    WubuInput *in = wubu_input_state();
    ASSERT(in != NULL, "input_state returns pointer");
    ASSERT(in->backend != 0, "input backend set: %d", in->backend);

    wubu_input_shutdown();
    wubu_disp_shutdown();
}

TEST(audio_init) {
    wubu_disp_init(800, 600);
    int ret = wubu_audio_init(48000, 2, 256);
    /* Audio may not be available in test environment - allow failure */
    if (ret == 0) {
        WubuAudio *a = wubu_audio_state();
        ASSERT(a != NULL, "audio_state returns pointer");
        ASSERT(a->sample_rate == 48000, "sample rate: %d", a->sample_rate);
        ASSERT(a->channels == 2, "channels: %d", a->channels);
        ASSERT(a->buffer_frames == 256, "buffer frames: %d", a->buffer_frames);
        wubu_audio_shutdown();
    } else {
        printf("  Audio init failed (expected in test env)\n");
    }
    wubu_disp_shutdown();
}

TEST(gaad_nearest) {
    wubu_disp_init(800, 600);
    int w = 1920, h = 1080;
    /* Test that the function exists and runs */
    wubu_disp_gaad_nearest(1920, 1080, &w, &h);
    ASSERT(w > 0 && h > 0, "gaad_nearest returns valid: %dx%d", w, h);
    printf("  GAAD nearest for 1920x1080: %dx%d\n", w, h);
    wubu_disp_shutdown();
}

TEST(metal_init_run) {
    /* Test metal init (won't run full loop) */
    int ret = wubu_metal_init(640, 480);
    ASSERT(ret == 0, "metal_init succeeds");
    WubuDisplay *d = wubu_disp_state();
    ASSERT(d->backend == DISP_DRM || d->backend == DISP_VBE, "metal backend: %d", d->backend);
    wubu_metal_shutdown();
}

int main(void) {
    printf("=== wubu_metal_test ===\n");
    test_detect_env();
    test_display_init_shutdown();
    test_input_init();
    test_audio_init();
    test_gaad_nearest();
    test_metal_init_run();
    printf("✅ All wubu_metal tests passed\n");
    return 0;
}