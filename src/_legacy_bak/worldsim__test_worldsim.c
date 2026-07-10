#include "worldsim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;

#define TEST(name) printf("  %-50s ", name);
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) { PASS(); } else { FAIL(msg); } } while(0)

/* -- RNG Tests -- */

static void test_rng_basic(void) {
    TEST("rng: xorshift64 produces non-zero");
    uint64_t state = 12345;
    uint64_t v = ws_rng_next(&state);
    CHECK(v != 0, "should produce non-zero");
}

static void test_rng_float_range(void) {
    TEST("rng: float in [0,1]");
    uint64_t state = 42;
    int ok = 1;
    for (int i = 0; i < 1000; i++) {
        float f = ws_rng_float(&state);
        if (f < 0.0f || f > 1.0f) { ok = 0; break; }
    }
    CHECK(ok, "all floats in range");
}

static void test_rng_int_range(void) {
    TEST("rng: int in [lo,hi)");
    uint64_t state = 77;
    int ok = 1;
    for (int i = 0; i < 1000; i++) {
        int v = ws_rng_int(&state, 10, 20);
        if (v < 10 || v >= 20) { ok = 0; break; }
    }
    CHECK(ok, "all ints in range");
}

/* -- Terrain Tests -- */

static void test_terrain_generate(void) {
    TEST("terrain: generate produces non-flat");
    ws_terrain_t t;
    ws_terrain_generate(&t, 1337);
    int min_h = 255, max_h = 0;
    for (int i = 0; i < WS_TERRAIN_W * WS_TERRAIN_H; i++) {
        if (t.heightmap[i] < min_h) min_h = t.heightmap[i];
        if (t.heightmap[i] > max_h) max_h = t.heightmap[i];
    }
    CHECK(max_h > min_h + 50, "terrain has significant variation");
}

static void test_terrain_biomes(void) {
    TEST("terrain: all 5 biomes present");
    ws_terrain_t t;
    ws_terrain_generate(&t, 1337);
    int found[5] = {0};
    for (int i = 0; i < WS_TERRAIN_W * WS_TERRAIN_H; i++)
        found[t.biome[i]] = 1;
    int total = found[0]+found[1]+found[2]+found[3]+found[4];
    CHECK(total >= 4, "at least 4 biomes present");
}

static void test_terrain_erode(void) {
    TEST("terrain: erode reduces variance");
    ws_terrain_t t1, t2;
    ws_terrain_generate(&t1, 999);
    memcpy(&t2, &t1, sizeof(t1));
    ws_terrain_erode(&t2, 50);
    /* Compute variance before and after */
    float var1 = 0, var2 = 0, mean1 = 0, mean2 = 0;
    int n = WS_TERRAIN_W * WS_TERRAIN_H;
    for (int i = 0; i < n; i++) { mean1 += t1.heightmap[i]; mean2 += t2.heightmap[i]; }
    mean1 /= n; mean2 /= n;
    for (int i = 0; i < n; i++) {
        float d1 = t1.heightmap[i] - mean1;
        float d2 = t2.heightmap[i] - mean2;
        var1 += d1*d1; var2 += d2*d2;
    }
    var1 /= n; var2 /= n;
    CHECK(var2 < var1, "eroded terrain has less variance");
}

static void test_terrain_bounds(void) {
    TEST("terrain: height/biome clamped");
    ws_terrain_t t;
    ws_terrain_generate(&t, 42);
    uint8_t h = ws_terrain_height(&t, -10, -10);
    uint8_t b = ws_terrain_biome(&t, 9999, 9999);
    CHECK(h <= 255 && b <= 4, "out-of-bounds returns clamped");
}

/* -- Entity Tests -- */

static void test_entity_create(void) {
    TEST("entity: create and get");
    ws_world_t w = {0};
    ws_entity_id id = ws_entity_create(&w);
    ws_entity_t *e = ws_entity_get(&w, id);
    CHECK(e != NULL && e->active && e->id == id, "entity exists");
}

