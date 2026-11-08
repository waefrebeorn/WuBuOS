/*
 * test_worldsim.c -- WorldSim engine behaviour gate.
 *
 * The module (633 LOC: terrain / ECS / physics / render / sim) had no test
 * file, so `make test_worldsim` could never build.  This is a real gate over
 * the public API in worldsim.h -- determinism, bounds, component lifecycle,
 * gravity + terrain clamping, and actual pixel output.
 *
 * C11, hosted.  Exit 0 = all gates passed.
 */
#include "worldsim.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  [PASS] %s\n", msg); } \
    else      { g_fail++; printf("  [FAIL] %s\n", msg); } \
} while (0)

/* -- RNG ------------------------------------------------------------ */

static void test_rng(void) {
    printf("\n[RNG]\n");
    uint64_t a = 12345, b = 12345;
    uint64_t r1 = ws_rng_next(&a);
    uint64_t r2 = ws_rng_next(&b);
    CHECK(r1 == r2, "xorshift64 is deterministic for the same seed");
    CHECK(a == b, "rng state advances identically");

    uint64_t s = 42;
    int in_range = 1;
    for (int i = 0; i < 2000; i++) {
        float f = ws_rng_float(&s);
        if (f < 0.0f || f >= 1.0f) { in_range = 0; break; }
    }
    CHECK(in_range, "ws_rng_float stays in [0,1) over 2000 draws");

    uint64_t t = 7;
    int lo_ok = 1;
    for (int i = 0; i < 2000; i++) {
        int v = ws_rng_int(&t, 3, 9);
        if (v < 3 || v >= 9) { lo_ok = 0; break; }
    }
    CHECK(lo_ok, "ws_rng_int stays in [lo,hi) over 2000 draws");
}

/* -- Terrain --------------------------------------------------------- */

static void test_terrain(void) {
    printf("\n[Terrain]\n");
    ws_terrain_t t1, t2, t3;
    ws_terrain_generate(&t1, 1234);
    ws_terrain_generate(&t2, 1234);
    ws_terrain_generate(&t3, 9999);

    CHECK(t1.width == WS_TERRAIN_W && t1.height == WS_TERRAIN_H,
          "generate sets width/height to the terrain constants");
    CHECK(t1.seed == 1234, "generate records the seed");
    CHECK(memcmp(t1.heightmap, t2.heightmap, sizeof(t1.heightmap)) == 0,
          "same seed -> identical heightmap (deterministic)");
    CHECK(memcmp(t1.heightmap, t3.heightmap, sizeof(t1.heightmap)) != 0,
          "different seed -> different heightmap");

    int biome_ok = 1;
    for (int i = 0; i < WS_TERRAIN_W * WS_TERRAIN_H; i++) {
        if (t1.heightmap[i] > 255) biome_ok = 0;         /* uint8, always true; keep explicit */
        if (t1.biome[i] > 4) biome_ok = 0;
    }
    CHECK(biome_ok, "all biome values are in 0..4");

    /* Height accessor clamps out-of-range coordinates. */
    CHECK(ws_terrain_height(&t1, -5, -5) == ws_terrain_height(&t1, 0, 0),
          "height() clamps negative coords to 0");
    CHECK(ws_terrain_height(&t1, 9999, 9999) ==
          ws_terrain_height(&t1, WS_TERRAIN_W - 1, WS_TERRAIN_H - 1),
          "height() clamps overflow coords to the last cell");
    CHECK(ws_terrain_biome(&t1, 99999, 99999) ==
          ws_terrain_biome(&t1, WS_TERRAIN_W - 1, WS_TERRAIN_H - 1),
          "biome() clamps overflow coords");

    /* Erosion is a smoothing operator: total absolute neighbour difference
     * must not increase, and the value range stays valid. */
    uint64_t before = 0, after = 0;
    for (int y = 1; y < WS_TERRAIN_H - 1; y++) {
        for (int x = 1; x < WS_TERRAIN_W - 1; x++) {
            int i = y * WS_TERRAIN_W + x;
            before += (int)abs((int)t1.heightmap[i] - (int)t1.heightmap[i - 1]);
            before += (int)abs((int)t1.heightmap[i] - (int)t1.heightmap[i - WS_TERRAIN_W]);
        }
    }
    ws_terrain_erode(&t1, 20);
    for (int y = 1; y < WS_TERRAIN_H - 1; y++) {
        for (int x = 1; x < WS_TERRAIN_W - 1; x++) {
            int i = y * WS_TERRAIN_W + x;
            after += (int)abs((int)t1.heightmap[i] - (int)t1.heightmap[i - 1]);
            after += (int)abs((int)t1.heightmap[i] - (int)t1.heightmap[i - WS_TERRAIN_W]);
        }
    }
    CHECK(after <= before, "erosion does not increase total slope (smoothing)");
}

