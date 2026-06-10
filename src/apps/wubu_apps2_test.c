/*
 * wubu_apps_test.c — Tests for Editor, Canvas, and Codec
 *
 * Cell 396/397/398: Notepad++ editor, Photoship canvas, FFmpeg codec.
 */
#include "wubu_editor.h"
#include "wubu_canvas.h"
#include "wubu_codec.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static int pass = 0, fail = 0;
#define TEST(name) printf("  TEST: %-60s", name)
#define PASS() do { pass++; printf("✅\n"); } while(0)
#define FAIL(msg) do { fail++; printf("❌ %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ── Editor Tests ───────────────────────────────────────────────── */

static void test_ed_create(void) {
    TEST("editor create");
    WubuEditor *ed = wubu_ed_create();
    CHECK(ed != NULL, "editor should exist");
    CHECK(ed->n_tabs == 0, "should start with no tabs");
    wubu_ed_destroy(ed);
    PASS();
}

static void test_ed_new_file(void) {
    TEST("editor new file");
    WubuEditor *ed = wubu_ed_create();
    wubu_ed_new_file(ed);
    CHECK(ed->n_tabs == 1, "should have 1 tab");
    CHECK(ed->active_tab == 0, "active tab should be 0");
    wubu_ed_destroy(ed);
    PASS();
}

static void test_ed_insert(void) {
    TEST("editor insert + newline");
    WubuEditor *ed = wubu_ed_create();
    wubu_ed_new_file(ed);
    wubu_ed_insert_char(ed, 'H');
    wubu_ed_insert_char(ed, 'i');
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    CHECK(tab->lines[0].len == 2, "line should be 2 chars");
    CHECK(tab->lines[0].text[0] == 'H', "first char is H");
    wubu_ed_insert_newline(ed);
    CHECK(tab->n_lines == 2, "should have 2 lines");
    wubu_ed_insert_char(ed, 'x');
    CHECK(tab->lines[1].text[0] == 'x', "second line starts with x");
    wubu_ed_destroy(ed);
    PASS();
}

static void test_ed_delete(void) {
    TEST("editor backspace");
    WubuEditor *ed = wubu_ed_create();
    wubu_ed_new_file(ed);
    wubu_ed_insert_char(ed, 'A');
    wubu_ed_insert_char(ed, 'B');
    wubu_ed_delete_char(ed);
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    CHECK(tab->lines[0].len == 1, "should be 1 char after delete");
    CHECK(tab->lines[0].text[0] == 'A', "remaining char is A");
    wubu_ed_destroy(ed);
    PASS();
}

static void test_ed_syntax_detect(void) {
    TEST("editor syntax detection");
    CHECK(wubu_ed_detect_syntax("test.c") == SYNTAX_C, ".c → C");
    CHECK(wubu_ed_detect_syntax("test.HC") == SYNTAX_HOLYC, ".HC → HolyC");
    CHECK(wubu_ed_detect_syntax("test.py") == SYNTAX_PYTHON, ".py → Python");
    CHECK(wubu_ed_detect_syntax("test.sh") == SYNTAX_SHELL, ".sh → Shell");
    CHECK(wubu_ed_detect_syntax("Makefile") == SYNTAX_MAKEFILE, "Makefile");
    CHECK(wubu_ed_detect_syntax("test.wubu") == SYNTAX_TOML, ".wubu → TOML");
    CHECK(wubu_ed_detect_syntax("test.json") == SYNTAX_JSON, ".json → JSON");
    PASS();
}

static void test_ed_tabs(void) {
    TEST("editor multiple tabs");
    WubuEditor *ed = wubu_ed_create();
    wubu_ed_new_file(ed);
    wubu_ed_new_file(ed);
    wubu_ed_new_file(ed);
    CHECK(wubu_ed_tab_count(ed) == 3, "should have 3 tabs");
    wubu_ed_switch_tab(ed, 1);
    CHECK(wubu_ed_active_tab(ed) == 1, "active should be 1");
    wubu_ed_close_tab(ed, 1);
    CHECK(wubu_ed_tab_count(ed) == 2, "should have 2 after close");
    wubu_ed_destroy(ed);
    PASS();
}

/* ── Canvas Tests ───────────────────────────────────────────────── */

static void test_cv_create(void) {
    TEST("canvas create 640×480");
    WubuCanvas *cv = wubu_cv_create(640, 480);
    CHECK(cv != NULL, "canvas should exist");
    CHECK(cv->w == 640 && cv->h == 480, "dimensions should match");
    CHECK(cv->n_layers == 1, "should have background layer");
    wubu_cv_destroy(cv);
    PASS();
}

static void test_cv_layers(void) {
    TEST("canvas layer add + opacity + blend");
    WubuCanvas *cv = wubu_cv_create(640, 480);
    int l1 = wubu_cv_layer_add(cv, "Layer 1");
    int l2 = wubu_cv_layer_add(cv, "Layer 2");
    CHECK(cv->n_layers == 3, "bg + 2 layers = 3");
    wubu_cv_layer_set_opacity(cv, l1, 128);
    CHECK(cv->layers[l1].opacity == 128, "opacity should be 128");
    wubu_cv_layer_set_blend(cv, l1, BLEND_MULTIPLY);
    CHECK(cv->layers[l1].blend == BLEND_MULTIPLY, "blend should be multiply");
    wubu_cv_layer_set_visible(cv, l2, false);
    CHECK(cv->layers[l2].visible == false, "layer 2 should be hidden");
    wubu_cv_destroy(cv);
    PASS();
}

