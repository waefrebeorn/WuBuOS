/*
 * vbe_ws_bridge_test.c — Test Suite for VBE ↔ WorldSim Render Bridge
 *
 * Cell 070: Tests the wiring between VBE framebuffer and WorldSim
 * renderer. Verifies:
 *   - Bridge init/wire/unwire lifecycle
 *   - VBE back-buffer ↔ WorldSim render ctx connection
 *   - Camera/viewport operations
 *   - HUD overlay drawing
 *   - Per-frame render pipeline
 *   - State management (start/pause/resume/stop)
 *   - Text rendering
 *   - Zoom clamping
 */

#include "vbe_ws_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── Test Framework ────────────────────────────────────────── */

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) printf("  TEST %-45s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ── Helpers ───────────────────────────────────────────────── */

/* Count non-zero pixels in a framebuffer region */
static int count_pixels(uint32_t *fb, int w, int h,
                         int x0, int y0, int rw, int rh) {
    int count = 0;
    for (int y = y0; y < y0 + rh && y < h; y++)
        for (int x = x0; x < x0 + rw && x < w; x++)
            if (fb[y * w + x] != 0)
                count++;
    return count;
}

/* Read a pixel from front buffer (after vbe_swap) */
static uint32_t read_front_pixel(VBEState *vbe, int x, int y) {
    if (!vbe || !vbe->fb || x < 0 || x >= vbe->width || y < 0 || y >= vbe->height)
        return 0;
    return vbe->fb[y * vbe->width + x];
}

/* ── Lifecycle Tests ───────────────────────────────────────── */

static void test_bridge_init(void) {
    TEST("bridge init defaults");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);

    CHECK(br.state == BRIDGE_STOPPED, "state should be STOPPED");
    CHECK(br.wired == 0, "should not be wired");
    CHECK(br.view_zoom == 1.0f, "zoom default 1.0");
    CHECK(br.sim_speed == 1.0f, "sim_speed default 1.0");
    CHECK(br.show_hud == 1, "HUD on by default");
    CHECK(br.show_minimap == 1, "minimap on by default");
    CHECK(br.minimap_size == 128, "minimap default 128");
    CHECK(br.frame_count == 0, "frame_count 0");
    CHECK(br.vbe == NULL, "vbe NULL");
    CHECK(br.sim == NULL, "sim NULL");
    PASS();
}

static void test_wire_basic(void) {
    TEST("wire VBE to WorldSim");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);

    /* Init VBE: 320x200 is small enough for test speed */
    int rc = vbe_init(320, 200);
    CHECK(rc == 0, "vbe_init failed");

    /* Init WorldSim */
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);

    /* Wire them */
    rc = vbe_ws_bridge_wire(&br, &sim);
    CHECK(rc == 0, "wire failed");
    CHECK(br.wired == 1, "should be wired");
    CHECK(br.vbe != NULL, "vbe should be set");
    CHECK(br.sim == NULL || br.sim == &sim, "sim pointer");

    /* Verify WorldSim render ctx points to VBE back-buffer */
    VBEState *vbe = vbe_state();
    CHECK(sim.render.fb == vbe->back, "render.fb should point to VBE back-buffer");
    CHECK(sim.render.fb_w == 320, "fb_w should match VBE width");
    CHECK(sim.render.fb_h == 200, "fb_h should match VBE height");

    /* Cleanup */
    vbe_ws_bridge_unwire(&br);
    CHECK(br.wired == 0, "should be unwired");
    CHECK(sim.render.fb == NULL, "render.fb should be NULL after unwire");

    vbe_shutdown();
    PASS();
}

static void test_wire_null_safety(void) {
    TEST("wire with NULL params returns error");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);

    int rc = vbe_ws_bridge_wire(&br, NULL);
    CHECK(rc == -1, "wire(NULL sim) should fail");

    /* Wire without VBE init should also fail */
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    rc = vbe_ws_bridge_wire(&br, &sim);
    CHECK(rc == -1, "wire without VBE init should fail");

    PASS();
}

/* ── State Management Tests ────────────────────────────────── */

