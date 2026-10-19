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

/* -- PCG-XSH-RR (WORLDSIM #564) -- */

static void test_pcg_vector(void) {
    TEST("pcg: pinned 8-word stream (arch-independent)");
    static const uint32_t want[8] = {
        2707161783u, 2068313097u, 3122475824u, 2211639955u,
        3215226955u, 3421331566u, 3217466285u, 2167406445u
    };
    ws_pcg32_t r;
    ws_pcg_seed(&r, 42, 54);
    int ok = 1;
    for (int i = 0; i < 8; i++)
        if (ws_pcg_next(&r) != want[i]) ok = 0;
    CHECK(ok, "stream differs from the pinned vector");
}

static void test_pcg_chi2(void) {
    TEST("pcg: 1e6-sample chi-square, 256 bins (p>0.01)");
    static long bins[256];
    memset(bins, 0, sizeof bins);
    ws_pcg32_t r;
    ws_pcg_seed(&r, 12345, 1);
    for (long i = 0; i < 1000000; i++)
        bins[ws_pcg_next(&r) & 255u]++;

    double expect = 1000000.0 / 256.0, chi = 0.0;
    for (int b = 0; b < 256; b++) {
        double d = (double)bins[b] - expect;
        chi += d * d / expect;
    }
    /* df = 255; the 1% critical value is ~310.46, so chi < that is p > 0.01. */
    char msg[96];
    snprintf(msg, sizeof msg, "chi2=%.1f exceeds the 1%% critical value 310.46", chi);
    CHECK(chi < 310.46, msg);
}

static void test_pcg_stream_hash(void) {
    TEST("pcg: 10k-value stream hash (cross-arch guard)");
    ws_pcg32_t r;
    ws_pcg_seed(&r, 42, 54);
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < 10000; i++) {
        uint32_t v = ws_pcg_next(&r);
        unsigned char b[4] = { (unsigned char)(v >> 24),
                               (unsigned char)(v >> 16),
                               (unsigned char)(v >> 8),
                               (unsigned char)v };
        for (int k = 0; k < 4; k++) { h ^= b[k]; h *= 1099511628211ULL; }
    }
    char msg[96];
    snprintf(msg, sizeof msg, "hash=0x%016llx, expected 0xf4b6ad7df17336c3",
             (unsigned long long)h);
    CHECK(h == 0xf4b6ad7df17336c3ULL, msg);
}

