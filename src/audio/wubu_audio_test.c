/*
 * wubu_audio_test.c — Tests for audio engine (DAW + Furnace + SF2 + AI)
 */

#include "wubu_audio.h"
#include "wubu_math.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define TEST(name) void test_##name(void)
#define ASSERT(cond, fmt, ...) \
    do { if (!(cond)) { \
        fprintf(stderr, "FAIL: " fmt " at %s:%d\n", ##__VA_ARGS__, __FILE__, __LINE__); \
        exit(1); \
    } else { \
        printf("PASS: " fmt "\n", ##__VA_ARGS__); \
    } } while(0)

TEST(math_functions) {
    double x = 1.0;
    double s, c;
    wubu_sincos(x, &s, &c);
    ASSERT(s > 0.84 && s < 0.85, "sin(1) ≈ 0.841");
    ASSERT(c > 0.54 && c < 0.55, "cos(1) ≈ 0.540");

    ASSERT(wubu_sqrt(4.0) == 2.0, "sqrt(4) = 2");
    ASSERT(wubu_sqrt(2.0) > 1.414 && wubu_sqrt(2.0) < 1.415, "sqrt(2) ≈ 1.414");
    ASSERT(wubu_exp(1.0) > 2.718 && wubu_exp(1.0) < 2.719, "exp(1) ≈ e");
    ASSERT(wubu_log(WUBU_E) > 0.999 && wubu_log(WUBU_E) < 1.001, "log(e) = 1");
    ASSERT(wubu_pow(2.0, 3.0) > 7.99 && wubu_pow(2.0, 3.0) < 8.01, "2^3 = 8");
}

TEST(engine_lifecycle) {
    int ret = wubu_audio_engine_create(48000, 256, 2);
    ASSERT(ret == 0, "engine_create succeeds");

    WubuAudioEngine *e = wubu_audio_engine();
    ASSERT(e != NULL, "engine() returns pointer");
    ASSERT(e->sample_rate == 48000, "sample rate: %d", e->sample_rate);
    ASSERT(e->buffer_frames == 256, "buffer frames: %d", e->buffer_frames);
    ASSERT(e->channels == 2, "channels: %d", e->channels);

    ret = wubu_audio_start();
    ASSERT(ret == 0, "audio_start succeeds");
    ASSERT(e->playing == true, "playing = true");

    wubu_audio_stop();
    ASSERT(e->playing == false, "playing = false after stop");

    wubu_audio_engine_destroy();
}

TEST(daw_tracks) {
    wubu_audio_engine_create(48000, 256, 2);

    int t1 = wubu_daw_add_track("Audio 1", TRACK_AUDIO);
    ASSERT(t1 == 0, "add track 0");
    int t2 = wubu_daw_add_track("Furnace", TRACK_FURNACE);
    ASSERT(t2 == 1, "add track 1");
    int t3 = wubu_daw_add_track("SF2", TRACK_SF2);
    ASSERT(t3 == 2, "add track 2");

    ASSERT(wubu_daw_track_count() == 3, "track count = 3");

    WubuAudioEngine *e = wubu_audio_engine();
    ASSERT(strcmp(e->tracks[0].name, "Audio 1") == 0, "track 0 name");
    ASSERT(e->tracks[1].type == TRACK_FURNACE, "track 1 type");
    ASSERT(e->tracks[2].type == TRACK_SF2, "track 2 type");

    wubu_daw_remove_track(1);
    ASSERT(wubu_daw_track_count() == 2, "track count after remove = 2");

    wubu_audio_engine_destroy();
}

TEST(transport) {
    wubu_audio_engine_create(48000, 256, 2);
    wubu_daw_add_track("Test", TRACK_AUDIO);

    wubu_daw_play();
    WubuAudioEngine *e = wubu_audio_engine();
    ASSERT(e->playing == true, "playing after play");

    wubu_daw_seek(10.5);
    ASSERT(e->playhead == 10.5, "seek to 10.5");

    wubu_daw_stop();
    ASSERT(e->playing == false, "not playing after stop");
    ASSERT(e->playhead == 0.0, "playhead reset to 0");

    wubu_audio_engine_destroy();
}

TEST(furnace_init) {
    wubu_audio_engine_create(48000, 256, 2);

    WubuChipType chips[] = { CHIP_NES_APU, CHIP_SN76489, CHIP_SID };
    int ret = wubu_furnace_init(3, chips);
    ASSERT(ret == 0, "furnace_init succeeds");

    WubuAudioEngine *e = wubu_audio_engine();
    ASSERT(e->furnace.n_chips == 3, "3 chips");
    ASSERT(e->furnace.chips[0] == CHIP_NES_APU, "chip 0 = NES_APU");
    ASSERT(e->furnace.chips[1] == CHIP_SN76489, "chip 1 = SN76489");
    ASSERT(e->furnace.chips[2] == CHIP_SID, "chip 2 = SID");
    ASSERT(e->furnace_active == true, "furnace active");

    wubu_furnace_shutdown();
    ASSERT(e->furnace_active == false, "furnace inactive after shutdown");

    wubu_audio_engine_destroy();
}

TEST(furnace_pattern) {
    wubu_audio_engine_create(48000, 256, 2);
    WubuChipType chips[] = { CHIP_NES_APU };
    wubu_furnace_init(1, chips);

    /* Set up a simple pattern: C-4, F-4, G-4, C-5 */
    wubu_furnace_set_note(0, 0, 0, 0, 4);  /* C-4 */
    wubu_furnace_set_note(0, 1, 0, 5, 4);  /* F-4 (note 5 = F) */
    wubu_furnace_set_note(0, 2, 0, 7, 4);  /* G-4 */
    wubu_furnace_set_note(0, 3, 0, 0, 5);  /* C-5 */

    /* Set volume on all notes (0-15) */
    for (int row = 0; row < 4; row++) {
        wubu_furnace_set_volume(0, row, 0, 15);
    }
    wubu_furnace_set_inst(0, 0, 0, 0);
    wubu_furnace_set_inst(0, 1, 0, 0);

    float out[1024];
    int rendered = wubu_furnace_render_pattern(0, out, 1024);
    ASSERT(rendered == 1024, "rendered 1024 frames");

    /* Check that output has non-zero content */
    float max = 0.0f;
    for (int i = 0; i < 1024; i++) {
        float abs_val = out[i] < 0 ? -out[i] : out[i];
        if (abs_val > max) max = abs_val;
    }
    ASSERT(max > 0.01f, "output has signal: max=%f", max);

    wubu_furnace_shutdown();
    wubu_audio_engine_destroy();
}

TEST(sf2_basic) {
    wubu_audio_engine_create(48000, 256, 2);

    /* Create minimal SF2 data (RIFF header only) */
    uint8_t minimal_sf2[512] = {0};
    memcpy(minimal_sf2, "RIFF", 4);
    *(uint32_t*)(minimal_sf2 + 4) = 500; /* size */
    memcpy(minimal_sf2 + 8, "sfbk", 4);
    *(uint32_t*)(minimal_sf2 + 12) = 0x200; /* LIST */
    *(uint32_t*)(minimal_sf2 + 16) = 0; /* dummy */

    int ret = wubu_sf2_load(minimal_sf2, 512);
    ASSERT(ret == 0, "sf2_load succeeds");

    WubuAudioEngine *e = wubu_audio_engine();
    ASSERT(e->sf2_active == true, "sf2 active");
    ASSERT(e->sf2.n_presets > 0, "has presets: %d", e->sf2.n_presets);

    const WubuSF2Preset *p = wubu_sf2_preset(0);
    ASSERT(p != NULL, "preset 0 exists");
    ASSERT(strlen(p->name) > 0, "preset has name: %s", p->name);

    wubu_sf2_note_on(0, 60, 100); /* Middle C */
    wubu_sf2_note_on(0, 64, 100); /* E */
    wubu_sf2_note_on(0, 67, 100); /* G */

    float out[1024]; /* 512 frames * 2 channels */
    wubu_sf2_render(out, 512, 2);

    float max = 0.0f;
    for (int i = 0; i < 1024; i++) {
        float abs_val = out[i] < 0 ? -out[i] : out[i];
        if (abs_val > max) max = abs_val;
    }
    ASSERT(max > 0.0f, "sf2 renders signal: max=%f", max);

    wubu_sf2_note_off(0, 60);
    wubu_sf2_note_off(0, 64);
    wubu_sf2_note_off(0, 67);

    wubu_sf2_unload();
    ASSERT(e->sf2_active == false, "sf2 inactive after unload");

    wubu_audio_engine_destroy();
}

TEST(ai_plugins) {
    wubu_audio_engine_create(48000, 256, 2);

    int idx = wubu_ai_plugin_register("Separator", AI_PLUGIN_SEPARATION, "/models/separator.wubu");
    ASSERT(idx == 0, "register plugin idx 0");

    idx = wubu_ai_plugin_register("Mastering", AI_PLUGIN_MASTERING, "/models/mastering.wubu");
    ASSERT(idx == 1, "register plugin idx 1");

    WubuAudioEngine *e = wubu_audio_engine();
    ASSERT(e->n_ai_plugins == 2, "2 plugins registered");

    int start_ret = wubu_ai_plugin_start(0);
    ASSERT(start_ret == 0, "start plugin 0");
    ASSERT(e->ai_plugins[0].active == true, "plugin 0 active");

    float in[256] = {0}, out[256] = {0};
    in[0] = 0.5f; in[1] = 0.5f;
    int processed = wubu_ai_plugin_process(0, in, out, 2, 2);
    ASSERT(processed > 0, "plugin processed frames");

    wubu_ai_plugin_stop(0);
    ASSERT(e->ai_plugins[0].active == false, "plugin 0 stopped");

    wubu_audio_engine_destroy();
}

TEST(ingestion) {
    int n = wubu_ingest_enumerate(NULL, NULL, 0);
    ASSERT(n >= 0, "enumerate returns count: %d", n);

    char uris[16][256];
    char names[16][64];
    n = wubu_ingest_enumerate(uris, names, 16);
    ASSERT(n > 0, "enumerate returns sources: %d", n);
    for (int i = 0; i < n; i++) {
        printf("  Source %d: %s (%s)\n", i, names[i], uris[i]);
    }

    int handle = wubu_ingest_open("alsa:seq");
    if (handle >= 0) {
        uint8_t buf[64];
        int read = wubu_ingest_read(handle, buf, 64);
        ASSERT(read >= 0, "read returns: %d", read);
        wubu_ingest_close(handle);
    }

    /* Test file ingestion */
    handle = wubu_ingest_open("file:/proc/version");
    if (handle >= 0) {
        char buf[64];
        int read = wubu_ingest_read(handle, (uint8_t*)buf, 63);
        ASSERT(read >= 0, "file read: %d bytes", read);
        wubu_ingest_close(handle);
    }
}

TEST(audio_process_integration) {
    wubu_audio_engine_create(48000, 256, 2);
    wubu_daw_add_track("Master", TRACK_MASTER);

    wubu_audio_start();
    WubuAudioEngine *e = wubu_audio_engine();

    float out[512]; /* 256 frames * 2 channels */
    wubu_audio_process(out, 256);

    ASSERT(e->playhead > 0.0, "playhead advanced: %f", e->playhead);

    wubu_audio_stop();
    wubu_audio_engine_destroy();
}

int main(void) {
    printf("=== wubu_audio_test ===\n");
    printf("Running test_math_functions...\n"); fflush(stdout);
    test_math_functions();
    printf("Running test_engine_lifecycle...\n"); fflush(stdout);
    test_engine_lifecycle();
    printf("Running test_daw_tracks...\n"); fflush(stdout);
    test_daw_tracks();
    printf("Running test_transport...\n"); fflush(stdout);
    test_transport();
    printf("Running test_furnace_init...\n"); fflush(stdout);
    test_furnace_init();
    printf("Running test_furnace_pattern...\n"); fflush(stdout);
    test_furnace_pattern();
    printf("Running test_sf2_basic...\n"); fflush(stdout);
    test_sf2_basic();
    printf("Running test_ai_plugins...\n"); fflush(stdout);
    test_ai_plugins();
    printf("Running test_ingestion...\n"); fflush(stdout);
    test_ingestion();
    printf("Running test_audio_process_integration...\n"); fflush(stdout);
    test_audio_process_integration();
    printf("✅ All wubu_audio tests passed\n");
    return 0;
}