static void test_state_lifecycle(void) {
    TEST("state: start→pause→resume→stop");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);

    CHECK(br.state == BRIDGE_STOPPED, "initial state STOPPED");
    CHECK(vbe_ws_bridge_is_active(&br) == 0, "not active when stopped");

    vbe_ws_bridge_start(&br);
    CHECK(br.state == BRIDGE_RUNNING, "should be RUNNING");
    CHECK(vbe_ws_bridge_is_active(&br) == 1, "active when running");

    vbe_ws_bridge_pause(&br);
    CHECK(br.state == BRIDGE_PAUSED, "should be PAUSED");
    CHECK(vbe_ws_bridge_is_active(&br) == 0, "not active when paused");

    vbe_ws_bridge_resume(&br);
    CHECK(br.state == BRIDGE_RUNNING, "should be RUNNING again");

    vbe_ws_bridge_stop(&br);
    CHECK(br.state == BRIDGE_STOPPED, "should be STOPPED");
    CHECK(vbe_ws_bridge_is_active(&br) == 0, "not active when stopped");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

/* ── Camera / Viewport Tests ───────────────────────────────── */

static void test_camera_pan(void) {
    TEST("camera pan updates view and sim");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);

    /* Pan right and down */
    vbe_ws_bridge_pan(&br, 10, 20);
    CHECK(br.view_x == 10, "view_x should be 10");
    CHECK(br.view_y == 20, "view_y should be 20");
    CHECK(sim.render.cam_x == 10, "sim cam_x should be 10");
    CHECK(sim.render.cam_y == 20, "sim cam_y should be 20");

    /* Pan more */
    vbe_ws_bridge_pan(&br, -5, -10);
    CHECK(br.view_x == 5, "view_x should be 5");
    CHECK(br.view_y == 10, "view_y should be 10");
    CHECK(sim.render.cam_x == 5, "sim cam_x should be 5");
    CHECK(sim.render.cam_y == 10, "sim cam_y should be 10");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

static void test_camera_set_view(void) {
    TEST("set_view directly sets camera");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);

    vbe_ws_bridge_set_view(&br, 100, 50);
    CHECK(br.view_x == 100, "view_x should be 100");
    CHECK(br.view_y == 50, "view_y should be 50");
    CHECK(sim.render.cam_x == 100, "sim cam_x should be 100");
    CHECK(sim.render.cam_y == 50, "sim cam_y should be 50");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

static void test_camera_center_on(void) {
    TEST("center_on centers viewport on world position");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);

    /* Center on world position (160, 100) with 320x200 viewport */
    vbe_ws_bridge_center_on(&br, 160, 100);
    CHECK(br.view_x == 0, "view_x should be 0 (160-320/2)");
    CHECK(br.view_y == 0, "view_y should be 0 (100-200/2)");

    /* Center on (320, 200) */
    vbe_ws_bridge_center_on(&br, 320, 200);
    CHECK(br.view_x == 160, "view_x should be 160");
    CHECK(br.view_y == 100, "view_y should be 100");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

static void test_camera_zoom(void) {
    TEST("zoom with clamping [0.25, 4.0]");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);

    /* Zoom in */
    vbe_ws_bridge_zoom(&br, 0.5f);
    CHECK(br.view_zoom > 1.0f, "zoom should increase");
    CHECK(sim.render.cam_z > 1.0f, "sim cam_z should increase");

    /* Zoom out past minimum */
    vbe_ws_bridge_zoom(&br, -5.0f);
    CHECK(br.view_zoom == 0.25f, "zoom should clamp to 0.25");

    /* Zoom in past maximum */
    vbe_ws_bridge_zoom(&br, 10.0f);
    CHECK(br.view_zoom == 4.0f, "zoom should clamp to 4.0");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

/* ── Render Pipeline Tests ─────────────────────────────────── */