/* -- Entities (ECS) -------------------------------------------------- */

static void test_entities(void) {
    printf("\n[Entities]\n");
    ws_world_t w;
    memset(&w, 0, sizeof(w));

    ws_entity_id id = ws_entity_create(&w);
    CHECK(id != 0, "create returns a non-zero id");
    CHECK(w.count == 1, "create increments world count");
    ws_entity_t *e = ws_entity_get(&w, id);
    CHECK(e != NULL && e->id == id && e->active, "get returns the created entity");

    ws_position_t *pos = ws_entity_add_component(e, WS_COMP_POSITION);
    CHECK(pos != NULL, "add_component returns storage");
    pos->x = 1.0f; pos->y = 2.0f; pos->z = 3.0f;
    ws_position_t *got = ws_entity_get_component(e, WS_COMP_POSITION);
    CHECK(got == pos && got->x == 1.0f && got->z == 3.0f,
          "get_component round-trips the stored position");
    CHECK(ws_entity_get_component(e, WS_COMP_VELOCITY) == NULL,
          "get_component on a missing component returns NULL");

    ws_entity_remove_component(e, WS_COMP_POSITION);
    CHECK(ws_entity_get_component(e, WS_COMP_POSITION) == NULL,
          "remove_component clears the component");

    ws_entity_destroy(&w, id);
    CHECK(w.count == 0, "destroy decrements world count");
    CHECK(ws_entity_get(&w, id) == NULL, "destroyed entity is no longer reachable");

    /* Fill the world: the allocator must refuse past WS_MAX_ENTITIES. */
    int created = 0;
    for (int i = 0; i < WS_MAX_ENTITIES; i++)
        if (ws_entity_create(&w) != 0) created++;
    CHECK(created == WS_MAX_ENTITIES, "allocator fills all 1024 slots");
    CHECK(ws_entity_create(&w) == 0, "allocator refuses past WS_MAX_ENTITIES");
}

/* -- Physics --------------------------------------------------------- */

static void test_physics(void) {
    printf("\n[Physics]\n");
    ws_physics_config_t cfg;
    ws_physics_init(&cfg);
    CHECK(cfg.gravity < 0.0f, "physics init sets a downward gravity");
    CHECK(cfg.dt > 0.0f, "physics init sets a positive timestep");
    CHECK(cfg.terrain_collide == 1, "physics init enables terrain collision");

    /* Gravity: a free body must fall. */
    ws_world_t w;
    memset(&w, 0, sizeof(w));
    ws_entity_id id = ws_entity_create(&w);
    ws_entity_t *e = ws_entity_get(&w, id);
    ws_entity_add_component(e, WS_COMP_POSITION);
    ws_velocity_t *vel = ws_entity_add_component(e, WS_COMP_VELOCITY);
    ws_physics_t *ph = ws_entity_add_component(e, WS_COMP_PHYSICS);
    ph->collide = 0;
    ph->gravity = 1;
    ws_position_t *pos = ws_entity_get_component(e, WS_COMP_POSITION);
    pos->x = 10.0f; pos->y = 100.0f; pos->z = 10.0f;

    ws_terrain_t t;
    ws_terrain_generate(&t, 1);
    ws_physics_step(&w, &t, &cfg);
    CHECK(pos->y < 100.0f, "gravity makes a free body fall (y decreases)");
    CHECK(vel->dy < 0.0f, "gravity gives the body downward velocity");

    /* Terrain clamp: a body below the surface is pushed up and stopped. */
    ws_world_t w2;
    memset(&w2, 0, sizeof(w2));
    ws_entity_id id2 = ws_entity_create(&w2);
    ws_entity_t *e2 = ws_entity_get(&w2, id2);
    ws_entity_add_component(e2, WS_COMP_POSITION);
    ws_velocity_t *v2 = ws_entity_add_component(e2, WS_COMP_VELOCITY);
    ws_physics_t *p2 = ws_entity_add_component(e2, WS_COMP_PHYSICS);
    p2->collide = 1; p2->gravity = 1;
    ws_position_t *q2 = ws_entity_get_component(e2, WS_COMP_POSITION);
    q2->x = 10.0f; q2->y = 0.0f; q2->z = 10.0f;
    v2->dy = -50.0f;

    float ground = (float)ws_terrain_height(&t, 10, 10);
    ws_physics_step(&w2, &t, &cfg);
    CHECK(q2->y >= ground, "terrain clamp lifts a body to the surface");
    CHECK(v2->dy == 0.0f, "terrain clamp zeroes vertical velocity");
}