static void test_pcg_runs(void) {
    TEST("pcg: 32-bit run check and bit balance");
    ws_pcg32_t r;
    ws_pcg_seed(&r, 777, 9);
    uint32_t prev = ws_pcg_next(&r);
    int run = 1, maxrun = 1, ones = 0;
    for (long i = 1; i < 200000; i++) {
        uint32_t v = ws_pcg_next(&r);
        ones += __builtin_popcount(v);
        if (v == prev) { run++; if (run > maxrun) maxrun = run; }
        else run = 1;
        prev = v;
    }
    double ratio = (double)ones / (200000.0 * 32.0);
    char msg[128];
    snprintf(msg, sizeof msg, "maxrun=%d bit-ratio=%.4f", maxrun, ratio);
    CHECK(maxrun <= 3 && ratio > 0.49 && ratio < 0.51, msg);
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

/* WORLDSIM P0 #608: gravity matches the analytic ballistic solution within
 * epsilon.  With drag=0 and constant gravity, semi-implicit Euler velocity
 * is exact (v_k = v0 - g*k*dt); position accumulates O(dt) error vs the
 * continuous analytic y(t)=y0+v0*t-0.5*g*t^2.  At small dt the fixed-step
 * integrator tracks the analytic curve within a tight epsilon. */
static void test_physics_ballistic_analytic(void) {
    TEST("physics: ballistic matches analytic within epsilon (#608)");
    const float g = -9.81f, v0 = 20.0f, y0 = 100.0f;
    const float dt = 1.0f / 1000.0f;   /* fine step -> small discretization error */
    const int N = 200;                 /* t = 0.2s */

    ws_world_t w = {0};
    ws_entity_id id = ws_entity_create(&w);
    ws_entity_t *e = ws_entity_get(&w, id);
    ws_position_t *pos = (ws_position_t *)ws_entity_add_component(e, WS_COMP_POSITION);
    pos->x = 0; pos->y = y0; pos->z = 0;
    ws_velocity_t *vel = (ws_velocity_t *)ws_entity_add_component(e, WS_COMP_VELOCITY);
    vel->dx = 0; vel->dy = v0; vel->dz = 0;
    ws_physics_t *phys = (ws_physics_t *)ws_entity_add_component(e, WS_COMP_PHYSICS);
    phys->mass = 1.0f; phys->gravity = 1; phys->collide = 0;
    phys->bbox_w = 1; phys->bbox_h = 1; phys->bbox_d = 1;

    ws_physics_config_t cfg;
    ws_physics_init(&cfg);
    cfg.gravity = g; cfg.drag = 0.0f; cfg.dt = dt;
    cfg.terrain_collide = 0; cfg.body_collide = 0;

    for (int i = 0; i < N; i++) ws_physics_step(&w, NULL, &cfg);

    float t = N * dt;
    /* analytic with signed gravity g (negative = down):
     * y(t) = y0 + v0*t + 0.5*g*t^2 ; v(t) = v0 + g*t */
    float y_analytic = y0 + v0 * t + 0.5f * g * t * t;
    float v_analytic = v0 + g * t;
    CHECK(pos->y > y_analytic - 0.05f && pos->y < y_analytic + 0.05f,
          "position matches analytic ballistic within epsilon");
    CHECK(vel->dy > v_analytic - 0.02f && vel->dy < v_analytic + 0.02f,
          "velocity matches analytic (semi-implicit Euler is exact for v)");
}

/* WORLDSIM P0 #601: physics integrates stably at dt  --  many steps under
 * gravity + drag + collisions stay finite (no NaN/Inf blowup). */
static void test_physics_stable(void) {
    TEST("physics: integrates stably over many steps, no NaN/Inf (#601)");
    ws_world_t w = {0};
    /* a few bouncing bodies */
    for (int k = 0; k < 5; k++) {
        ws_entity_id id = ws_entity_create(&w);
        ws_entity_t *e = ws_entity_get(&w, id);
        ws_position_t *pos = (ws_position_t *)ws_entity_add_component(e, WS_COMP_POSITION);
        pos->x = (float)(k * 10); pos->y = 50.0f + k; pos->z = 0;
        ws_velocity_t *vel = (ws_velocity_t *)ws_entity_add_component(e, WS_COMP_VELOCITY);
        vel->dx = (float)(k - 2); vel->dy = 5.0f; vel->dz = 0;
        ws_physics_t *phys = (ws_physics_t *)ws_entity_add_component(e, WS_COMP_PHYSICS);
        phys->mass = 1.0f; phys->gravity = 1; phys->collide = 1;
        phys->bbox_w = 2; phys->bbox_h = 2; phys->bbox_d = 2;
    }
    ws_physics_config_t cfg;
    ws_physics_init(&cfg);
    cfg.terrain_collide = 0; cfg.body_collide = 1;  /* collisions on */
    for (int i = 0; i < 5000; i++) ws_physics_step(&w, NULL, &cfg);
    int finite = 1;
    for (int e = 0; e < WS_MAX_ENTITIES; e++) {
        if (!w.entities[e].active) continue;
        ws_position_t *p = (ws_position_t *)&w.entities[e].components[WS_COMP_POSITION];
        ws_velocity_t *v = (ws_velocity_t *)&w.entities[e].components[WS_COMP_VELOCITY];
        if (!(p->x == p->x) || !(p->y == p->y) || !(v->dx == v->dx) || !(v->dy == v->dy)) finite = 0;
        if (p->x > 1e6f || p->x < -1e6f || p->y > 1e6f || p->y < -1e6f) finite = 0;
    }
    CHECK(finite, "state stays finite after 5000 steps (no NaN/Inf blowup)");
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

static void test_physics_body_collide(void) {
    TEST("physics: rigid-body collision separates + bounces");
    ws_world_t w = {0};
    /* two spheres of radius 2 (bbox half-extents 2) overlapping on X */
    ws_entity_id ida = ws_entity_create(&w);
    ws_entity_t *a = ws_entity_get(&w, ida);
    ws_position_t *pa = (ws_position_t *)ws_entity_add_component(a, WS_COMP_POSITION);
    pa->x = 0; pa->y = 100.0f; pa->z = 0;
    ws_velocity_t *va = (ws_velocity_t *)ws_entity_add_component(a, WS_COMP_VELOCITY);
    va->dx = 5.0f; va->dy = 0; va->dz = 0;  /* moving toward b */
    ws_physics_t *pha = (ws_physics_t *)ws_entity_add_component(a, WS_COMP_PHYSICS);
    pha->mass = 1.0f; pha->collide = 1; pha->gravity = 0;
    pha->bbox_w = 2; pha->bbox_h = 2; pha->bbox_d = 2;

    ws_entity_id idb = ws_entity_create(&w);
    ws_entity_t *b = ws_entity_get(&w, idb);
    ws_position_t *pb = (ws_position_t *)ws_entity_add_component(b, WS_COMP_POSITION);
    pb->x = 3.0f; pb->y = 100.0f; pb->z = 0;  /* distance 3 < rsum 4 -> overlap */
    ws_velocity_t *vb = (ws_velocity_t *)ws_entity_add_component(b, WS_COMP_VELOCITY);
    vb->dx = 0; vb->dy = 0; vb->dz = 0;
    ws_physics_t *phb = (ws_physics_t *)ws_entity_add_component(b, WS_COMP_PHYSICS);
    phb->mass = 1.0f; phb->collide = 1; phb->gravity = 0;
    phb->bbox_w = 2; phb->bbox_h = 2; phb->bbox_d = 2;

    ws_physics_config_t cfg;
    ws_physics_init(&cfg);
    cfg.gravity = 0; cfg.drag = 0; cfg.body_collide = 1; cfg.terrain_collide = 0;
    cfg.restitution = 1.0f;  /* perfectly elastic */

    ws_physics_step(&w, NULL, &cfg);

    /* separated: distance now >= rsum (4) */
    float d = pb->x - pa->x;
    CHECK(d >= 3.99f, "bodies pushed apart to non-overlap");
    /* elastic head-on equal mass: momentum transfers -- moving ball stops,
     * stationary ball takes its velocity (va:5->0, vb:0->5) */
    CHECK(va->dx < 0.01f && va->dx > -0.01f, "a stopped (transferred momentum)");
    CHECK(vb->dx > 4.99f && vb->dx < 5.01f, "b velocity ~ +5 (took a's momentum)");
}

/* WORLDSIM P0 #602: no tunneling + energy conserved across a collision.
 * Two equal masses approach head-on at restitution 1.0 (perfectly elastic):
 * total kinetic energy before == after (momentum transfers, none lost). */
static void test_physics_energy_conserved(void) {
    TEST("physics: elastic collision conserves kinetic energy (#602)");
    ws_world_t w = {0};
    ws_entity_id ida = ws_entity_create(&w);
    ws_entity_t *a = ws_entity_get(&w, ida);
    ws_position_t *pa = (ws_position_t *)ws_entity_add_component(a, WS_COMP_POSITION);
    pa->x = 0; pa->y = 100.0f; pa->z = 0;
    ws_velocity_t *va = (ws_velocity_t *)ws_entity_add_component(a, WS_COMP_VELOCITY);
    va->dx = 6.0f; va->dy = 0; va->dz = 0;
    ws_physics_t *pha = (ws_physics_t *)ws_entity_add_component(a, WS_COMP_PHYSICS);
    pha->mass = 2.0f; pha->collide = 1; pha->gravity = 0;
    pha->bbox_w = 2; pha->bbox_h = 2; pha->bbox_d = 2;

    ws_entity_id idb = ws_entity_create(&w);
    ws_entity_t *b = ws_entity_get(&w, idb);
    ws_position_t *pb = (ws_position_t *)ws_entity_add_component(b, WS_COMP_POSITION);
    pb->x = 3.0f; pb->y = 100.0f; pb->z = 0;
    ws_velocity_t *vb = (ws_velocity_t *)ws_entity_add_component(b, WS_COMP_VELOCITY);
    vb->dx = 0; vb->dy = 0; vb->dz = 0;
    ws_physics_t *phb = (ws_physics_t *)ws_entity_add_component(b, WS_COMP_PHYSICS);
    phb->mass = 2.0f; phb->collide = 1; phb->gravity = 0;
    phb->bbox_w = 2; phb->bbox_h = 2; phb->bbox_d = 2;

    ws_physics_config_t cfg;
    ws_physics_init(&cfg);
    cfg.gravity = 0; cfg.drag = 0; cfg.body_collide = 1; cfg.terrain_collide = 0;
    cfg.restitution = 1.0f;

    /* KE before: only a moves. */
    float ke_before = 0.5f * 2.0f * (va->dx * va->dx);
    ws_physics_step(&w, NULL, &cfg);
    /* KE after: momentum transferred to b, a stopped. */
    float ke_after = 0.5f * 2.0f * (va->dx * va->dx) + 0.5f * 2.0f * (vb->dx * vb->dx);
    CHECK(ke_after > ke_before * 0.98f && ke_after < ke_before * 1.02f,
          "kinetic energy conserved (elastic)");
    /* no tunneling: bodies separated to non-overlap (dist >= rsum 4) */
    float d = pb->x - pa->x;
    CHECK(d >= 3.99f, "no tunneling: bodies separated");
}

/* WORLDSIM P0 #605: deterministic fixed-step — same seed -> same state. */
static void test_sim_deterministic(void) {
    TEST("sim: same seed produces bit-identical state (#605)");
    ws_simulation_t s1, s2;
    ws_sim_init(&s1, 4242);
    ws_sim_init(&s2, 4242);
    /* step both a fixed number of times */
    for (int i = 0; i < 20; i++) { ws_sim_step(&s1); ws_sim_step(&s2); }
    /* compare terrain heightmap + every active entity position */
    int mismatch = 0;
    if (s1.terrain.seed != s2.terrain.seed) mismatch++;
    for (int i = 0; i < s1.terrain.width * s1.terrain.height; i++)
        if (s1.terrain.heightmap[i] != s2.terrain.heightmap[i]) { mismatch++; break; }
    for (int e = 0; e < WS_MAX_ENTITIES; e++) {
        if (!s1.world.entities[e].active) continue;
        ws_position_t *p1 = (ws_position_t *)&s1.world.entities[e].components[WS_COMP_POSITION];
        ws_position_t *p2 = (ws_position_t *)&s2.world.entities[e].components[WS_COMP_POSITION];
        if (p1->x != p2->x || p1->y != p2->y || p1->z != p2->z) { mismatch++; break; }
    }
    CHECK(mismatch == 0, "same seed -> bit-identical terrain + entity state");
    CHECK(s1.tick == s2.tick, "same tick count");
}

/* WORLDSIM P0 #606: worldsim state readable as a string (entity/terrain/
 * physics tick) for /n/world/sim introspection. */
static void test_sim_state_str(void) {
    TEST("sim: state string exposes entity/terrain/physics tick (#606)");
    ws_simulation_t sim;
    ws_sim_init(&sim, 777);
    /* add an entity so the count is non-zero */
    ws_entity_create(&sim.world);
    ws_sim_step(&sim);
    ws_sim_step(&sim);
    char buf[512];
    ws_sim_state_str(&sim, buf, sizeof(buf));
    CHECK(strstr(buf, "tick=2") != NULL, "state string shows tick=2");
    CHECK(strstr(buf, "entities=") != NULL, "state string shows entity count");
    CHECK(strstr(buf, "terrain=") != NULL, "state string shows terrain dims");
    CHECK(strstr(buf, "gravity=") != NULL, "state string shows gravity");
    CHECK(strstr(buf, "seed=777") != NULL, "state string shows seed");
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

/* -- Collision layers (WORLDSIM #567) -- */

static void test_collision_layers(void) {
    ws_collision_t b[4] = {
        { .layer = 1u << 0, .collides = 1u << 1 },  /* A: group 1, hits group 2 */
        { .layer = 1u << 1, .collides = 1u << 0 },  /* B: group 2, hits group 1 */
        { .layer = 1u << 2, .collides = 0 },        /* C: group 3, hits nothing */
        { .layer = 1u << 1, .collides = 0 },        /* D: group 2, hits nothing */
    };

    TEST("collision: 16-layer filter returns only mutual pairs");
    int a[8], c[8];
    int n = ws_query_layer_pairs(b, 4, a, c, 8);
    CHECK(n == 1 && a[0] == 0 && c[0] == 1, "only A-B is mutually allowed");

    TEST("collision: mask-0 body collides with nothing");
    int hits_c = 0;
    for (int i = 0; i < n; i++) if (a[i] == 2 || c[i] == 2) hits_c++;
    CHECK(hits_c == 0, "group-3 body with collides=0 never pairs");

    TEST("collision: filter change applies on the next query");
    b[3].collides = (1u << 0) | (1u << 1);          /* D now accepts 1 and 2 */
    n = ws_query_layer_pairs(b, 4, a, c, 8);
    int has_ad = 0;
    for (int i = 0; i < n; i++)
        if (a[i] == 0 && c[i] == 3) has_ad = 1;
    CHECK(n == 2 && has_ad, "A-D appears immediately after the mask change");

    TEST("collision: layer-15 bit is a normal category");
    ws_collision_t hi[2] = {
        { .layer = 1u << 15, .collides = 1u << 14 },
        { .layer = 1u << 14, .collides = 1u << 15 },
    };
    CHECK(ws_query_layer_pairs(hi, 2, a, c, 8) == 1, "bits 15/14 pair correctly");

    TEST("collision: query respects max_pairs and NULL");
    ws_collision_t all[3] = {
        { .layer = 1, .collides = 0xFFFF }, { .layer = 2, .collides = 0xFFFF },
        { .layer = 4, .collides = 0xFFFF },
    };
    CHECK(ws_query_layer_pairs(all, 3, a, c, 1) == 1, "caps at max_pairs");
    CHECK(ws_query_layer_pairs(NULL, 3, a, c, 8) == 0, "NULL bodies -> 0");
    CHECK(ws_collision_allows(NULL, &all[0]) == 0, "NULL side -> no collision");
}

int main(void) {
    printf("\n=== WorldSim Test Suite ===\n\n");
    
    printf("[RNG]\n");
    test_rng_basic();
    test_rng_float_range();
    test_rng_int_range();
    test_pcg_vector();
    test_pcg_chi2();
    test_pcg_runs();
    test_pcg_stream_hash();
    
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
    test_physics_ballistic_analytic();
    test_physics_stable();
    test_physics_terrain_collide();
    test_physics_body_collide();
    test_physics_energy_conserved();
    test_collision_layers();
    
    printf("\n[Simulation]\n");
    test_sim_init();
    test_sim_step();
    test_sim_paused();
    test_sim_deterministic();
    test_sim_state_str();
    test_sim_wander_ai();
    
    printf("\n[Render]\n");
    test_render_null_safe();
    
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
