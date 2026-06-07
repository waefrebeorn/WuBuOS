#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include <stddef.h>

/*
 * WorldSim — Lightweight world simulation engine for My Seed
 * 
 * Layers:
 *   1. Terrain  — heightmap-based procedural terrain
 *   2. Entities — simple ECS (entity-component-system)
 *   3. Physics  — basic gravity, collision (AABB)
 *   4. Render   —Software rasterizer hooks for VBE framebuffer
 *
 * All C11, no external deps, <2000 LOC total.
 */

/* ── Terrain ── */

#define WS_TERRAIN_W 256
#define WS_TERRAIN_H 256
#define WS_HEIGHT_SCALE 255

typedef struct {
    uint8_t heightmap[WS_TERRAIN_W * WS_TERRAIN_H];
    uint8_t biome[WS_TERRAIN_W * WS_TERRAIN_H]; /* 0=water 1=plains 2=forest 3=mountain 4=snow */
    int width, height;
    uint32_t seed;
} ws_terrain_t;

void ws_terrain_generate(ws_terrain_t *t, uint32_t seed);
void ws_terrain_erode(ws_terrain_t *t, int iterations);
uint8_t ws_terrain_height(const ws_terrain_t *t, int x, int y);
uint8_t ws_terrain_biome(const ws_terrain_t *t, int x, int y);

/* ── Entities (ECS) ── */

#define WS_MAX_ENTITIES 1024
#define WS_MAX_COMPONENTS 16

typedef uint64_t ws_entity_id;

typedef enum {
    WS_COMP_POSITION  = 0,
    WS_COMP_VELOCITY  = 1,
    WS_COMP_SPRITE    = 2,
    WS_COMP_PHYSICS   = 3,
    WS_COMP_AI        = 4,
    WS_COMP_HEALTH    = 5,
    WS_COMP_INVENTORY = 6,
    WS_COMP_COUNT
} ws_component_type;

typedef struct {
    float x, y, z;
} ws_vec3_t;

typedef struct {
    float x, y, z; /* extends ws_vec3_t conceptually */
} ws_position_t;

typedef struct {
    float dx, dy, dz;
} ws_velocity_t;

typedef struct {
    uint32_t tex_id;
    int w, h;
    uint8_t *pixels; /* RGBA */
} ws_sprite_t;

typedef struct {
    float mass;
    float bbox_w, bbox_h, bbox_d; /* AABB half-extents */
    uint8_t collide : 1;
    uint8_t gravity : 1;
    uint8_t _pad : 6;
} ws_physics_t;

typedef struct {
    uint8_t ai_type; /* 0=none 1=wander 2=chase 3=flee */
    uint16_t state;
    float timer;
} ws_ai_t;

typedef struct {
    int16_t hp, max_hp;
} ws_health_t;

typedef struct {
    uint16_t items[32];
    uint8_t count;
} ws_inventory_t;

typedef union {
    ws_position_t  pos;
    ws_velocity_t  vel;
    ws_sprite_t    sprite;
    ws_physics_t   phys;
    ws_ai_t        ai;
    ws_health_t    health;
    ws_inventory_t inv;
    uint8_t        raw[64]; /* generic storage */
} ws_component_data_t;

typedef struct {
    ws_entity_id id;
    uint8_t component_mask; /* bitmask of which components are active */
    ws_component_data_t components[WS_COMP_COUNT];
    uint8_t active;
} ws_entity_t;

typedef struct {
    ws_entity_t entities[WS_MAX_ENTITIES];
    ws_entity_id next_id;
    int count;
} ws_world_t;

/* Entity API */
ws_entity_id ws_entity_create(ws_world_t *w);
void ws_entity_destroy(ws_world_t *w, ws_entity_id id);
ws_entity_t *ws_entity_get(ws_world_t *w, ws_entity_id id);
void *ws_entity_add_component(ws_entity_t *e, ws_component_type type);
void *ws_entity_get_component(ws_entity_t *e, ws_component_type type);
void ws_entity_remove_component(ws_entity_t *e, ws_component_type type);

/* ── Physics ── */

typedef struct {
    float gravity;    /* default -9.81 */
    float drag;       /* air resistance coefficient */
    float dt;         /* timestep */
    int terrain_collide; /* clamp entities to terrain surface */
} ws_physics_config_t;

void ws_physics_init(ws_physics_config_t *cfg);
void ws_physics_step(ws_world_t *w, const ws_terrain_t *t, const ws_physics_config_t *cfg);

/* ── Render ── */

typedef struct {
    uint32_t *fb;     /* framebuffer (XRGB8888) */
    int fb_w, fb_h;  /* framebuffer dimensions */
    int cam_x, cam_y; /* camera offset (top-left) */
    float cam_z;      /* zoom level */
} ws_render_ctx_t;

void ws_render_terrain(const ws_terrain_t *t, ws_render_ctx_t *ctx);
void ws_render_entities(const ws_world_t *w, ws_render_ctx_t *ctx);
void ws_render_minimap(const ws_terrain_t *t, ws_render_ctx_t *ctx, int mx, int my, int size);

/* ── Simulation ── */

typedef struct {
    ws_terrain_t terrain;
    ws_world_t   world;
    ws_physics_config_t physics;
    ws_render_ctx_t render;
    uint64_t tick;
    int paused;
} ws_simulation_t;

void ws_sim_init(ws_simulation_t *sim, uint32_t seed);
void ws_sim_step(ws_simulation_t *sim);
void ws_sim_render(ws_simulation_t *sim);

/* ── RNG (xorshift64) ── */
uint64_t ws_rng_next(uint64_t *state);
float ws_rng_float(uint64_t *state); /* 0.0..1.0 */
int ws_rng_int(uint64_t *state, int lo, int hi);

#endif /* WORLD_H */