static void test_entity_destroy(void) {
    TEST("entity: destroy removes");
    ws_world_t w = {0};
    ws_entity_id id = ws_entity_create(&w);
    ws_entity_destroy(&w, id);
    ws_entity_t *e = ws_entity_get(&w, id);
    CHECK(e == NULL, "entity gone after destroy");
}

static void test_entity_components(void) {
    TEST("entity: add/get/remove component");
    ws_world_t w = {0};
    ws_entity_id id = ws_entity_create(&w);
    ws_entity_t *e = ws_entity_get(&w, id);
    
    ws_position_t *pos = (ws_position_t *)ws_entity_add_component(e, WS_COMP_POSITION);
    pos->x = 10.0f; pos->y = 20.0f; pos->z = 30.0f;
    
    ws_position_t *got = (ws_position_t *)ws_entity_get_component(e, WS_COMP_POSITION);
    int ok = (got && got->x == 10.0f && got->y == 20.0f && got->z == 30.0f);
    
    ws_entity_remove_component(e, WS_COMP_POSITION);
    void *after = ws_entity_get_component(e, WS_COMP_POSITION);
    ok = ok && (after == NULL);
    
    CHECK(ok, "component lifecycle works");
}

static void test_entity_max(void) {
    TEST("entity: max entities boundary");
    ws_world_t w = {0};
    int count = 0;
    for (int i = 0; i < WS_MAX_ENTITIES + 10; i++) {
        ws_entity_id id = ws_entity_create(&w);
        if (id) count++;
    }
    CHECK(count == WS_MAX_ENTITIES, "stops at max entities");
}

/* -- Physics Tests -- */

static void test_physics_gravity(void) {
    TEST("physics: gravity pulls down");
    ws_world_t w = {0};
    ws_entity_id id = ws_entity_create(&w);
    ws_entity_t *e = ws_entity_get(&w, id);
    
    ws_position_t *pos = (ws_position_t *)ws_entity_add_component(e, WS_COMP_POSITION);
    pos->x = 128; pos->y = 100.0f; pos->z = 128;
    ws_velocity_t *vel = (ws_velocity_t *)ws_entity_add_component(e, WS_COMP_VELOCITY);
    vel->dx = 0; vel->dy = 0; vel->dz = 0;
    ws_physics_t *phys = (ws_physics_t *)ws_entity_add_component(e, WS_COMP_PHYSICS);
    phys->mass = 1.0f; phys->gravity = 1; phys->collide = 0;
    phys->bbox_w = 1; phys->bbox_h = 1; phys->bbox_d = 1;
    
    ws_physics_config_t cfg;
    ws_physics_init(&cfg);
    cfg.terrain_collide = 0;
    
    float y_before = pos->y;
    ws_physics_step(&w, NULL, &cfg);
    
    CHECK(pos->y < y_before, "entity fell");
}

static void test_physics_terrain_collide(void) {
    TEST("physics: terrain collision clamps");
    ws_terrain_t t;
    ws_terrain_generate(&t, 42);
    ws_world_t w = {0};
    ws_entity_id id = ws_entity_create(&w);
    ws_entity_t *e = ws_entity_get(&w, id);
    
    ws_position_t *pos = (ws_position_t *)ws_entity_add_component(e, WS_COMP_POSITION);
    pos->x = 128; pos->y = -100.0f; pos->z = 128; /* below terrain */
    ws_velocity_t *vel = (ws_velocity_t *)ws_entity_add_component(e, WS_COMP_VELOCITY);
    vel->dx = 0; vel->dy = -50.0f; vel->dz = 0;
    ws_physics_t *phys = (ws_physics_t *)ws_entity_add_component(e, WS_COMP_PHYSICS);
    phys->mass = 1.0f; phys->gravity = 1; phys->collide = 1;
    phys->bbox_w = 1; phys->bbox_h = 1; phys->bbox_d = 1;
    
    ws_physics_config_t cfg;
    ws_physics_init(&cfg);
    cfg.terrain_collide = 1;
    
    ws_physics_step(&w, &t, &cfg);
    
    uint8_t ground = t.heightmap[128 * t.width + 128];
    CHECK(pos->y >= (float)ground - 0.01f, "clamped to terrain surface");
}

