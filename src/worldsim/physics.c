#include "worldsim.h"

void ws_physics_init(ws_physics_config_t *cfg) {
    cfg->gravity = -9.81f;
    cfg->drag = 0.01f;
    cfg->dt = 1.0f / 60.0f;
    cfg->terrain_collide = 1;
}

void ws_physics_step(ws_world_t *w, const ws_terrain_t *t, const ws_physics_config_t *cfg) {
    float dt = cfg->dt;
    
    for (int i = 0; i < WS_MAX_ENTITIES; i++) {
        ws_entity_t *e = &w->entities[i];
        if (!e->active) continue;
        if (!(e->component_mask & (1 << WS_COMP_POSITION))) continue;
        
        ws_position_t *pos = (ws_position_t *)&e->components[WS_COMP_POSITION];
        ws_velocity_t *vel = NULL;
        ws_physics_t *phys = NULL;
        
        if (e->component_mask & (1 << WS_COMP_VELOCITY))
            vel = (ws_velocity_t *)&e->components[WS_COMP_VELOCITY];
        if (e->component_mask & (1 << WS_COMP_PHYSICS))
            phys = (ws_physics_t *)&e->components[WS_COMP_PHYSICS];
        
        if (vel) {
            /* Apply gravity */
            if (phys && phys->gravity) {
                vel->dy += cfg->gravity * dt;
            }
            
            /* Apply drag */
            vel->dx *= (1.0f - cfg->drag);
            vel->dy *= (1.0f - cfg->drag);
            vel->dz *= (1.0f - cfg->drag);
            
            /* Integrate position */
            pos->x += vel->dx * dt;
            pos->y += vel->dy * dt;
            pos->z += vel->dz * dt;
        }
        
        /* Terrain collision  --  clamp to surface */
        if (cfg->terrain_collide && phys && phys->collide && t) {
            int tx = (int)pos->x;
            int ty = (int)pos->z; /* z maps to terrain Y */
            if (tx >= 0 && tx < t->width && ty >= 0 && ty < t->height) {
                float ground = (float)t->heightmap[ty * t->width + tx];
                if (pos->y < ground) {
                    pos->y = ground;
                    if (vel) vel->dy = 0.0f; /* stop vertical velocity */
                }
            }
        }
    }
}