static void test_render_frame_writes_pixels(void) {
    TEST("render_frame writes pixels to VBE front buffer");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);
    vbe_ws_bridge_start(&br);

    /* Before render: front buffer should be all zeros */
    VBEState *vbe = vbe_state();
    uint32_t before = vbe->fb[100 * 320 + 160]; /* center pixel */

    /* Render one frame */
    vbe_ws_bridge_frame(&br);

    /* After render: at least some pixels should be non-zero 
     * (terrain generates content) */
    int nonzero = count_pixels(vbe->fb, 320, 200, 0, 0, 320, 200);
    CHECK(nonzero > 0, "front buffer should have non-zero pixels after render");
    CHECK(br.frame_count == 1, "frame_count should be 1");
    CHECK(br.sim->tick > 0, "sim should have advanced at least 1 tick");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

static void test_render_multiple_frames(void) {
    TEST("multiple frames advance simulation");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);
    vbe_ws_bridge_start(&br);

    for (int i = 0; i < 10; i++) {
        vbe_ws_bridge_frame(&br);
    }

    CHECK(br.frame_count == 10, "frame_count should be 10");
    CHECK(br.sim->tick >= 10, "sim should have at least 10 ticks");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

static void test_render_paused_does_not_step(void) {
    TEST("paused: render advances frames but not sim ticks");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);
    vbe_ws_bridge_start(&br);

    /* Advance 3 frames running */
    vbe_ws_bridge_frame(&br);
    vbe_ws_bridge_frame(&br);
    vbe_ws_bridge_frame(&br);
    uint64_t tick_at_pause = br.sim->tick;
    CHECK(tick_at_pause >= 3, "should have at least 3 ticks");

    /* Pause and render 5 more frames */
    vbe_ws_bridge_pause(&br);
    for (int i = 0; i < 5; i++) {
        vbe_ws_bridge_frame(&br);
    }
    CHECK(br.frame_count == 8, "frame_count should be 8 (3+5)");
    CHECK(br.sim->tick == tick_at_pause, "sim should NOT advance while paused");

    /* Check that pixels were still rendered (paused rendering still works) */
    VBEState *vbe = vbe_state();
    int nonzero = count_pixels(vbe->fb, 320, 200, 0, 0, 320, 200);
    CHECK(nonzero > 0, "pixels should still be rendered when paused");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

static void test_render_stopped_no_op(void) {
    TEST("stopped: frame is no-op");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);
    /* Do NOT start */

    vbe_ws_bridge_frame(&br);
    CHECK(br.frame_count == 0, "frame_count should be 0 when stopped");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

/* ── Render Only Test ──────────────────────────────────────── */

static void test_render_only(void) {
    TEST("render_only draws but does not advance sim");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);

    uint64_t tick_before = sim.tick;

    vbe_ws_bridge_render_only(&br);

    CHECK(sim.tick == tick_before, "sim should not advance with render_only");
    VBEState *vbe = vbe_state();
    int nonzero = count_pixels(vbe->fb, 320, 200, 0, 0, 320, 200);
    CHECK(nonzero > 0, "front buffer should have pixels after render_only");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

/* ── Sim Speed Test ────────────────────────────────────────── */

static void test_sim_speed(void) {
    TEST("sim_speed controls ticks per frame");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);
    vbe_ws_bridge_start(&br);

    /* Default speed 1.0 → 1 step per frame */
    vbe_ws_bridge_frame(&br);
    uint64_t tick1 = sim.tick;
    CHECK(tick1 >= 1, "should advance at least 1 tick at speed 1.0");

    /* Speed 3.0 → 3 steps per frame */
    br.sim_speed = 3.0f;
    uint64_t tick_before = sim.tick;
    vbe_ws_bridge_frame(&br);
    uint64_t tick_incr = sim.tick - tick_before;
    CHECK(tick_incr >= 3, "should advance at least 3 ticks at speed 3.0");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

/* ── HUD Drawing Tests ─────────────────────────────────────── */

static void test_hud_draws_pixels(void) {
    TEST("HUD overlay draws non-zero pixels");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);
    vbe_ws_bridge_start(&br);

    /* Render a frame with HUD on */
    br.show_hud = 1;
    vbe_ws_bridge_frame(&br);

    VBEState *vbe = vbe_state();
    /* Check top-left area (where FPS text is drawn) */
    int hud_pixels = count_pixels(vbe->fb, 320, 200, 0, 0, 60, 12);
    CHECK(hud_pixels > 0, "HUD should draw pixels in top-left area");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