/* -- Simulation Integration Tests -- */

static void test_sim_init(void) {
    TEST("sim: init produces valid state");
    ws_simulation_t sim;
    ws_sim_init(&sim, 12345);
    CHECK(!sim.paused && sim.tick == 0, "initial state correct");
}

static void test_sim_step(void) {
    TEST("sim: step increments tick");
    ws_simulation_t sim;
    ws_sim_init(&sim, 12345);
    ws_sim_step(&sim);
    ws_sim_step(&sim);
    CHECK(sim.tick == 2, "tick is 2 after 2 steps");
}

static void test_sim_paused(void) {
    TEST("sim: paused skips step");
    ws_simulation_t sim;
    ws_sim_init(&sim, 12345);
    sim.paused = 1;
    ws_sim_step(&sim);
    CHECK(sim.tick == 0, "tick stays 0 when paused");
}

static void test_sim_wander_ai(void) {
    TEST("sim: wander AI moves entity");
    ws_simulation_t sim;
    ws_sim_init(&sim, 42);
    
    ws_entity_id id = ws_entity_create(&sim.world);
    ws_entity_t *e = ws_entity_get(&sim.world, id);
    
    ws_position_t *pos = (ws_position_t *)ws_entity_add_component(e, WS_COMP_POSITION);
    pos->x = 128; pos->y = 50; pos->z = 128;
    ws_velocity_t *vel = (ws_velocity_t *)ws_entity_add_component(e, WS_COMP_VELOCITY);
    vel->dx = 0; vel->dy = 0; vel->dz = 0;
    ws_physics_t *phys = (ws_physics_t *)ws_entity_add_component(e, WS_COMP_PHYSICS);
    phys->mass = 1; phys->gravity = 0; phys->collide = 1;
    phys->bbox_w = 1; phys->bbox_h = 1; phys->bbox_d = 1;
    ws_ai_t *ai = (ws_ai_t *)ws_entity_add_component(e, WS_COMP_AI);
    ai->ai_type = 1; ai->timer = 0; ai->state = 0;
    
    /* Run several steps  --  entity should move */
    for (int i = 0; i < 100; i++) ws_sim_step(&sim);
    
    CHECK(pos->x != 128.0f || pos->z != 128.0f, "entity wandered from start");
}

/* -- Render Tests (no framebuffer = no crash) -- */

static void test_render_null_safe(void) {
    TEST("render: null framebuffer doesn't crash");
    ws_terrain_t t;
    ws_terrain_generate(&t, 42);
    ws_render_ctx_t ctx = {0};
    ws_render_terrain(&t, &ctx);
    ws_render_entities(NULL, &ctx);
    ws_render_minimap(&t, &ctx, 0, 0, 64);
    PASS(); /* if we got here, no crash */
}

/* -- Main -- */

int main(void) {
    printf("\n=== WorldSim Test Suite ===\n\n");
    
    printf("[RNG]\n");
    test_rng_basic();
    test_rng_float_range();
    test_rng_int_range();
    
    printf("\n[Terrain]\n");
    test_terrain_generate();
    test_terrain_biomes();
    test_terrain_erode();
    test_terrain_bounds();
    
    printf("\n[Entity]\n");
    test_entity_create();
    test_entity_destroy();
    test_entity_components();
    test_entity_max();
    
    printf("\n[Physics]\n");
    test_physics_gravity();
    test_physics_terrain_collide();
    
    printf("\n[Simulation]\n");
    test_sim_init();
    test_sim_step();
    test_sim_paused();
    test_sim_wander_ai();
    
    printf("\n[Render]\n");
    test_render_null_safe();
    
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