static void test_cv_blend_normal(void) {
    TEST("canvas blend: normal mode");
    uint32_t dst = 0x00FF0000; /* Red */
    uint32_t src = 0x0000FF00; /* Green */
    uint32_t result = wubu_blend(dst, src, 255, BLEND_NORMAL);
    CHECK(result == src, "normal at full opacity = src");
    PASS();
}

static void test_cv_blend_multiply(void) {
    TEST("canvas blend: multiply mode");
    uint32_t white = 0x00FFFFFF;
    uint32_t red = 0x00FF0000;
    uint32_t result = wubu_blend(white, red, 255, BLEND_MULTIPLY);
    /* multiply with white keeps the color */
    CHECK((result & 0xFF) == (red & 0xFF), "multiply: R channel preserved with white");
    PASS();
}

static void test_cv_brush(void) {
    TEST("canvas brush paint");
    WubuCanvas *cv = wubu_cv_create(100, 100);
    cv->tool.fg_color = 0x00FF0000;
    cv->tool.brush_size = 3;
    wubu_cv_brush(cv, 50, 50);
    uint32_t px = wubu_cv_pick(cv, 50, 50);
    CHECK(px == 0x00FF0000, "center pixel should be red");
    wubu_cv_destroy(cv);
    PASS();
}

static void test_cv_save_bmp(void) {
    TEST("canvas save BMP");
    WubuCanvas *cv = wubu_cv_create(64, 64);
    int ret = wubu_cv_save_bmp(cv, "/tmp/wubu-test.bmp");
    CHECK(ret == 0, "BMP save should succeed");
    /* Verify file exists */
    FILE *f = fopen("/tmp/wubu-test.bmp", "rb");
    CHECK(f != NULL, "BMP file should exist");
    if (f) fclose(f);
    unlink("/tmp/wubu-test.bmp");
    wubu_cv_destroy(cv);
    PASS();
}

static void test_cv_plugin(void) {
    TEST("canvas plugin register + run");
    WubuCanvas *cv = wubu_cv_create(100, 100);
    /* Register a no-op plugin */
    WubuPlugin p = {0};
    strncpy(p.name, "test-plugin", sizeof(p.name));
    p.active = true;
    int idx = wubu_cv_plugin_register(cv, &p);
    CHECK(idx >= 0, "plugin should register");
    CHECK(cv->n_plugins == 1, "should have 1 plugin");
    wubu_cv_plugin_unregister(cv, idx);
    CHECK(cv->n_plugins == 0, "should have 0 after unregister");
    wubu_cv_destroy(cv);
    PASS();
}

static void test_cv_zoom_phi(void) {
    TEST("canvas zoom uses φ scale");
    WubuCanvas *cv = wubu_cv_create(640, 480);
    CHECK(cv->zoom == 1.0, "initial zoom = 1.0");
    wubu_cv_zoom_in(cv);
    CHECK(cv->zoom > 1.6 && cv->zoom < 1.62, "zoom in = ×φ ≈ 1.618");
    wubu_cv_zoom_out(cv);
    CHECK(cv->zoom > 0.99 && cv->zoom < 1.01, "zoom out restores ≈ 1.0");
    wubu_cv_destroy(cv);
    PASS();
}

/* ── Codec Tests ────────────────────────────────────────────────── */

static void test_codec_detect(void) {
    TEST("codec format detection");
    CHECK(wubu_codec_detect_type("video.mp4") == WUBU_MEDIA_VIDEO, "mp4");
    CHECK(wubu_codec_detect_type("audio.mp3") == WUBU_MEDIA_AUDIO, "mp3");
    CHECK(wubu_codec_detect_type("image.png") == WUBU_MEDIA_IMAGE, "png");
    CHECK(wubu_codec_detect_type("video.webm") == WUBU_MEDIA_VIDEO, "webm");
    CHECK(wubu_codec_detect_type("audio.opus") == WUBU_MEDIA_AUDIO, "opus");
    CHECK(wubu_codec_detect_type("image.gif") == WUBU_MEDIA_IMAGE, "gif");
    PASS();
}

static void test_codec_available(void) {
    TEST("codec ffmpeg available check");
    /* This is environment-dependent, just verify it doesn't crash */
    bool avail = wubu_codec_available();
    printf("(%s) ", avail ? "yes" : "no");
    PASS();
}

/* ── Main ───────────────────────────────────────────────────────── */

int main(void) {
    printf("\n── Code Editor (Cell 396) ──\n\n");
    test_ed_create();
    test_ed_new_file();
    test_ed_insert();
    test_ed_delete();
    test_ed_syntax_detect();
    test_ed_tabs();

    printf("\n── Image Canvas (Cell 397) ──\n\n");
    test_cv_create();
    test_cv_layers();
    test_cv_blend_normal();
    test_cv_blend_multiply();
    test_cv_brush();
    test_cv_save_bmp();
    test_cv_plugin();
    test_cv_zoom_phi();

    printf("\n── Codec Layer (Cell 398) ──\n\n");
    test_codec_detect();
    test_codec_available();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n",
           pass, pass + fail, fail);
    printf("══════════════════════════════════════════════════\n");
    return fail > 0 ? 1 : 0;
}