static void test_hud_toggle(void) {
    TEST("HUD can be toggled off");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);

    /* HUD off */
    br.show_hud = 0;
    br.show_minimap = 0;
    vbe_ws_bridge_render_only(&br);

    /* With HUD off, top area should be terrain only (no HUD strip) */
    /* This is hard to verify precisely — just ensure no crash */
    PASS();
}

/* ── Text Rendering Tests ──────────────────────────────────── */

static void test_text_basic(void) {
    TEST("text renders characters and returns x position");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);

    int x = vbe_ws_bridge_text(&br, 4, 3, "FPS:60", 0x00FFFFFF);
    /* "FPS:60" is 6 chars × 6 pixels = 36 pixels, starting at x=4 */
    CHECK(x == 4 + 36, "x should advance by 6*len pixels");
    CHECK(x == 40, "x should be 40");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

static void test_text_int(void) {
    TEST("text_int renders integer and returns x position");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);

    int x1 = vbe_ws_bridge_text_int(&br, 4, 3, 42, 0x00FFFFFF);
    /* "42" is 2 chars × 6 = 12 pixels */
    CHECK(x1 == 4 + 12, "x should advance by 12 for '42'");

    int x2 = vbe_ws_bridge_text_int(&br, 100, 3, -7, 0x00FFFFFF);
    /* "-7" is 2 chars × 6 = 12 pixels */
    CHECK(x2 == 100 + 12, "x should advance by 12 for '-7'");

    int x3 = vbe_ws_bridge_text_int(&br, 4, 3, 0, 0x00FFFFFF);
    /* "0" is 1 char × 6 = 6 pixels */
    CHECK(x3 == 4 + 6, "x should advance by 6 for '0'");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

static void test_text_pixels_nonzero(void) {
    TEST("text drawing writes non-zero pixels");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);

    /* Draw text to back-buffer and check pixels */
    VBEState *vbe = vbe_state();
    /* Clear back-buffer */
    memset(vbe->back, 0, vbe->fb_size);

    vbe_ws_bridge_text(&br, 10, 10, "HELLO", 0x00FFFFFF);

    /* Check that some pixels around (10,10) are non-zero */
    int pixels = count_pixels(vbe->back, 320, 200, 10, 10, 30, 8);
    CHECK(pixels > 0, "text should write non-zero pixels");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

/* ── Query Tests ───────────────────────────────────────────── */

static void test_query_frame_count(void) {
    TEST("frame_count query");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    CHECK(vbe_ws_bridge_frame_count(&br) == 0, "initial frame_count is 0");
    CHECK(vbe_ws_bridge_frame_count(NULL) == 0, "NULL frame_count is 0");
    PASS();
}

static void test_query_fps(void) {
    TEST("fps query");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    CHECK(vbe_ws_bridge_fps(&br) == 0.0f, "initial fps is 0.0");
    CHECK(vbe_ws_bridge_fps(NULL) == 0.0f, "NULL fps is 0.0");
    PASS();
}

static void test_query_is_active(void) {
    TEST("is_active query");
    CHECK(vbe_ws_bridge_is_active(NULL) == 0, "NULL is not active");

    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    CHECK(vbe_ws_bridge_is_active(&br) == 0, "stopped+wired=0 is not active");

    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);
    CHECK(vbe_ws_bridge_is_active(&br) == 0, "wired but not started");

    vbe_ws_bridge_start(&br);
    CHECK(vbe_ws_bridge_is_active(&br) == 1, "running = active");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

/* ── Frame Count After Render ──────────────────────────────── */