/* -- Render ---------------------------------------------------------- */

static void test_render(void) {
    printf("\n[Render]\n");
    ws_terrain_t t;
    ws_terrain_generate(&t, 7);

    uint32_t fb[64 * 64];
    memset(fb, 0, sizeof(fb));
    ws_render_ctx_t ctx = { .fb = fb, .fb_w = 64, .fb_h = 64,
                            .cam_x = 0, .cam_y = 0, .cam_z = 1.0f };
    ws_render_terrain(&t, &ctx);
    int non_zero = 0;
    for (int i = 0; i < 64 * 64; i++) if (fb[i] != 0) non_zero++;
    CHECK(non_zero == 64 * 64, "terrain render fills the whole framebuffer");

    /* Entity without sprite pixels -> magenta placeholder rect. */
    ws_world_t w;
    memset(&w, 0, sizeof(w));
    ws_entity_id id = ws_entity_create(&w);
    ws_entity_t *e = ws_entity_get(&w, id);
    ws_position_t *pos = ws_entity_add_component(e, WS_COMP_POSITION);
    ws_sprite_t *spr = ws_entity_add_component(e, WS_COMP_SPRITE);
    pos->x = 5.0f; pos->y = 5.0f;
    spr->w = 4; spr->h = 4; spr->pixels = NULL;

    memset(fb, 0, sizeof(fb));
    ws_render_entities(&w, &ctx);
    int magenta = 0;
    for (int i = 0; i < 64 * 64; i++)
        if (fb[i] == 0xFF00FFFF) magenta++;
    CHECK(magenta == 16, "sprite-less entity draws a 4x4 magenta placeholder");

    /* Minimap draws its border. */
    memset(fb, 0, sizeof(fb));
    ws_render_minimap(&t, &ctx, 0, 0, 16);
    CHECK(fb[0] == 0xFFFFFFFF, "minimap draws a white border");
}

/* -- Simulation ------------------------------------------------------ */

static void test_sim(void) {
    printf("\n[Simulation]\n");
    ws_simulation_t sim;
    ws_sim_init(&sim, 2026);
    CHECK(sim.tick == 0, "sim init starts at tick 0");
    CHECK(sim.terrain.width == WS_TERRAIN_W, "sim init generates terrain");
    CHECK(sim.world.count == 0, "sim init starts with no entities");
    CHECK(sim.physics.dt > 0.0f, "sim init configures physics");

    ws_sim_step(&sim);
    ws_sim_step(&sim);
    CHECK(sim.tick == 2, "sim step advances the tick counter");

    uint64_t t_before = sim.tick;
    sim.paused = 1;
    ws_sim_step(&sim);
    CHECK(sim.tick == t_before, "paused sim does not advance");

    /* Render with a real framebuffer must not crash and must draw terrain. */
    uint32_t fb[128 * 128];
    memset(fb, 0, sizeof(fb));
    sim.render.fb = fb;
    sim.render.fb_w = 128; sim.render.fb_h = 128;
    sim.paused = 0;
    ws_sim_render(&sim);
    int non_zero = 0;
    for (int i = 0; i < 128 * 128; i++) if (fb[i] != 0) non_zero++;
    CHECK(non_zero > 0, "sim render draws into the framebuffer");
}

int main(void) {
    printf("=== WorldSim behaviour gate ===\n");
    test_rng();
    test_terrain();
    test_entities();
    test_physics();
    test_render();
    test_sim();
    printf("\n=== worldsim: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