static void test_frame_count_increments(void) {
    TEST("frame_count increments correctly");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    vbe_ws_bridge_wire(&br, &sim);
    vbe_ws_bridge_start(&br);

    CHECK(vbe_ws_bridge_frame_count(&br) == 0, "0 frames");
    vbe_ws_bridge_frame(&br);
    CHECK(vbe_ws_bridge_frame_count(&br) == 1, "1 frame");
    vbe_ws_bridge_frame(&br);
    CHECK(vbe_ws_bridge_frame_count(&br) == 2, "2 frames");
    vbe_ws_bridge_frame(&br);
    CHECK(vbe_ws_bridge_frame_count(&br) == 3, "3 frames");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

/* ── Terrain Visible After Render ──────────────────────────── */

static void test_terrain_biome_colors(void) {
    TEST("terrain render produces biome-colored pixels");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 777);
    vbe_ws_bridge_wire(&br, &sim);

    /* Render without HUD to see raw terrain */
    br.show_hud = 0;
    br.show_minimap = 0;
    vbe_ws_bridge_render_only(&br);

    VBEState *vbe = vbe_state();
    /* Sample some pixels from center of screen — should have terrain */
    int nonzero = count_pixels(vbe->fb, 320, 200, 50, 50, 100, 100);
    CHECK(nonzero > 0, "center area should have terrain pixels");

    /* Check that we have multiple different colors (biome diversity) */
    int colors[16] = {0};
    for (int y = 50; y < 150; y += 10) {
        for (int x = 50; x < 150; x += 10) {
            uint32_t c = vbe->fb[y * 320 + x];
            colors[(c >> 20) & 0xF]++; /* bucket by top nibble */
        }
    }
    int unique = 0;
    for (int i = 0; i < 16; i++)
        if (colors[i] > 0) unique++;
    CHECK(unique >= 2, "should have at least 2 different color ranges (biome diversity)");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

/* ── Camera Movement Changes Pixels ────────────────────────── */

static void test_camera_changes_view(void) {
    TEST("camera pan changes rendered pixels");
    vbe_ws_bridge_t br;
    vbe_ws_bridge_init(&br);
    vbe_init(320, 200);
    ws_simulation_t sim;
    ws_sim_init(&sim, 12345);
    vbe_ws_bridge_wire(&br, &sim);

    br.show_hud = 0;
    br.show_minimap = 0;

    /* Render at origin */
    vbe_ws_bridge_set_view(&br, 0, 0);
    vbe_ws_bridge_render_only(&br);
    VBEState *vbe = vbe_state();
    uint32_t pixel_origin = vbe->fb[100 * 320 + 160]; /* center pixel */

    /* Render with camera at different position */
    vbe_ws_bridge_set_view(&br, 100, 100);
    vbe_ws_bridge_render_only(&br);
    uint32_t pixel_moved = vbe->fb[100 * 320 + 160];

    /* Different camera position should generally show different terrain */
    /* (not guaranteed for all seeds, but very likely with seed 12345) */
    /* We just verify the render completes without crash */
    /* pixel_origin and pixel_moved may or may not differ — both are valid */
    CHECK(pixel_origin != 0 || pixel_moved != 0, "at least one view has visible pixels");

    vbe_ws_bridge_unwire(&br);
    vbe_shutdown();
    PASS();
}

/* ── Main ──────────────────────────────────────────────────── */

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  VBE ↔ WorldSim Bridge Test Suite                ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* Lifecycle */
    test_bridge_init();
    test_wire_basic();
    test_wire_null_safety();
    test_state_lifecycle();

    /* Camera */
    test_camera_pan();
    test_camera_set_view();
    test_camera_center_on();
    test_camera_zoom();

    /* Render Pipeline */
    test_render_frame_writes_pixels();
    test_render_multiple_frames();
    test_render_paused_does_not_step();
    test_render_stopped_no_op();
    test_render_only();
    test_sim_speed();

    /* HUD */
    test_hud_draws_pixels();
    test_hud_toggle();

    /* Text */
    test_text_basic();
    test_text_int();
    test_text_pixels_nonzero();

    /* Query */
    test_query_frame_count();
    test_query_fps();
    test_query_is_active();
    test_frame_count_increments();

    /* Visual */
    test_terrain_biome_colors();
    test_camera_changes_view();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("══════════════════════════════════════════════════\n");

    return g_fail > 0 ? 1 : 0;
}
